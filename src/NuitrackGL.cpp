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
	saving.store(false);
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

/*
* Shaders are written in GLSL. They are programs that run on the GPU.
* They are responsible for converting raw data (vertices, indices and textures) into coloured pixels.
* Read more about shaders and OpenGL pipeline at learnopengl.com
*/
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"out vec2 TexCoord;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 1.0);\n"
"   TexCoord = aTexCoord;\n"
"}\n\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D ourTexture;\n"
"void main()\n"
"{\n"
"   FragColor = texture(ourTexture, TexCoord);\n"
"}\n\0";

const char* vertexShaderSource2 = "#version 330 core\n"
"uniform float pointSize;\n"
"layout (location = 0) in vec2 aPos;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 1.0, 1.0);\n"
"	gl_PointSize = pointSize;\n"
"}\n\0";

const char* fragmentShaderSource2 = "#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec4 color;\n"
"void main()\n"
"{\n"
"   FragColor = color;\n"
"}\n\0";

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
	_colorSensor = tdv::nuitrack::ColorSensor::create();
	_colorSensor->connectOnNewFrame(std::bind(&NuitrackGL::onNewRGBFrame, this, std::placeholders::_1));

	_outputMode = _colorSensor->getOutputMode();
	_width = _outputMode.xres;
	_height = _outputMode.yres;

	_userTracker = tdv::nuitrack::UserTracker::create();
	_userTracker->connectOnNewUser(std::bind(&NuitrackGL::onNewUserCallback, this, std::placeholders::_1));
	_userTracker->connectOnLostUser(std::bind(&NuitrackGL::onLostUserCallback, this, std::placeholders::_1));

	_skeletonTracker = tdv::nuitrack::SkeletonTracker::create();
	_skeletonTracker->connectOnUpdate(std::bind(&NuitrackGL::onSkeletonUpdate, this, std::placeholders::_1));

	_onIssuesUpdateHandler = tdv::nuitrack::Nuitrack::connectOnIssuesUpdate(std::bind(&NuitrackGL::onIssuesUpdate, this, std::placeholders::_1));
}

