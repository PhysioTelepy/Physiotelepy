#define M_PI 3.14159265358979323846
#define CORRECTNESS_THRESHOLD 80

#include "NuitrackGL.h"

#include <string>
#include <iostream>
#include <fstream>
#include "UserInteraction.h"
#include "DiskHelper.h"

NuitrackGL::NuitrackGL() :
	_textureID(0),
	_textureBuffer(0),
	_width(640),
	_height(480),
	_isInitialized(false)
{
	record.store(false);
	replay.store(false);
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

		std::cout << std::endl << "Available devices:" << std::endl;
		for (int i = 0; i < devices.size(); i++)
		{
			printf("    [%d] %s (%s), License: %s\n",
				i,
				devices[i]->getInfo(tdv::nuitrack::device::DeviceInfoType::SERIAL_NUMBER).c_str(),
				devices[i]->getInfo(tdv::nuitrack::device::DeviceInfoType::DEVICE_NAME).c_str(),
				toString(devices[i]->getActivationStatus()).c_str());
		}

		int devIndex;
		std::cout << std::endl << "Select the device number" << std::endl;
		std::cin >> devIndex;

		if (devIndex < 0 || devIndex >= devices.size())
			throw tdv::nuitrack::Exception("Invalid device index.");
		const auto& device = devices[devIndex];

		bool isActivated = device->getActivationStatus() == tdv::nuitrack::device::ActivationStatus::PRO;

		if (isActivated)
			isActivated = !UserInteraction::confirm("The device is already activated! Do you want to reactivate it?");
		else
			std::cout << "device isnt activated............." << std::endl;

		if (!isActivated)
		{
			std::string activationKey = "license:19372:Qep454hfDLfyNCYh";
			device->activate(activationKey);
		}
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

bool NuitrackGL::update(float* skeletonColor, float* jointColor, const float& pointSize, const float& lineWidth, const bool& overrideJointColour)
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
		std::thread replayLoader;
		bool isReplay = false;
		if (replay.load())
		{
			if (replayPointer < readJointDataBuffer.size())
			{
				isReplay = true;
				replayLoader = std::thread(&NuitrackGL::updateTrainerSkeleton, this);
			}
			else {
				replay.store(false);
			}
		}
		tdv::nuitrack::Nuitrack::waitUpdate(_skeletonTracker);
		if (isReplay)
			replayLoader.join();
		
		if (isReplay)
		{
			replayPointer++;
		}

		renderTexture();
		renderLinesUser();
		//renderLinesTrainer(skeletonColor, jointColor, pointSize, lineWidth, _lines2, numLines2, replay.load(), overrideJointColour);
	}
	catch (const tdv::nuitrack::LicenseNotAcquiredException& e)
	{
		// Update failed, negative result
		std::cerr << "LicenseNotAcquired exception (ExceptionType: " << e.type() << ")" << std::endl;
		return false;
	}
	catch (const tdv::nuitrack::Exception& e)
	{
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
		_textureBuffer = 0;
	}
}

void NuitrackGL::loadDataToBuffer(const std::string& path)
{
	DiskHelper::readDatafromDisk(path, readJointDataBuffer);
}

void NuitrackGL::saveBufferToDisk()
{
	jointDataBufferMutex.lock();

	DiskHelper::writeDataToDisk("C:/dev/JointData/", writeJointDataBuffer);

	jointDataBufferMutex.unlock();
}

void NuitrackGL::playLoadedData()
{
	replayPointer = 0;
	replay.store(true);
}


