#define M_PI 3.14159265358979323846
#define CORRECTNESS_THRESHOLD_UPPER 40
#define CORRECTNESS_THRESHOLD_LOWER 30
#define CORRECTNESS_THRESHOLD_FULL 60

#include "NuitrackGL.h"

#include <string>
#include <iostream>
#include <fstream>
#include "UserInteraction.h"
#include "DiskHelper.h"
#include <chrono>

NuitrackGL::NuitrackGL() :
	_textureID(0),
	_textureBuffer(0),
	_width(0),
	_height(0),
	_isInitialized(false)
{
	record.store(false);
	loadedBufferReplay.store(false);
	recordingReplay.store(false);
	loaded.store(false);
}

NuitrackGL::~NuitrackGL()
{
	try
	{
		tdv::nuitrack::Nuitrack::release();
	}
	catch (const tdv::nuitrack::Exception& e)
	{
		// Do nothing
	}
}

std::string toString(tdv::nuitrack::device::ActivationStatus status)
{
	switch (status)
	{
		case tdv::nuitrack::device::ActivationStatus::NONE: return "None";
		case tdv::nuitrack::device::ActivationStatus::TRIAL: return "Trial";
		case tdv::nuitrack::device::ActivationStatus::PRO: return "Pro";
		default: return "Unknown type";
	}
}