bool NuitrackGL::update(float* skeletonColor, float* jointColor, const float& pointSize, const float& lineWidth, const bool& overrideJointColour)
{
	if (!_isInitialized)
	{
		// Create texture by DepthSensor output mode
		initTexture(_width, _height);
		initLines();

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
		bool join = false;
		if (replay.load())
		{
			if (replayPointer < readJointDataBuffer.size())
			{
				join = true;
				replayLoader = std::thread(&NuitrackGL::updateTrainerSkeleton, this);
			}
			else {
				replay.store(false);
			}
		}
		tdv::nuitrack::Nuitrack::waitUpdate(_skeletonTracker);
		if (join)
			replayLoader.join();
		// Set next frame here
		
		renderTexture();
		renderLinesUser(skeletonColor, jointColor, pointSize, lineWidth, _lines, numLines, true, overrideJointColour);
		renderLinesTrainer(skeletonColor, jointColor, pointSize, lineWidth, _lines2, numLines2, replay.load(), overrideJointColour);
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

	DiskHelper::writeDataToDisk("test.txt", writeJointDataBuffer);

	jointDataBufferMutex.unlock();
}

void NuitrackGL::playLoadedData()
{
	replayPointer = 0;
	replay.store(true);
}

void NuitrackGL::stopRecording()
{
	if (record.load() && !saving.load())
	{
		saving.store(true);
		record.store(false);
		// Start a thread that saves the data from the recorded buffer.
		std::cout << "Saving data to disk" << std::endl;
		std::thread writerThread (&NuitrackGL::saveBufferToDisk, this);
		writerThread.join();
		saving.store(false);
		std::cout << "Data saved to disk" << std::endl << std::endl;
	}
	else 
	{
		std::cout << "Invalid operation" << std::endl;
	}
}

void NuitrackGL::stopRecordingTimer(const int& duration)
{
	std::this_thread::sleep_for(std::chrono::seconds(duration));
	std::cout << "Stopping recording" << std::endl;
	stopRecording();
}

void NuitrackGL::startRecording(const int& duration)
{
	if (record.load() || saving.load())
	{
		std::cout << "A video is currently being recorded, please wait until it has finished." << std::endl;
	}
	else {
		std::cout << std::endl << "Starting video recording" << std::endl;
		writeJointDataBuffer.clear();
		record.store(true);
		std::thread timerThread(&NuitrackGL::stopRecordingTimer, this, duration);
		timerThread.detach();
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

// Copy color frame data, received from Nuitrack, to texture to visualize
void NuitrackGL::onNewRGBFrame(tdv::nuitrack::RGBFrame::Ptr frame)
{
	//std::thread::id this_id = std::this_thread::get_id();
	//std::cout << "RGB update thread: " << this_id << std::endl;

	// Storing from end of the buffer to start because frame data is received from top to bottom
	// OpenGl requires texture data from the bottom to top.
	// This will reverse the x-axis order of pixels too but it works out because image from a camera is mirrored.
	uint8_t* texturePtr = _textureBuffer + (3 * _width * _height) - 1;
	const tdv::nuitrack::Color3* colorPtr = frame->getData();

	float wStep = (float)_width / frame->getCols();
	float hStep = (float)_height / frame->getRows();

	//std::cout << "Output : " << frame->getCols() << std::endl << "Output rows: " << frame->getRows() << std::endl;

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

		for (size_t j = 0; j < _width; ++j, texturePtr -= 3)
		{
			if (j == (int)nextHorizontalBorder)
			{
				++col;
				nextHorizontalBorder += wStep;
			}

			*(texturePtr - 2) = (colorPtr + col)->red;
			*(texturePtr - 1) = (colorPtr + col)->green;
			*(texturePtr - 0) = (colorPtr + col)->blue;
		}
	}
}

// Prepare visualization of skeletons, received from Nuitrack
void NuitrackGL::onSkeletonUpdate(tdv::nuitrack::SkeletonData::Ptr userSkeletons)
{
	numLines = 0;

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

	std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	if (record.load() && !saving.load() && hasJoints)
	{
		if (jointDataBufferMutex.try_lock()) {
			JointFrame frame;

			for (int i = 0; i < 25; i++)
			{
				frame.joints[i].x = joints[i].proj.x;
				frame.joints[i].y = joints[i].proj.y;
				frame.confidence[i] = joints[i].confidence;
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

	hasAllJoints = hasJoints;
}

void NuitrackGL::updateTrainerSkeleton()
{
	numLines2 = 0;

	const JointFrame& jf = readJointDataBuffer.at(replayPointer);
	replayPointer += 1;

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

void NuitrackGL::drawBone(const JointFrame& j1, int index1, int index2)
{
	if (j1.confidence[index1] > 0.15 && j1.confidence[index2] > 0.15)
	{
		_lines2[numLines2] = (-j1.joints[index1].x * 2) + 1;
		_lines2[numLines2 + 1] = (-j1.joints[index1].y * 2) + 1;
		_lines2[numLines2 + 2] = (-j1.joints[index2].x * 2) + 1;
		_lines2[numLines2 + 3] = (-j1.joints[index2].y * 2) + 1;

		numLines2 += 4;
	}
}

// Helper function to draw a skeleton bone
bool NuitrackGL::drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2)
{
	// Prepare line data for confident enough bones only

	// Illustration of the projected coordinates w.r.t the window
	//////////////////
	//(1,0)	  (0, 0)//
	//				//
	//				//
	//(1, 1)  (0, 1)//
	//////////////////

	// This implies that if a joint is at the top left of the window, 
	// the projected coordinates for the joint will be ~ (1,0)
	// OpenGl coordinate system in use is -1 to 1 for both x and y.
	// That means that the top-left joint would be (-1, 1) in OpenGl.
	// => X-axis 1 to 0 needs to be translated to -1 to 1
	// => Y-axis 1 to 0 needs to be translated to -1 to 1

	if (j1.confidence > 0.15 && j2.confidence > 0.15)
	{
		_lines[numLines] = (-j1.proj.x * 2) + 1;
		_lines[numLines+1] = (-j1.proj.y * 2) + 1;
		_lines[numLines+2] = (-j2.proj.x * 2) + 1;
		_lines[numLines+3] = (-j2.proj.y * 2) + 1;

		numLines += 4;
		return true;
	}
	else 
	{
		return false;
	}
}

// Render prepared background texture
void NuitrackGL::renderTexture()
{
	GLCall(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
	GLCall(glClear(GL_COLOR_BUFFER_BIT));
	
	GLCall(glBindTexture(GL_TEXTURE_2D, _textureID));
	// Todo: Look into using multiple texture buffers for performance
	GLCall(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, _textureBuffer));

	GLCall(glBindVertexArray(VAO));
	GLCall(glUseProgram(shaderProgram));
	GLCall(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
	
	GLCall(glBindVertexArray(0));
}

void NuitrackGL::renderLinesUser(const float* skeletonColor, const float* jointColor, const float& pointSize, const float& lineWidth, const float* lines, const int& size, bool render, const bool& overrideJointColour)
{
	if (size == 0 || !render)
		return;

	GLCall(glBindBuffer(GL_ARRAY_BUFFER, VBO2));
	// Todo: Look into using multiple buffers for performance
	GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, size * sizeof(float), lines));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));

	GLCall(glBindVertexArray(VAO2));


	if (skeletonColorUniformLocation == -1)
	{
		GLCall(skeletonColorUniformLocation = glGetUniformLocation(shaderProgram2, "color"));
	}

	if (pointSizeUniformLocation == -1)
	{
		GLCall(pointSizeUniformLocation = glGetUniformLocation(shaderProgram2, "pointSize"));
	}
	GLCall(glUseProgram(shaderProgram2));


	if (overrideJointColour)
	{
		GLCall(glUniform4f(skeletonColorUniformLocation, skeletonColor[0], skeletonColor[1], skeletonColor[2], skeletonColor[3]));
	}
	else
	{
		if (hasAllJoints)
		{
			GLCall(glUniform4f(skeletonColorUniformLocation, 0.0f, 1.0f, 0.0f, 1.0f));
		}
		else
		{
			GLCall(glUniform4f(skeletonColorUniformLocation, 1.0f, 0.0f, 0.0f, 1.0f));
		}
	}

	GLCall(glEnable(GL_LINE_SMOOTH));
	GLCall(glLineWidth(lineWidth));
	GLCall(glDrawArrays(GL_LINES, 0, size / 2));
	GLCall(glLineWidth(1.0f));
	GLCall(glDisable(GL_LINE_SMOOTH));

	if (overrideJointColour)
	{
		GLCall(glUniform4f(skeletonColorUniformLocation, jointColor[0], jointColor[1], jointColor[2], jointColor[3]));
	}
	else
	{
		if (hasAllJoints)
		{
			GLCall(glUniform4f(skeletonColorUniformLocation, 0.0f, 1.0f, 0.0f, 1.0f));
		}
		else
		{
			GLCall(glUniform4f(skeletonColorUniformLocation, 1.0f, 0.0f, 0.0f, 1.0f));
		}
	}

	GLCall(glEnable(GL_PROGRAM_POINT_SIZE));
	GLCall(glUniform1f(pointSizeUniformLocation, pointSize));
	GLCall(glDrawArrays(GL_POINTS, 0, size / 2));
	GLCall(glDisable(GL_PROGRAM_POINT_SIZE));
	GLCall(glBindVertexArray(0));
}

// Visualize bones, joints and hand positions
void NuitrackGL::renderLinesTrainer(const float *skeletonColor, const float* jointColor, const float& pointSize, const float& lineWidth, const float* lines, const int& size, bool render, const bool& overrideJointColour)
{
	if (size == 0 || !render)
		return;

	GLCall(glBindBuffer(GL_ARRAY_BUFFER, VBO2));
	// Todo: Look into using multiple buffers for performance
	GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, size * sizeof(float), lines));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));
	
	GLCall(glBindVertexArray(VAO2));
	
	
	if (skeletonColorUniformLocation == -1) 
	{
		GLCall(skeletonColorUniformLocation = glGetUniformLocation(shaderProgram2, "color"));
	}

	if (pointSizeUniformLocation == -1)
	{
		GLCall(pointSizeUniformLocation = glGetUniformLocation(shaderProgram2, "pointSize"));
	}
	GLCall(glUseProgram(shaderProgram2));

	
	if (overrideJointColour)
	{
		GLCall(glUniform4f(skeletonColorUniformLocation, skeletonColor[0], skeletonColor[1], skeletonColor[2], skeletonColor[3]));
	}
	else
	{
		GLCall(glUniform4f(skeletonColorUniformLocation, 1.0f, 0.41f, 0.0f, 1.0f));
	}
	
	GLCall(glEnable(GL_LINE_SMOOTH));
	GLCall(glLineWidth(lineWidth));
	GLCall(glDrawArrays(GL_LINES, 0, size / 2));
	GLCall(glLineWidth(1.0f));
	GLCall(glDisable(GL_LINE_SMOOTH));

	if (overrideJointColour)
	{
		GLCall(glUniform4f(skeletonColorUniformLocation, jointColor[0], jointColor[1], jointColor[2], jointColor[3]));
	}
	else
	{
		GLCall(glUniform4f(skeletonColorUniformLocation, 1.0f, 0.41f, 0.0f, 1.0f));
	}

	GLCall(glEnable(GL_PROGRAM_POINT_SIZE));
	GLCall(glUniform1f(pointSizeUniformLocation, pointSize));
	GLCall(glDrawArrays(GL_POINTS, 0, size / 2));
	GLCall(glDisable(GL_PROGRAM_POINT_SIZE));
	GLCall(glBindVertexArray(0));
}

