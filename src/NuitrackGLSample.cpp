#include "NuitrackGLSample.h"

#include <string>
#include <iostream>
#include "UserInteraction.h"


NuitrackGLSample::NuitrackGLSample() :
	_textureID(0),
	_textureBuffer(0),
	_width(640),
	_height(480),
	_isInitialized(false)
{}

NuitrackGLSample::~NuitrackGLSample()
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
"}\0";

const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D ourTexture;\n"
"void main()\n"
"{\n"
"   FragColor = texture(ourTexture, TexCoord);\n"
"}\n\0";

void NuitrackGLSample::init(const std::string& config)
{
	// Initialize Nuitrack first, then create Nuitrack modules
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

		bool isActivated = device->getActivationStatus() != tdv::nuitrack::device::ActivationStatus::NONE;

		if (isActivated)
			isActivated = !UserInteraction::confirm("The device is already activated! Do you want to reactivate it?");
		else
			std::cout << "device isnt activated............." << std::endl;

		std::cout << "hello before activating..." << std::endl;
		if (!isActivated)
		{
			std::string activationKey = "license:19372:Qep454hfDLfyNCYh";
			device->activate(activationKey);
			std::cout << "hello i activated" << std::endl;
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
	_colorSensor->connectOnNewFrame(std::bind(&NuitrackGLSample::onNewRGBFrame, this, std::placeholders::_1));

	_outputMode = _colorSensor->getOutputMode();
	_width = _outputMode.xres;
	_height = _outputMode.yres;

	_userTracker = tdv::nuitrack::UserTracker::create();
	_userTracker->connectOnNewUser(std::bind(&NuitrackGLSample::onNewUserCallback, this, std::placeholders::_1));
	_userTracker->connectOnLostUser(std::bind(&NuitrackGLSample::onLostUserCallback, this, std::placeholders::_1));

	_skeletonTracker = tdv::nuitrack::SkeletonTracker::create();
	_skeletonTracker->connectOnUpdate(std::bind(&NuitrackGLSample::onSkeletonUpdate, this, std::placeholders::_1));

	_onIssuesUpdateHandler = tdv::nuitrack::Nuitrack::connectOnIssuesUpdate(std::bind(&NuitrackGLSample::onIssuesUpdate, this, std::placeholders::_1));
}

bool NuitrackGLSample::update()
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
		// Wait and update Nuitrack data
		tdv::nuitrack::Nuitrack::waitUpdate(_skeletonTracker);
		
		renderTexture();
		renderLines();
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

void NuitrackGLSample::release()
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

// Copy color frame data, received from Nuitrack, to texture to visualize
void NuitrackGLSample::onNewRGBFrame(tdv::nuitrack::RGBFrame::Ptr frame)
{
	// Storing from end of the buffer to start because frame data is received from top to bottom
	// OpenGl requires texture data from the bottom to top.
	// Not sure about left to right yet (because we have a free way to mirror flip image using Nuitrack config
	uint8_t* texturePtr = _textureBuffer + (3 * _width * _height) - 1;
	const tdv::nuitrack::Color3* colorPtr = frame->getData();

	float wStep = (float)_width / frame->getCols();
	float hStep = (float)_height / frame->getRows();

	std::cout << "Output : " << frame->getCols() << std::endl << "Output rows: " << frame->getRows() << std::endl;

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

// Callback for onLostUser event
void NuitrackGLSample::onLostUserCallback(int id)
{
	std::cout << "Lost User " << id << std::endl;
}

// Callback for onNewUser event
void NuitrackGLSample::onNewUserCallback(int id)
{
	std::cout << "New User " << id << std::endl;
}

// Prepare visualization of skeletons, received from Nuitrack
void NuitrackGLSample::onSkeletonUpdate(tdv::nuitrack::SkeletonData::Ptr userSkeletons)
{
	_lines.clear();
	
	auto skeletons = userSkeletons->getSkeletons();
	for (auto skeleton: skeletons)
	{
		drawSkeleton(skeleton.joints);
	}
}

void NuitrackGLSample::onIssuesUpdate(tdv::nuitrack::IssuesData::Ptr issuesData)
{
	_issuesData = issuesData;
}

// Helper function to draw a skeleton bone
void NuitrackGLSample::drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2)
{
	// Prepare line data for confident enough bones only
	if (j1.confidence > 0.15 && j2.confidence > 0.15)
	{
		_lines.push_back(_width * j1.proj.x);
		_lines.push_back(_height * j1.proj.y);
		_lines.push_back(_width * j2.proj.x);
		_lines.push_back(_height * j2.proj.y);
	}
}

// Helper function to draw skeleton from Nuitrack data
void NuitrackGLSample::drawSkeleton(const std::vector<tdv::nuitrack::Joint>& joints)
{
	// We need to draw a bone for every pair of neighbour joints
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
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_WRIST], joints[tdv::nuitrack::JOINT_LEFT_HAND]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_SHOULDER], joints[tdv::nuitrack::JOINT_RIGHT_ELBOW]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_ELBOW], joints[tdv::nuitrack::JOINT_RIGHT_WRIST]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_WRIST], joints[tdv::nuitrack::JOINT_RIGHT_HAND]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_HIP], joints[tdv::nuitrack::JOINT_RIGHT_KNEE]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_HIP], joints[tdv::nuitrack::JOINT_LEFT_KNEE]);
	drawBone(joints[tdv::nuitrack::JOINT_RIGHT_KNEE], joints[tdv::nuitrack::JOINT_RIGHT_ANKLE]);
	drawBone(joints[tdv::nuitrack::JOINT_LEFT_KNEE], joints[tdv::nuitrack::JOINT_LEFT_ANKLE]);
}