void NuitrackGL::init(const std::string& config)
{
	try
	{
		tdv::nuitrack::Nuitrack::init(config);
		std::vector<tdv::nuitrack::device::NuitrackDevice::Ptr> devices = tdv::nuitrack::Nuitrack::getDeviceList();
		if (devices.empty())
			throw tdv::nuitrack::Exception("No devices found.");

		const auto& device = devices[0];
		std::string activationKey = "license:19372:Qep454hfDLfyNCYh";
		device->activate(activationKey);
	}
	catch (const tdv::nuitrack::Exception& e)
	{
		std::cerr << "Can not initialize Nuitrack (ExceptionType: " << e.type() << ")" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	// Create all required Nuitrack modules

	// colorsensor doesn not seem to work unless depth sensor is created
	_depthSensor = tdv::nuitrack::DepthSensor::create();
	_depthSensor->connectOnNewFrame(std::bind(&NuitrackGL::onNewDepthFrame, this, std::placeholders::_1));

	_colorSensor = tdv::nuitrack::ColorSensor::create();
	_colorSensor->connectOnNewFrame(std::bind(&NuitrackGL::onNewRGBFrame, this, std::placeholders::_1));

	_outputMode = _depthSensor->getOutputMode();
	tdv::nuitrack::OutputMode colorOutputMode = _colorSensor->getOutputMode();
	if (colorOutputMode.xres > _outputMode.xres)
		_outputMode.xres = colorOutputMode.xres;
	if (colorOutputMode.yres > _outputMode.yres)
		_outputMode.yres = colorOutputMode.yres;

	_width = _outputMode.xres;
	_height = _outputMode.yres;

	_userTracker = tdv::nuitrack::UserTracker::create();
	_userTracker->connectOnUpdate(std::bind(&NuitrackGL::onUserUpdateCallback, this, std::placeholders::_1));
	_userTracker->connectOnNewUser(std::bind(&NuitrackGL::onNewUserCallback, this, std::placeholders::_1));
	_userTracker->connectOnLostUser(std::bind(&NuitrackGL::onLostUserCallback, this, std::placeholders::_1));

	_skeletonTracker = tdv::nuitrack::SkeletonTracker::create();
	_skeletonTracker->connectOnUpdate(std::bind(&NuitrackGL::onSkeletonUpdate, this, std::placeholders::_1));

	_gestureRecognizer = tdv::nuitrack::GestureRecognizer::create();
	_gestureRecognizer->connectOnNewGestures(std::bind(&NuitrackGL::onNewGesture, this, std::placeholders::_1));

	_onIssuesUpdateHandler = tdv::nuitrack::Nuitrack::connectOnIssuesUpdate(std::bind(&NuitrackGL::onIssuesUpdate, this, std::placeholders::_1));
}

void NuitrackGL::displayVideo(bool display, bool joints, bool trainerJoints)
{
	showVideo = display;
	showJoints = joints;
	showJointsTrainer = trainerJoints;
}

void NuitrackGL::onUserUpdateCallback(tdv::nuitrack::UserFrame::Ptr frame) {

}

// Display information about gestures in the console
void NuitrackGL::onNewGesture(tdv::nuitrack::GestureData::Ptr gestureData)
{
	_userGestures = gestureData->getGestures();
	for (int i = 0; i < _userGestures.size(); ++i)
	{
		printf("Recognized %d from %d\n", _userGestures[i].type, _userGestures[i].userId);
	}
}

bool NuitrackGL::update(bool &userInFrame)
{
	if (!_isInitialized)
	{
		// Create texture by DepthSensor output mode
		initTexture(_width, _height);

		// When Nuitrack modules are created, we need to call Nuitrack::run() to start processing all modules
		try
		{
			tdv::nuitrack::Nuitrack::run();
		}
		catch (const tdv::nuitrack::Exception& e)
		{
			std::cerr << "Can not start Nuitrack (ExceptionType: " << e.type() << ")" << std::endl;
			release();
			exit(EXIT_FAILURE);
		}
		_isInitialized = true;
	}
	try
	{
		_lines.clear();
		_recordingLines.clear();
		std::thread replayLoader;

		bool isReplay = false;
		if (loadedBufferReplay.load())
		{
			if (replayPointer < readJointDataBuffer.size())
			{
				replayLoader = std::thread(&NuitrackGL::updateTrainerSkeleton, this);
				isReplay = true;
			}
			else 
			{
				*replayComplete = true;
				loadedBufferReplay.store(false);
			}
		}

		else if (recordingReplay.load())
		{
			if (recordingReplayPointer < writeJointDataBuffer.size())
			{
				replayLoader = std::thread(&NuitrackGL::updateTrainerSkeleton, this);
				isReplay = true;
			}
			else
			{
				*recordingReplayComplete = true;
				recordingReplay.store(false);
			}
		}

		else if (loadedBufferAnalysis.load())
		{
			if (analysisPointer < readJointDataBuffer.size())
			{
				replayLoader = std::thread(&NuitrackGL::updateTrainerSkeleton, this);
				isReplay = true;
			}
			else
			{
				*analysisComplete = true;
				loadedBufferAnalysis.store(false);

				float totalColouredFrames = redFrames + greenFrames + yellowFrames + orangeFrames;
				float percentRed = (((float)redFrames) / totalColouredFrames) * 100.0f;
				float percentYellow = (((float)yellowFrames) / totalColouredFrames) * 100.0f;
				float percentGreen = (((float)greenFrames) / totalColouredFrames) * 100.0f;
				float percentOrange = (((float)orangeFrames) / totalColouredFrames) * 100.0f;

				float totalFrames = checkingFrames + passFrames;
				int percentCheckingFrames = 100.0f * ((float)checkingFrames) / ((float)totalFrames);
				int percentPassFrames = 100.0f * ((float)passFrames) / ((float)totalFrames);

				float percentCorrectFrames = 100.0f * ((float)correctFrames + (float)passFrames) / ((float)totalFrames);
				float percentIncorrectFrames = 100.0f - percentCorrectFrames;

				std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

				std::string l0 = std::string("Total Duration - ") + std::to_string(now - startTime);
				std::string l1 = std::string("Total Frames - ") + std::to_string(totalFrames);
				std::string l2 = std::string("Percent Red Frames - ") + std::to_string(percentRed);
				std::string l3 = std::string("Percent Orange Frames - ") + std::to_string(percentOrange);
				std::string l4 = std::string("Percent Yellow Frames - ") + std::to_string(percentYellow);
				std::string l5 = std::string("Percent Green Frames - ") + std::to_string(percentGreen);
				std::string l6 = std::string("Percent Analysis Frames - ") + std::to_string(percentCheckingFrames);
				std::string l7 = std::string("Percent No-Analysis Frames - ") + std::to_string(percentPassFrames);
				std::string l8 = std::string("Percent Correct Frames - ") + std::to_string(percentCorrectFrames);
				std::string l9 = std::string("Percent Incorrect Frames - ") + std::to_string(percentIncorrectFrames);
				
				*analysisRef = l0 + "\n" + l1 + "\n" + l2 + "\n" + l3 + "\n" + l4 + "\n" + l5 + "\n" + l6 + "\n" + l7 + "\n" + l8 + "\n" + l9;
			}
		}

		hasAllJoints = false;
		float correctness = 0.0f;
		float threshold = 0.0f;
		int samplingRate = 30;

		tdv::nuitrack::Nuitrack::waitUpdate(_skeletonTracker);
		if (isReplay)
			replayLoader.join();

		if (isReplay)
		{
			if (loadedBufferReplay.load())
			{
				replayPointer++;
			}
			else if (recordingReplay.load())
			{
				recordingReplayPointer++;
			}
			else if (loadedBufferAnalysis.load())
			{
				if (analysisPointer % samplingRate == 0)
				{
					// Assume recording has all the required joints.
					// Check if user joints has all the required joints
					// If yes check angle
					checkingFrames += 1;

					if (hasAllJoints) {
						if (exerciseType == UPPER)
						{
							threshold = CORRECTNESS_THRESHOLD_UPPER;
							float angleCorrectness = 0.0f;
							for (int i = 7; i < 17; i++)
							{
								angleCorrectness += pow(userAngles[i] - readJointDataBuffer.at(analysisPointer).angles[i], 2);
							}
							angleCorrectness = sqrt(angleCorrectness);

							if (angleCorrectness < CORRECTNESS_THRESHOLD_UPPER)
							{
								analysisPointer++;
							}

							correctness = angleCorrectness;
						}
						else if (exerciseType == LOWER)
						{
							threshold = CORRECTNESS_THRESHOLD_LOWER;
							float angleCorrectness = 0;
							for (int i = 0; i < 7; i++)
							{
								angleCorrectness += pow(userAngles[i] - readJointDataBuffer.at(analysisPointer).angles[i], 2);
							}
							angleCorrectness = sqrt(angleCorrectness);

							if (angleCorrectness < CORRECTNESS_THRESHOLD_LOWER)
							{
								analysisPointer++;
							}
							correctness = angleCorrectness;
						}
						else if (exerciseType == FULL)
						{
							threshold = CORRECTNESS_THRESHOLD_FULL;
							float angleCorrectness = 0;
							for (int i = 0; i < 17; i++)
							{
								angleCorrectness += pow(userAngles[i] - readJointDataBuffer.at(analysisPointer).angles[i], 2);
							}
							angleCorrectness = sqrt(angleCorrectness);

							if (angleCorrectness < CORRECTNESS_THRESHOLD_FULL)
							{
								analysisPointer++;
							}
							correctness = angleCorrectness;
						}

						if (correctness < threshold)
						{
							correctFrames++;
						}
						else
						{
							incorrectFrames++;
						}
					}
					else
					{
						incorrectFrames += 1;
					}
				}
				else
				{
					passFrames += 1;
					analysisPointer++;
				}
			}
		}

		if (showVideo)
		{
			if (_lines.empty())
			{
				userInFrame = false;
			}
			else
			{
				userInFrame = true;
			}
		}
		else
		{
			userInFrame = true;
		} 

		Vector3 col;
		Vector3 colTrainer;

		colTrainer.x = 1.0f;
		colTrainer.y = 1.0f;
		colTrainer.z = 1.0f;

		col.x = 1.0f;
		col.y = 1.0f;
		col.y = 1.0f;

		renderTexture();
		
		if (isReplay)
		{
			renderLines(_recordingLines, 6, 16, 1.0f, 1.0f, 1.0f, showJointsTrainer);
			if (loadedBufferAnalysis.load())
			{
				if (hasAllJoints)
				{
					if (correctness == 0.0f)
					{
						greenFrames++;
						renderLines(_lines, 6, 16, 0.0f, 1.0f, 0.0f, showJoints);
					}
					else
					{
						if (correctness < threshold)
						{
							greenFrames++;
							renderLines(_lines, 6, 16, 0.0f, 1.0f, 0.0f, showJoints);
						}
						else if (correctness >= 40.0f && correctness < 1.175f * threshold)
						{
							yellowFrames++;
							renderLines(_lines, 6, 16, 1.0f, 1.0f, 0.0f, showJoints);
						}
						else if (correctness >= 1.175f * threshold && correctness < 1.325f * threshold)
						{
							orangeFrames++;
							renderLines(_lines, 6, 16, 1.0f, 0.5f, 0.0f, showJoints);
						}
						else if (correctness >= 1.325f * threshold)
						{
							redFrames++;
							renderLines(_lines, 6, 16, 1.0f, 0.0f, 0.0f, showJoints);
						}
					}
				}
				else
				{
					redFrames++;
					renderLines(_lines, 6, 16, 1.0f, 0.0f, 0.0f, showJoints);
				}
			}
			else
			{
				renderLines(_lines, 6, 16, 1.0f, 1.0f, 1.0f, showJoints);
			}
		}
		else
		{
			renderLines(_lines, 6, 16, 1.0f, 1.0f, 1.0f, showJoints);
		}

		std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		if (record.load() && now > recordTill)
		{
			record.store(false);
			saveBufferToDisk(storePath);
			*recordingComplete = true;
		}
	}
	catch (const tdv::nuitrack::LicenseNotAcquiredException& e)
	{
		userInFrame = true;
		// Update failed, negative result
		std::cerr << "LicenseNotAcquired exception (ExceptionType: " << e.type() << ")" << std::endl;
		return false;
	}
	catch (const tdv::nuitrack::Exception& e)
	{
		userInFrame = true;
		// Update failed, negative result
		std::cerr << "Nuitrack update failed (ExceptionType: " << e.type() << ")" << std::endl;
		return false;
	}
	
	return true;
}

void onNewDepthFrame(tdv::nuitrack::DepthFrame::Ptr frame) {

}

void NuitrackGL::release()
{
	if (_onIssuesUpdateHandler)
		tdv::nuitrack::Nuitrack::disconnectOnIssuesUpdate(_onIssuesUpdateHandler);

	// Release Nuitrack and remove all modules
	try
	{
		tdv::nuitrack::Nuitrack::release();
	}
	catch (const tdv::nuitrack::Exception& e)
	{
		std::cerr << "Nuitrack release failed (ExceptionType: " << e.type() << ")" << std::endl;
	}

	_isInitialized = false;
	// Free texture buffer
	if (_textureBuffer)
	{
		delete[] _textureBuffer;
		delete[] emptyTextureBuffer;
		_textureBuffer = 0;
	}
}

void NuitrackGL::clearBuffer(const std::string& path)
{
	writeJointDataBuffer.clear();
	loaded.store(false);
}

void::NuitrackGL::loadDiskToBuffer(const std::string & path)
{
	readJointDataBuffer.clear();
	DiskHelper::readDatafromDisk(path, readJointDataBuffer);
	loaded.store(true);
}

void NuitrackGL::saveBufferToDisk(std::string *path)
{
	DiskHelper::writeDataToDisk(path, writeJointDataBuffer);
}

void NuitrackGL::replayRecording(bool& complete)
{
	if (!record.load() && !loadedBufferReplay.load() && !loadedBufferAnalysis.load())
	{
		recordingReplayPointer = 0;
		recordingReplay.store(true);
		recordingReplayComplete = &complete;
	}
}

void NuitrackGL::analyzeLoadedData(bool &complete, std::string &analysis)
{
	if (loaded.load() && !record.load() && !loadedBufferReplay.load())
	{
		analysisPointer = 0;
		loadedBufferAnalysis.store(true);
		analysisComplete = &complete;
		analysisRef = &analysis;
		int type = readJointDataBuffer.at(0).requiredJoints;

		incorrectFrames = 0;
		checkingFrames = 0;
		correctFrames = 0;
		passFrames = 0;
		startTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		redFrames = 0;
		yellowFrames = 0;
		orangeFrames = 0;
		greenFrames = 0;

		if (type == 0)
		{
			exerciseType = UNDEFINED;
		}
		else if (type == 1)
		{
			exerciseType = UPPER;
		}
		else if (type == 2)
		{
			exerciseType = LOWER;
		}
		else if (type == 3)
		{
			exerciseType = FULL;
		}
	}
}

void NuitrackGL::replayLoadedData(bool& complete)
{
	if (loaded.load() && !record.load() && !loadedBufferAnalysis.load())
	{
		replayPointer = 0;
		loadedBufferReplay.store(true);
		replayComplete = &complete;
	}
}


void NuitrackGL::startRecording(const int &duration, std::string &path, bool &finishedRecording, int requiredJoints)
{
	std::cout << "Starting recording" << std::endl;
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	recordTill = now + duration;
	record.store(true);
	storePath = &path;
	recordingComplete = &finishedRecording;
	writeJointDataBuffer.clear();

	if (requiredJoints == -1) // get from read joint data buffer
	{
		requiredJoints = readJointDataBuffer.at(0).requiredJoints;
	}

	if (requiredJoints == 0)
	{
		exerciseType = UNDEFINED;
	}
	else if (requiredJoints == 1)
	{
		exerciseType = UPPER;
	}
	else if (requiredJoints == 2)
	{
		exerciseType = LOWER;
	}
	else if (requiredJoints == 3)
	{
		exerciseType = FULL;
	}
}

// Callback for onLostUser event
void NuitrackGL::onLostUserCallback(int id)
{
	std::cout << "Lost User " << id << std::endl;
}

// Callback for onNewUser event
void NuitrackGL::onNewUserCallback(int id)
{
	std::cout << "New User " << id << std::endl;
}

void NuitrackGL::onIssuesUpdate(tdv::nuitrack::IssuesData::Ptr issuesData)
{
	_issuesData = issuesData;
}

void NuitrackGL::onNewDepthFrame(tdv::nuitrack::DepthFrame::Ptr frame)
{
}

int NuitrackGL::get3DAngleABC(const std::vector<tdv::nuitrack::Joint>& joints, int a_index, int b_index, int c_index)
{
	const tdv::nuitrack::Joint& a = joints[a_index];
	const tdv::nuitrack::Joint& b = joints[b_index];
	const tdv::nuitrack::Joint& c = joints[c_index];

	float ab[3] = { b.real.x - a.real.x, b.real.y - a.real.y, b.real.z - a.real.z };
	float bc[3] = { c.real.x - b.real.x, c.real.y - b.real.y, c.real.z - b.real.z };

	float abVec = sqrt(ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2]);
	float bcVec = sqrt(bc[0] * bc[0] + bc[1] * bc[1] + bc[2] * bc[2]);

	float abNorm[3] = { ab[0] / abVec, ab[1] / abVec, ab[2] / abVec };
	float bcNorm[3] = { bc[0] / bcVec, bc[1] / bcVec, bc[2] / bcVec };

	float res = abNorm[0] * bcNorm[0] + abNorm[1] * bcNorm[1] + abNorm[2] * bcNorm[2];

	return static_cast<int>(acos(res) * 180.0f / 3.141592653589793f);
}