void NuitrackGL::initLines()
{
	GLCall(int vertexShader = glCreateShader(GL_VERTEX_SHADER));
	GLCall(glShaderSource(vertexShader, 1, &vertexShaderSource2, NULL));
	GLCall(glCompileShader(vertexShader));
	// check for shader compile errors
	int success;
	char infoLog[512];
	GLCall(glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success));
	if (!success)
	{
		GLCall(glGetShaderInfoLog(vertexShader, 512, NULL, infoLog));
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// fragment shader
	GLCall(int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
	GLCall(glShaderSource(fragmentShader, 1, &fragmentShaderSource2, NULL));
	GLCall(glCompileShader(fragmentShader));
	// check for shader compile errors
	GLCall(glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success));
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// link shaders
	shaderProgram2 = glCreateProgram();
	GLCall(glAttachShader(shaderProgram2, vertexShader));
	GLCall(glAttachShader(shaderProgram2, fragmentShader));
	GLCall(glLinkProgram(shaderProgram2));
	// check for linking errors
	GLCall(glGetProgramiv(shaderProgram2, GL_LINK_STATUS, &success));
	if (!success) {
		glGetProgramInfoLog(shaderProgram2, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	GLCall(glDeleteShader(vertexShader));
	GLCall(glDeleteShader(fragmentShader));

	GLCall(glGenVertexArrays(1, &VAO2));
	GLCall(glGenBuffers(1, &VBO2));
	
	GLCall(glBindVertexArray(VAO2));
	
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, VBO2));
	GLCall(glBufferData(GL_ARRAY_BUFFER, 72 * sizeof(float), 0, GL_DYNAMIC_DRAW));

	GLCall(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0));
	GLCall(glEnableVertexAttribArray(0));
	
	// These lines can be removed for the final versoin but are helpful while developing
	GLCall(glBindVertexArray(0));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));
	GLCall(glDisableVertexAttribArray(0));
}

