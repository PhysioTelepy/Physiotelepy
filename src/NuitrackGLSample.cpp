#include "NuitrackGLSample.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace tdv::nuitrack;

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
		Nuitrack::release();
	}
	catch (const Exception& e)
	{
		// Do nothing
	}
}

void NuitrackGLSample::init(const std::string& config)
{
	// Initialize Nuitrack first, then create Nuitrack modules
	try
	{
		Nuitrack::init(config);
	}
	catch (const Exception& e)
	{
		std::cerr << "Can not initialize Nuitrack (ExceptionType: " << e.type() << ")" << std::endl;
		exit(EXIT_FAILURE);
	}
	
	// Create all required Nuitrack modules

	_depthSensor = DepthSensor::create();
	_colorSensor = ColorSensor::create();
	_colorSensor->connectOnNewFrame(std::bind(&NuitrackGLSample::onNewRGBFrame, this, std::placeholders::_1));

	_outputMode = _colorSensor->getOutputMode();
	_width = _outputMode.xres;
	_height = _outputMode.yres;

	_userTracker = UserTracker::create();
	_userTracker->connectOnNewUser(std::bind(&NuitrackGLSample::onNewUserCallback, this, std::placeholders::_1));
	_userTracker->connectOnLostUser(std::bind(&NuitrackGLSample::onLostUserCallback, this, std::placeholders::_1));

	_skeletonTracker = SkeletonTracker::create();
	_skeletonTracker->connectOnUpdate(std::bind(&NuitrackGLSample::onSkeletonUpdate, this, std::placeholders::_1));

	_onIssuesUpdateHandler = Nuitrack::connectOnIssuesUpdate(std::bind(&NuitrackGLSample::onIssuesUpdate, this, std::placeholders::_1));
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
			Nuitrack::run();
		}
		catch (const Exception& e)
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
		Nuitrack::waitUpdate(_skeletonTracker);
		
		renderTexture();
		renderLines();
	}
	catch (const LicenseNotAcquiredException& e)
	{
		// Update failed, negative result
		std::cerr << "LicenseNotAcquired exception (ExceptionType: " << e.type() << ")" << std::endl;
		return false;
	}
	catch (const Exception& e)
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
		Nuitrack::disconnectOnIssuesUpdate(_onIssuesUpdateHandler);

	// Release Nuitrack and remove all modules
	try
	{
		Nuitrack::release();
	}
	catch (const Exception& e)
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
void NuitrackGLSample::onNewRGBFrame(RGBFrame::Ptr frame)
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
void NuitrackGLSample::onSkeletonUpdate(SkeletonData::Ptr userSkeletons)
{
	_lines.clear();
	
	auto skeletons = userSkeletons->getSkeletons();
	for (auto skeleton: skeletons)
	{
		drawSkeleton(skeleton.joints);
	}
}

void NuitrackGLSample::onIssuesUpdate(IssuesData::Ptr issuesData)
{
	_issuesData = issuesData;
}

// Helper function to draw a skeleton bone
void NuitrackGLSample::drawBone(const Joint& j1, const Joint& j2)
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
void NuitrackGLSample::drawSkeleton(const std::vector<Joint>& joints)
{
	// We need to draw a bone for every pair of neighbour joints
	drawBone(joints[JOINT_HEAD], joints[JOINT_NECK]);
	drawBone(joints[JOINT_NECK], joints[JOINT_LEFT_COLLAR]);
	drawBone(joints[JOINT_LEFT_COLLAR], joints[JOINT_TORSO]);
	drawBone(joints[JOINT_LEFT_COLLAR], joints[JOINT_LEFT_SHOULDER]);
	drawBone(joints[JOINT_LEFT_COLLAR], joints[JOINT_RIGHT_SHOULDER]);
	drawBone(joints[JOINT_WAIST], joints[JOINT_LEFT_HIP]);
	drawBone(joints[JOINT_WAIST], joints[JOINT_RIGHT_HIP]);
	drawBone(joints[JOINT_TORSO], joints[JOINT_WAIST]);
	drawBone(joints[JOINT_LEFT_SHOULDER], joints[JOINT_LEFT_ELBOW]);
	drawBone(joints[JOINT_LEFT_ELBOW], joints[JOINT_LEFT_WRIST]);
	drawBone(joints[JOINT_LEFT_WRIST], joints[JOINT_LEFT_HAND]);
	drawBone(joints[JOINT_RIGHT_SHOULDER], joints[JOINT_RIGHT_ELBOW]);
	drawBone(joints[JOINT_RIGHT_ELBOW], joints[JOINT_RIGHT_WRIST]);
	drawBone(joints[JOINT_RIGHT_WRIST], joints[JOINT_RIGHT_HAND]);
	drawBone(joints[JOINT_RIGHT_HIP], joints[JOINT_RIGHT_KNEE]);
	drawBone(joints[JOINT_LEFT_HIP], joints[JOINT_LEFT_KNEE]);
	drawBone(joints[JOINT_RIGHT_KNEE], joints[JOINT_RIGHT_ANKLE]);
	drawBone(joints[JOINT_LEFT_KNEE], joints[JOINT_LEFT_ANKLE]);
}

// Render prepared background texture
void NuitrackGLSample::renderTexture()
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
		_textureCoords[0] = (float) _width / width;
		_textureCoords[1] = (float) _height / height;
		_textureCoords[2] = (float) _width / width;
		_textureCoords[3] = 0.0;
		_textureCoords[4] = 0.0;
		_textureCoords[5] = 0.0;
		_textureCoords[6] = 0.0;
		_textureCoords[7] = (float) _height / height;
		
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