// Copy color frame data, received from Nuitrack, to texture to visualize
void NuitrackGL::onNewRGBFrame(tdv::nuitrack::RGBFrame::Ptr frame)
{
	uint8_t* texturePtr = _textureBuffer;
	const tdv::nuitrack::Color3* colorPtr = frame->getData();

	float wStep = (float)_width / frame->getCols();
	float hStep = (float)_height / frame->getRows();

	float nextVerticalBorder = hStep;

	for (size_t i = 0; i < _height; ++i)
	{
		if (i == (int)nextVerticalBorder)
		{
			nextVerticalBorder += hStep;
			colorPtr += frame->getCols();
		}

		int col = 0;
		float nextHorizontalBorder = wStep;

		for (size_t j = 0; j < _width; ++j, texturePtr += 3)
		{
			if (j == (int)nextHorizontalBorder)
			{
				++col;
				nextHorizontalBorder += wStep;
			}

			texturePtr[0] = (colorPtr + col)->red;
			texturePtr[1] = (colorPtr + col)->green;
			texturePtr[2] = (colorPtr + col)->blue;
		}
	}
}

// Prepare visualization of skeletons, received from Nuitrack
void NuitrackGL::onSkeletonUpdate(tdv::nuitrack::SkeletonData::Ptr userSkeletons)
{
	auto skeletons = userSkeletons->getSkeletons();

	for (tdv::nuitrack::Skeleton skeleton: skeletons)
	{
		drawSkeleton(skeleton.joints);
	}
}