void NuitrackGL::initTexture(int width, int height)
{
	GLCall(int vertexShader = glCreateShader(GL_VERTEX_SHADER));
	GLCall(glShaderSource(vertexShader, 1, &vertexShaderSource, NULL));
	GLCall(glCompileShader(vertexShader));
	// check for shader compile errors
	int success;
	char infoLog[512];
	GLCall(glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success));
	if (!success)
	{
		GLCall(glGetShaderInfoLog(vertexShader, 512, NULL, infoLog));
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// fragment shader
	GLCall(int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
	GLCall(glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL));
	GLCall(glCompileShader(fragmentShader));
	// check for shader compile errors
	GLCall(glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success));
	if (!success)
	{
		GLCall(glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog));
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// link shaders
	GLCall(shaderProgram = glCreateProgram());
	GLCall(glAttachShader(shaderProgram, vertexShader));
	GLCall(glAttachShader(shaderProgram, fragmentShader));
	GLCall(glLinkProgram(shaderProgram));
	// check for linking errors
	GLCall(glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success));
	if (!success) {
		GLCall(glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog));
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	GLCall(glDeleteShader(vertexShader));
	GLCall(glDeleteShader(fragmentShader));


	// Set texture coordinates [0, 1] and vertexes position
	float vertices[] = {
		// positions         // texture coords
		1.0f,  1.0f, 0.0f,   1.0f, 1.0f,   // top right
		1.0f, -1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
		-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,   // bottom left
		-1.0f,  1.0f, 0.0f,  0.0f, 1.0f    // top left 
	};

	unsigned int indices[] = {
		0, 1, 3, //first triangle
		1, 2, 3  //second triangle
	};

	GLCall(glGenVertexArrays(1, &VAO));
	GLCall(glGenBuffers(1, &VBO));
	GLCall(glGenBuffers(1, &EBO));
	
	GLCall(glBindVertexArray(VAO));
	
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, VBO));
	GLCall(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));
	
	GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
	GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));
	
	// position attribute
	GLCall(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0));
	GLCall(glEnableVertexAttribArray(0));
	// texture coord attribute
	GLCall(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))));
	GLCall(glEnableVertexAttribArray(1));
	
	if (_textureBuffer != 0)
		delete[] _textureBuffer;
	
	_textureBuffer = new uint8_t[width * height * 3];
	memset(_textureBuffer, 0, sizeof(uint8_t) * width * height * 3);
	
	GLCall(glGenTextures(1, &_textureID));
	GLCall(glBindTexture(GL_TEXTURE_2D, _textureID));
	
	GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	
	// This line effectively does not allow resolution of texture to be changed.
	GLCall(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL));	
	
	// These lines can be removed for the final versoin but are helpful while developing
	GLCall(glBindVertexArray(0));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));
	GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	GLCall(glDisableVertexAttribArray(0));
	GLCall(glDisableVertexAttribArray(1));
}
