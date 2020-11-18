#ifndef NUITRACKGLSAMPLE_H_
#define NUITRACKGLSAMPLE_H_

#include "opgl.h"
#include <nuitrack/Nuitrack.h>
#include <string>
#include <ctime>
#include <chrono>

typedef enum
{
	DEPTH_SEGMENT_MODE = 0,
	RGB_MODE = 1,
	MODES_MAX_COUNT
} ViewMode;

// This data structure is too heavy, need to make is smaller.
// Real world coordinates are not required
// All joints are probably not required
// Orientation is not required
// (Maybe Data for non-confident joints can also be stripped?)
// With this data structure 20 secs of joint data (without sampling) is about 0.8MB!!!
// Maybe even think about compression
// Target for 20sec of video should be ~100-200KB
// Maybe add a sample rate (10 times /second or something?)
struct JointFrame {
	std::time_t timeStamp;
	std::vector<tdv::nuitrack::Joint> joints;
};


// Main class of the sample
class NuitrackGLSample final
{
public:
	NuitrackGLSample();
	~NuitrackGLSample();
	
	// Initialize sample: initialize Nuitrack, create all required modules,
	// register callbacks and start Nuitrack
	void init(const std::string& config = "");
	
	// Update the depth map, tracking and gesture recognition data,
	// then redraw the view
	bool update(float* skeletonColor, float* jointColor, const float& pointSize, const float& lineWidth);
	
	// Release all sample resources
	void release();

	tdv::nuitrack::OutputMode getOutputMode() const
	{
		return _outputMode;
	}

	// Record skeleton data for a duration in seconds
	void startRecording(int duration);

	void timer(int duration);

	void saveBufferToDisk();


private:
	std::mutex jointDataBufferMutex;
	std::vector<JointFrame> jointDataBuffer;
	std::atomic<bool> record;
	std::atomic<bool> saving;
	int _width, _height;
	// GL data
	int skeletonColorUniformLocation = -1;
	int pointSizeUniformLocation = -1;
	int shaderProgram;
	int shaderProgram2;
	unsigned int VBO, VAO, EBO; // For textures
	unsigned int VBO2, VAO2; // For lines
	GLuint _textureID;
	uint8_t* _textureBuffer;
	GLfloat _textureCoords[8];
	GLfloat _vertexes[8];
	GLfloat _lines[72];
	int numLines = 0;
	std::vector<GLfloat> _leftHandPointers;
	std::vector<GLfloat> _rightHandPointers;
	std::vector<tdv::nuitrack::Gesture> _userGestures;
	
	tdv::nuitrack::OutputMode _outputMode;
	
	tdv::nuitrack::DepthSensor::Ptr _depthSensor;
	tdv::nuitrack::ColorSensor::Ptr _colorSensor;
	tdv::nuitrack::UserTracker::Ptr _userTracker;
	tdv::nuitrack::SkeletonTracker::Ptr _skeletonTracker;
	
	tdv::nuitrack::IssuesData::Ptr _issuesData;
	uint64_t _onIssuesUpdateHandler;
	
	bool _isInitialized;

	/**
	 * Nuitrack callbacks
	 */
	void onNewRGBFrame(tdv::nuitrack::RGBFrame::Ptr frame);
	void onLostUserCallback(int id);
	void onNewUserCallback(int id);
	void onSkeletonUpdate(tdv::nuitrack::SkeletonData::Ptr userSkeletons);
	void onIssuesUpdate(tdv::nuitrack::IssuesData::Ptr issuesData);
	
	/**
	 * Draw methods
	 */
	void drawSkeleton(const std::vector<tdv::nuitrack::Joint>& joints);
	void drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2);
	void renderTexture();
	void renderLines(float* skeletonColor, float* jointColor, const float& pointSize, const float& lineWidth);
	
	void initTexture(int width, int height);
	void initLines();
	void stopRecording();
};

#endif /* NUITRACKGLSAMPLE_H_ */