// Helper function to draw skeleton from Nuitrack data
void NuitrackGL::drawSkeleton(const std::vector<tdv::nuitrack::Joint>& joints)
{
	hasAllJoints = true;

	if (exerciseType == UNDEFINED) // Any
	{
		hasAllJoints = false;
	}
	else  if (exerciseType == UPPER) // Upper body
	{
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_HEAD])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_NECK])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_COLLAR])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_SHOULDER])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_ELBOW])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_ELBOW])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_WRIST])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_WRIST])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_TORSO])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_WAIST])) hasAllJoints = false;
	}
	else if (exerciseType == LOWER) // Lower body
	{
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_TORSO])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_WAIST])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_HIP])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_HIP])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_KNEE])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_KNEE])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_ANKLE])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_ANKLE])) hasAllJoints = false;
	}
	else if (exerciseType == FULL) // Full body
	{
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_HEAD])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_NECK])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_COLLAR])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_SHOULDER])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_ELBOW])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_ELBOW])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_WRIST])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_WRIST])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_TORSO])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_WAIST])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_HIP])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_HIP])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_KNEE])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_KNEE])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_RIGHT_ANKLE])) hasAllJoints = false;
		if (!isValidJoint(joints[tdv::nuitrack::JOINT_LEFT_ANKLE])) hasAllJoints = false;
	}

	drawBone(joints[tdv::nuitrack::JOINT_HEAD], joints[tdv::nuitrack::JOINT_NECK]);
	drawBone(joints[tdv::nuitrack::JOINT_NECK], joints[tdv::nuitrack::JOINT_LEFT_COLLAR]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_COLLAR], joints[tdv::nuitrack::JOINT_TORSO]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_COLLAR], joints[tdv::nuitrack::JOINT_LEFT_SHOULDER]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_COLLAR], joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER]);
	drawBone(joints[tdv::nuitrack::JOINT_WAIST], joints[tdv::nuitrack::JOINT_LEFT_HIP]);
	drawBone(joints[tdv::nuitrack::JOINT_WAIST], joints[tdv::nuitrack::JOINT_RIGHT_HIP]);
	drawBone(joints[tdv::nuitrack::JOINT_TORSO], joints[tdv::nuitrack::JOINT_WAIST]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_SHOULDER], joints[tdv::nuitrack::JOINT_LEFT_ELBOW]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_ELBOW], joints[tdv::nuitrack::JOINT_LEFT_WRIST]);
	//drawBone(joints[tdv::nuitrack::JOINT_LEFT_WRIST], joints[tdv::nuitrack::JOINT_LEFT_HAND]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER], joints[tdv::nuitrack::JOINT_RIGHT_ELBOW]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_ELBOW], joints[tdv::nuitrack::JOINT_RIGHT_WRIST]);
	//drawBone(joints[tdv::nuitrack::JOINT_RIGHT_WRIST], joints[tdv::nuitrack::JOINT_RIGHT_HAND]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_HIP], joints[tdv::nuitrack::JOINT_RIGHT_KNEE]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_HIP], joints[tdv::nuitrack::JOINT_LEFT_KNEE]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_KNEE], joints[tdv::nuitrack::JOINT_RIGHT_ANKLE]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_KNEE], joints[tdv::nuitrack::JOINT_LEFT_ANKLE]);
		
	if (hasAllJoints) {
		userAngles[0] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_ANKLE, tdv::nuitrack::JOINT_LEFT_KNEE, tdv::nuitrack::JOINT_LEFT_HIP);
		userAngles[1] = get3DAngleABC(joints, tdv::nuitrack::JOINT_RIGHT_ANKLE, tdv::nuitrack::JOINT_RIGHT_KNEE, tdv::nuitrack::JOINT_RIGHT_HIP);
		userAngles[2] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_HIP, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_RIGHT_HIP);
		userAngles[3] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_KNEE, tdv::nuitrack::JOINT_LEFT_HIP, tdv::nuitrack::JOINT_WAIST);
		userAngles[4] = get3DAngleABC(joints, tdv::nuitrack::JOINT_RIGHT_KNEE, tdv::nuitrack::JOINT_RIGHT_HIP, tdv::nuitrack::JOINT_WAIST);
		userAngles[5] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_HIP, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_TORSO);
		userAngles[6] = get3DAngleABC(joints, tdv::nuitrack::JOINT_RIGHT_HIP, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_TORSO);
		userAngles[7] = get3DAngleABC(joints, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_TORSO, tdv::nuitrack::JOINT_LEFT_COLLAR); // Joint left collar same as joint right collar
		userAngles[8] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_SHOULDER, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_TORSO);
		userAngles[9] = get3DAngleABC(joints, tdv::nuitrack::JOINT_RIGHT_SHOULDER, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_TORSO);
		userAngles[10] = get3DAngleABC(joints, tdv::nuitrack::JOINT_NECK, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_LEFT_SHOULDER);
		userAngles[11] = get3DAngleABC(joints, tdv::nuitrack::JOINT_NECK, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_RIGHT_SHOULDER);
		userAngles[12] = get3DAngleABC(joints, tdv::nuitrack::JOINT_HEAD, tdv::nuitrack::JOINT_NECK, tdv::nuitrack::JOINT_LEFT_COLLAR);
		userAngles[13] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_LEFT_SHOULDER, tdv::nuitrack::JOINT_LEFT_ELBOW);
		userAngles[14] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_RIGHT_SHOULDER, tdv::nuitrack::JOINT_RIGHT_ELBOW);
		userAngles[15] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_SHOULDER, tdv::nuitrack::JOINT_LEFT_ELBOW, tdv::nuitrack::JOINT_LEFT_WRIST);
		userAngles[16] = get3DAngleABC(joints, tdv::nuitrack::JOINT_RIGHT_SHOULDER, tdv::nuitrack::JOINT_RIGHT_ELBOW, tdv::nuitrack::JOINT_RIGHT_WRIST);
	}
	

	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	if (record.load() && hasAllJoints) // Only recording if all the required joints are present
	{
		JointFrame frame;

		if (exerciseType == UPPER)
		{
			frame.requiredJoints = 1;
		}
		else if (exerciseType == LOWER) 
		{
			frame.requiredJoints = 2;
		}
		else if (exerciseType == FULL)
		{
			frame.requiredJoints = 3;
		}
		else if (exerciseType == UNDEFINED)
		{
			frame.requiredJoints = 0;
		}
		

		for (int i = 0; i < 25; i++)
		{
			frame.confidence[i] = joints[i].confidence;
			frame.realJoints[i].x = joints[i].real.x;
			frame.realJoints[i].y = joints[i].real.y;
			frame.realJoints[i].z = joints[i].real.z;
			frame.relativeJoints[i].x = joints[i].proj.x;
			frame.relativeJoints[i].y = joints[i].proj.y;
			frame.relativeJoints[i].z = joints[i].proj.z;
		}

		for (int i = 0; i < 19; i++) {
			frame.angles[i] = userAngles[i];
		}

		frame.timeStamp = time;
		writeJointDataBuffer.push_back(frame);
	}
}