// Render prepared background texture
void NuitrackGLSample::renderTexture()
{
	glClearColor(1.0f, 0.5f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glBindTexture(GL_TEXTURE_2D, _textureID);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, _textureBuffer);

	glBindVertexArray(VAO);
	glUseProgram(shaderProgram);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

int NuitrackGLSample::power2(int n)
{
	unsigned int m = 2;
	while (m < n)
		m <<= 1;
	
	return m;
}

// Visualize bones, joints and hand positions
void NuitrackGLSample::renderLines()
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
	
	if (!_leftHandPointers.empty())
	{
		glColor4f(1, 0, 0, 1);
		glPointSize(16);
		glVertexPointer(2, GL_FLOAT, 0, _leftHandPointers.data());
		glDrawArrays(GL_POINTS, 0, 1);
		if (_leftHandPointers.size() > 2)
		{
			glPointSize(24);
			glVertexPointer(2, GL_FLOAT, 0, _leftHandPointers.data() + 2);
			glDrawArrays(GL_POINTS, 0, 1);
		}
	}
	
	if (!_rightHandPointers.empty())
	{
		glColor4f(0, 0, 1, 1);
		glPointSize(16);
		glVertexPointer(2, GL_FLOAT, 0, _rightHandPointers.data());
		glDrawArrays(GL_POINTS, 0, 1);
		if (_rightHandPointers.size() > 2)
		{
			glPointSize(24);
			glVertexPointer(2, GL_FLOAT, 0, _rightHandPointers.data() + 2);
			glDrawArrays(GL_POINTS, 0, 1);
		}
	}
	
	glColor4f(1, 1, 1, 1);
	glPointSize(1);
	glDisable(GL_POINT_SMOOTH);
	
	glDisableClientState(GL_VERTEX_ARRAY);
}

void NuitrackGLSample::initTexture(int width, int height)
{
	int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// check for shader compile errors
	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// fragment shader
	int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// check for shader compile errors
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	}
	// link shaders
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	// check for linking errors
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);


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

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// texture coord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	if (_textureBuffer != 0)
		delete[] _textureBuffer;
	
	_textureBuffer = new uint8_t[width * height * 3];
	memset(_textureBuffer, 0, sizeof(uint8_t) * width * height * 3);
	
	glGenTextures(1, &_textureID);
	glBindTexture(GL_TEXTURE_2D, _textureID);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);	
}
