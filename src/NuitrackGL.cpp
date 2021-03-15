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
	_width(0),
	_height(0),
	_isInitialized(false)
{
	record.store(false);
	replay.store(false);
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

		_lines.clear();

		tdv::nuitrack::Nuitrack::waitUpdate(_skeletonTracker);
		if (isReplay)
			replayLoader.join();
		
		if (isReplay)
		{
			replayPointer++;
		}

		renderTexture();
		renderLinesUser();

		std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		if (record.load() && now > recordTill)
		{
			record.store(false);
			std::thread writerThread(&NuitrackGL::saveBufferToDisk, this, storePath);
			*recordingComplete = true;
			writerThread.detach();

		}
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

void NuitrackGL::clearBuffer(const std::string& path)
{
	writeJointDataBuffer.clear();
	loaded.store(false);
}


void NuitrackGL::loadDataToBuffer(const std::string& path)
{
	
	std::thread writerThread(&NuitrackGL::loadDiskToBuffer, this, path);
	writerThread.detach();
}

void::NuitrackGL::loadDiskToBuffer(const std::string & path)
{
	readJointDataBuffer.clear();
	DiskHelper::readDatafromDisk(path, readJointDataBuffer);
	loaded.store(true);
}

void NuitrackGL::saveBufferToDisk(std::string *path)
{
	jointDataBufferMutex.lock();

	DiskHelper::writeDataToDisk(path, writeJointDataBuffer);

	jointDataBufferMutex.unlock();
}

void NuitrackGL::playLoadedData()
{
	if (loaded.load())
	{
		replayPointer = 0;
		replay.store(true);
	}
}


void NuitrackGL::startRecording(const int &duration, std::string &path, bool &finishedRecording)
{
	std::cout << "Starting recording" << std::endl;
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	recordTill = now + duration;
	record.store(true);

	storePath = &path;
	recordingComplete = &finishedRecording;
	writeJointDataBuffer.clear();
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
		userAngles[17] = get3DAngleABC(joints, tdv::nuitrack::JOINT_LEFT_ELBOW, tdv::nuitrack::JOINT_LEFT_WRIST, tdv::nuitrack::JOINT_LEFT_HAND);
		userAngles[18] = get3DAngleABC(joints, tdv::nuitrack::JOINT_RIGHT_ELBOW, tdv::nuitrack::JOINT_RIGHT_WRIST, tdv::nuitrack::JOINT_RIGHT_HAND);
	}

	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	if (record.load())
	{
		if (jointDataBufferMutex.try_lock()) {
			JointFrame frame;

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
			jointDataBufferMutex.unlock();
		}
		else
		{
			std::cout << "Locking failed" << std::endl;
		}
	}
}

void NuitrackGL::updateTrainerSkeleton()
{
	const JointFrame& jf = readJointDataBuffer.at(replayPointer);

	drawBone(jf, tdv::nuitrack::JOINT_HEAD, tdv::nuitrack::JOINT_NECK);
	drawBone(jf, tdv::nuitrack::JOINT_NECK, tdv::nuitrack::JOINT_LEFT_COLLAR);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_RIGHT_SHOULDER);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_LEFT_SHOULDER);
	drawBone(jf, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_LEFT_HIP);
	drawBone(jf, tdv::nuitrack::JOINT_WAIST, tdv::nuitrack::JOINT_RIGHT_HIP);
	drawBone(jf, tdv::nuitrack::JOINT_TORSO, tdv::nuitrack::JOINT_WAIST);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_COLLAR, tdv::nuitrack::JOINT_TORSO);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_SHOULDER, tdv::nuitrack::JOINT_LEFT_ELBOW);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_ELBOW, tdv::nuitrack::JOINT_LEFT_WRIST);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_WRIST, tdv::nuitrack::JOINT_LEFT_HAND);
	drawBone(jf, tdv::nuitrack::JOINT_RIGHT_SHOULDER, tdv::nuitrack::JOINT_RIGHT_ELBOW);
	drawBone(jf, tdv::nuitrack::JOINT_RIGHT_ELBOW, tdv::nuitrack::JOINT_RIGHT_WRIST);
	drawBone(jf, tdv::nuitrack::JOINT_RIGHT_WRIST, tdv::nuitrack::JOINT_RIGHT_HAND);
	drawBone(jf, tdv::nuitrack::JOINT_RIGHT_HIP, tdv::nuitrack::JOINT_RIGHT_KNEE);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_HIP, tdv::nuitrack::JOINT_LEFT_KNEE);
	drawBone(jf, tdv::nuitrack::JOINT_RIGHT_KNEE, tdv::nuitrack::JOINT_RIGHT_ANKLE);
	drawBone(jf, tdv::nuitrack::JOINT_LEFT_KNEE, tdv::nuitrack::JOINT_LEFT_ANKLE);
}

bool NuitrackGL::drawBone(const JointFrame& j1, int index1, int index2)
{
	if (j1.confidence[index1] > 0.15 && j1.confidence[index2] > 0.15)
	{
		if (j1.relativeJoints[index1].x > 0 && j1.relativeJoints[index1].y > 0 && j1.relativeJoints[index2].x > 0 && j1.relativeJoints[index2].y > 0)
		{
			_lines.push_back(_width * j1.relativeJoints[index1].x);
			_lines.push_back(_height * j1.relativeJoints[index1].y);
			_lines.push_back(_width * j1.relativeJoints[index2].x);
			_lines.push_back(_height * j1.relativeJoints[index2].y);
			return true;
		}
		else
		{
			return false;
		}
	}
	else {
		return false;
	}
	
}

// Helper function to draw a skeleton bone
bool NuitrackGL::drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2)
{

	if (j1.confidence > 0.15 && j2.confidence > 0.15)
	{
		if (j1.proj.x > 0 && j1.proj.y > 0 && j2.proj.x > 0 && j2.proj.x > 0) 
		{
			_lines.push_back(_width * j1.proj.x);
			_lines.push_back(_height * j1.proj.y);
			_lines.push_back(_width * j2.proj.x);
			_lines.push_back(_height * j2.proj.y);
			return true;
		}
		else
		{
			return false;
		}
		
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

	glColor4f(1, 0, 0, 1);

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