void NuitrackGL::updateTrainerSkeleton()
{
	JointFrame* jf = 0;
	if (loadedBufferReplay.load())
	{
		jf = &readJointDataBuffer.at(replayPointer);
	}
	else if (loadedBufferAnalysis.load())
	{
		jf = &readJointDataBuffer.at(analysisPointer);
	}
	else if (recordingReplay.load())
	{
		jf = &writeJointDataBuffer.at(recordingReplayPointer);
	}
	else
	{
		return;
	}
	
	drawBone(*jf, tdv::nuitrack::JOINT_HEAD, tdv::nuitrack::JOINT_NECK);
	drawBone(*jf, tdv::nuitrack::JOINT_NECK, tdv::nuitrack::JOINT_LEFT_COLLAR);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_RIGHT_SHOULDER);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_LEFT_SHOULDER);
	drawBone(*jf, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_LEFT_HIP);
	drawBone(*jf, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_RIGHT_HIP);
	drawBone(*jf, tdv::nuitrack::JOINT_TORSO, tdv::nuitrack::JOINT_WAIST);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_TORSO);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_SHOULDER, tdv::nuitrack::JOINT_LEFT_ELBOW);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_ELBOW, tdv::nuitrack::JOINT_LEFT_WRIST);
	//drawBone(*jf, tdv::nuitrack::JOINT_LEFT_WRIST, tdv::nuitrack::JOINT_LEFT_HAND);
	drawBone(*jf, tdv::nuitrack::JOINT_RIGHT_SHOULDER, tdv::nuitrack::JOINT_RIGHT_ELBOW);
	drawBone(*jf, tdv::nuitrack::JOINT_RIGHT_ELBOW, tdv::nuitrack::JOINT_RIGHT_WRIST);
	//drawBone(*jf, tdv::nuitrack::JOINT_RIGHT_WRIST, tdv::nuitrack::JOINT_RIGHT_HAND);
	drawBone(*jf, tdv::nuitrack::JOINT_RIGHT_HIP, tdv::nuitrack::JOINT_RIGHT_KNEE);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_HIP, tdv::nuitrack::JOINT_LEFT_KNEE);
	drawBone(*jf, tdv::nuitrack::JOINT_RIGHT_KNEE, tdv::nuitrack::JOINT_RIGHT_ANKLE);
	drawBone(*jf, tdv::nuitrack::JOINT_LEFT_KNEE, tdv::nuitrack::JOINT_LEFT_ANKLE);
}