void NuitrackGL::startRecording(const int& duration)
{
	if (record.load() || saving.load())
	{
		std::cout << "A video is currently being recorded, please wait until it has finished." << std::endl;
	}
	else {
		std::cout << std::endl << "Starting video recording" << std::endl;
		num_frames_to_record = duration;
		writeJointDataBuffer.clear();
		record.store(true);
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
	_lines.clear();

	auto skeletons = userSkeletons->getSkeletons();


	for (tdv::nuitrack::Skeleton skeleton: skeletons)
	{
		drawSkeleton(skeleton.joints);
	}
}

// Helper function to draw skeleton from Nuitrack data
void NuitrackGL::drawSkeleton(const std::vector<tdv::nuitrack::Joint>& joints)
{
	bool hasJoints = true;

	// We need to draw a bone for every pair of neighbour joints
	if (!drawBone(joints[tdv::nuitrack::JOINT_HEAD], joints[tdv::nuitrack::JOINT_NECK]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_NECK], joints[tdv::nuitrack::JOINT_LEFT_COLLAR]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_COLLAR], joints[tdv::nuitrack::JOINT_TORSO]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_COLLAR], joints[tdv::nuitrack::JOINT_LEFT_SHOULDER]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_COLLAR], joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_WAIST], joints[tdv::nuitrack::JOINT_LEFT_HIP]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_WAIST], joints[tdv::nuitrack::JOINT_RIGHT_HIP]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_TORSO], joints[tdv::nuitrack::JOINT_WAIST]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_SHOULDER], joints[tdv::nuitrack::JOINT_LEFT_ELBOW]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_ELBOW], joints[tdv::nuitrack::JOINT_LEFT_WRIST]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_WRIST], joints[tdv::nuitrack::JOINT_LEFT_HAND]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER], joints[tdv::nuitrack::JOINT_RIGHT_ELBOW]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_RIGHT_ELBOW], joints[tdv::nuitrack::JOINT_RIGHT_WRIST]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_RIGHT_WRIST], joints[tdv::nuitrack::JOINT_RIGHT_HAND]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_RIGHT_HIP], joints[tdv::nuitrack::JOINT_RIGHT_KNEE]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_HIP], joints[tdv::nuitrack::JOINT_LEFT_KNEE]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_RIGHT_KNEE], joints[tdv::nuitrack::JOINT_RIGHT_ANKLE]))
		hasJoints = false;
	if (!drawBone(joints[tdv::nuitrack::JOINT_LEFT_KNEE], joints[tdv::nuitrack::JOINT_LEFT_ANKLE]))
		hasJoints = false;

	hasAllJoints = hasJoints;

	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	if (record.load())
	{
		if (writeJointDataBuffer.size() < num_frames_to_record) {

			if (jointDataBufferMutex.try_lock()) {
				JointFrame frame;

				for (int i = 0; i < 25; i++)
				{
					frame.confidence[i] = joints[i].confidence;
					frame.realJoints[i].x = joints[i].real.x;
					frame.realJoints[i].y = joints[i].real.y;
					frame.realJoints[i].z = joints[i].real.z;

				}

				frame.timeStamp = time;
				writeJointDataBuffer.push_back(frame);
				jointDataBufferMutex.unlock();
			}
			else
			{
				std::cout << "Locking failed" << std::endl;
			}
		}
		else 
		{
			record.store(false);
			std::thread writerThread(&NuitrackGL::saveBufferToDisk, this);
			writerThread.detach();

		}
	}
}

void NuitrackGL::updateTrainerSkeleton()
{
	
}

void NuitrackGL::drawBone(const JointFrame& j1, int index1, int index2)
{
	
}

// Helper function to draw a skeleton bone
bool NuitrackGL::drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2)
{

	if (j1.confidence > 0.15 && j2.confidence > 0.15)
	{
		_lines.push_back(_width * j1.proj.x);
		_lines.push_back(_height * j1.proj.y);
		_lines.push_back(_width * j2.proj.x);
		_lines.push_back(_height * j2.proj.y);
		return true;
	}
	else {
		return false;
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
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, _textureBuffer);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, 0, _vertexes);
	glTexCoordPointer(2, GL_FLOAT, 0, _textureCoords);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_TEXTURE_2D);
}

void NuitrackGL::renderLinesUser()
{
	if (_lines.empty())
		return;

	glEnableClientState(GL_VERTEX_ARRAY);

	glColor4f(1, 1, 1, 1);

	glLineWidth(6);

	glVertexPointer(2, GL_FLOAT, 0, _lines.data());
	glDrawArrays(GL_LINES, 0, _lines.size() / 2);

	glLineWidth(1);

	glEnable(GL_POINT_SMOOTH);
	glPointSize(16);

	glVertexPointer(2, GL_FLOAT, 0, _lines.data());
	glDrawArrays(GL_POINTS, 0, _lines.size() / 2);

	glColor4f(1, 1, 1, 1);
	glPointSize(1);
	glDisable(GL_POINT_SMOOTH);

	glDisableClientState(GL_VERTEX_ARRAY);
}

// Visualize bones, joints and hand positions
void NuitrackGL::renderLinesTrainer(const float *skeletonColor, const float* jointColor, const float& pointSize, const float& lineWidth, const float* lines, const int& size, bool render, const bool& overrideJointColour)
{
	
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

	_textureBuffer = new uint8_t[width * height * 3];
	memset(_textureBuffer, 0, sizeof(uint8_t) * width * height * 3);

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