bool NuitrackGL::isValidJoint(const tdv::nuitrack::Joint& j1)
{
	if (j1.confidence > 0.15 && j1.proj.x > 0 && j1.proj.y > 0 && j1.proj.x < 1 && j1.proj.y < 1)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void NuitrackGL::drawBone(const JointFrame& j1, int index1, int index2)
{
	if (j1.confidence[index1] > 0.15 && j1.confidence[index2] > 0.15)
	{
		if (j1.relativeJoints[index1].x > 0 && j1.relativeJoints[index1].y > 0 && j1.relativeJoints[index2].x > 0 && j1.relativeJoints[index2].y > 0
			&& j1.relativeJoints[index1].x < 1 && j1.relativeJoints[index1].y < 1 && j1.relativeJoints[index2].x < 1 && j1.relativeJoints[index2].y < 1)
		{
			_recordingLines.push_back(_width * j1.relativeJoints[index1].x);
			_recordingLines.push_back(_height * j1.relativeJoints[index1].y);
			_recordingLines.push_back(_width * j1.relativeJoints[index2].x);
			_recordingLines.push_back(_height * j1.relativeJoints[index2].y);
		}
	}
}

// Helper function to draw a skeleton bone
void NuitrackGL::drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2)
{

	if (j1.confidence > 0.15 && j2.confidence > 0.15)
	{
		if (j1.proj.x > 0 && j1.proj.y > 0 && j2.proj.x > 0 && j2.proj.y > 0 && j1.proj.x < 1 && j1.proj.y < 1 && j2.proj.x < 1 && j2.proj.y < 1)
		{
			_lines.push_back(_width * j1.proj.x);
			_lines.push_back(_height * j1.proj.y);
			_lines.push_back(_width * j2.proj.x);
			_lines.push_back(_height * j2.proj.y);
		}
	}
}

// Render prepared background texture
void NuitrackGL::renderTexture()
{
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_TEXTURE_2D);
	glColor4f(1, 1, 1, 1);

	glBindTexture(GL_TEXTURE_2D, _textureID);

	if (showVideo) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, _textureBuffer);
	}
	else
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, emptyTextureBuffer);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, 0, _vertexes);
	glTexCoordPointer(2, GL_FLOAT, 0, _textureCoords);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_TEXTURE_2D);
}

void NuitrackGL::renderLines(std::vector<GLfloat> &lines, int lineWidth, int pointSize, float r, float g, float b, bool show)
{
	if (show)
	{
		if (lines.empty())
			return;

		glEnableClientState(GL_VERTEX_ARRAY);

		glColor4f(r, g, b, 1.0f);
		glLineWidth(lineWidth);

		glVertexPointer(2, GL_FLOAT, 0, lines.data());
		glDrawArrays(GL_LINES, 0, lines.size() / 2);

		glLineWidth(1);

		glEnable(GL_POINT_SMOOTH);
		glPointSize(pointSize);

		glVertexPointer(2, GL_FLOAT, 0, lines.data());
		glDrawArrays(GL_POINTS, 0, lines.size() / 2);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glPointSize(1);
		glDisable(GL_POINT_SMOOTH);

		glDisableClientState(GL_VERTEX_ARRAY);
	}
}

int NuitrackGL::power2(int n)
{
	unsigned int m = 2;
	while (m < n)
		m <<= 1;

	return m;
}

void NuitrackGL::initTexture(int width, int height)
{
	glGenTextures(1, &_textureID);

	width = power2(width);
	height = power2(height);

	if (_textureBuffer != 0)
		delete[] _textureBuffer;

	if (emptyTextureBuffer != 0)
		delete[] _textureBuffer;

	_textureBuffer = new uint8_t[width * height * 3];
	emptyTextureBuffer = new uint8_t[width * height * 3];
	memset(_textureBuffer, 0, sizeof(uint8_t) * width * height * 3);
	memset(emptyTextureBuffer, 0, sizeof(uint8_t) * width * height * 3);

	glBindTexture(GL_TEXTURE_2D, _textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Set texture coordinates [0, 1] and vertexes position
	{
		_textureCoords[0] = (float)_width / width;
		_textureCoords[1] = (float)_height / height;
		_textureCoords[2] = (float)_width / width;
		_textureCoords[3] = 0.0;
		_textureCoords[4] = 0.0;
		_textureCoords[5] = 0.0;
		_textureCoords[6] = 0.0;
		_textureCoords[7] = (float)_height / height;

		_vertexes[0] = _width;
		_vertexes[1] = _height;
		_vertexes[2] = _width;
		_vertexes[3] = 0.0;
		_vertexes[4] = 0.0;
		_vertexes[5] = 0.0;
		_vertexes[6] = 0.0;
		_vertexes[7] = _height;
	}
}
