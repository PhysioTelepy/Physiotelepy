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

struct Vector2
{
	float x;
	float y;
};

struct Vector3
{
	float x;
	float y;
	float z;
};

// This data structure is too heavy, need to make is smaller.
// Real world coordinates are not required
// All joints are probably not required
// Orientation is not required
// (Maybe Data for non-confident joints can also be stripped?)
// With this data structure 20 secs of joint data (without sampling) is about 0.8MB!!!
// Maybe even think about compression
// Target for 20sec of video should be ~100-200KB
// Maybe add a sample rate (10 times /second or something?)
struct JointFrame 
{
	std::time_t timeStamp;
	Vector3 realJoints[25];
	float confidence[25];
};

// Main class of the sample
class NuitrackGL final
{
public:
	NuitrackGL();
	~NuitrackGL();

	// Initialize sample: initialize Nuitrack, create all required modules,
	// register callbacks and start Nuitrack
	void init(const std::string& config = "");

	void onUserUpdateCallback(tdv::nuitrack::UserFrame::Ptr frame);

	void onNewGesture(tdv::nuitrack::GestureData::Ptr gestureData);

	// Update the depth map, tracking and gesture recognition data,
	// then redraw the view
	bool update(float* skeletonColor, float* jointColor, const float& pointSize, const float& lineWidth, const bool& overrideJointColour);

	// Release all sample resources
	void release();

	tdv::nuitrack::OutputMode getOutputMode() const
	{
		return _outputMode;
	}

	// Record skeleton data for a duration in seconds
	void startRecording(const int& duration);
	void loadDataToBuffer(const std::string& path);
	void saveBufferToDisk();
	void playLoadedData();

private:
	int userAngles[19];

	std::mutex jointDataBufferMutex;

	std::vector<JointFrame> writeJointDataBuffer;
	std::vector<JointFrame> readJointDataBuffer;
	int replayPointer = 0;

	std::atomic<bool> record;
	std::atomic<bool> saving;

	std::atomic<bool> replay;



	int _width, _height;
	// GL data

	int num_frames_to_record = 0;

	GLuint _textureID;
	uint8_t* _textureBuffer;
	GLfloat _textureCoords[8];
	GLfloat _vertexes[8];
	std::vector<GLfloat> _lines;
	bool hasAllJoints = false;

	std::vector<tdv::nuitrack::Gesture> _userGestures;

	tdv::nuitrack::OutputMode _outputMode;
	tdv::nuitrack::DepthSensor::Ptr _depthSensor;
	tdv::nuitrack::ColorSensor::Ptr _colorSensor;
	tdv::nuitrack::UserTracker::Ptr _userTracker;
	tdv::nuitrack::SkeletonTracker::Ptr _skeletonTracker;
	tdv::nuitrack::GestureRecognizer::Ptr _gestureRecognizer;

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
	void onNewDepthFrame(tdv::nuitrack::DepthFrame::Ptr frame);

	/**
	 * Draw methods
	 */
	void drawSkeleton(const std::vector<tdv::nuitrack::Joint>& joints);
	void drawBone(const JointFrame& j1, int index1, int index2);
	bool drawBone(const tdv::nuitrack::Joint& j1, const tdv::nuitrack::Joint& j2);
	void renderTexture();
	void renderLinesUser();
	void renderLinesTrainer(const float* skeletonColor, const float* jointColor, const float& pointSize, const float& lineWidth, const float* lines, const int& numLines, bool render, const bool& overrideJointColour);

	int power2(int n);

	void updateTrainerSkeleton();

	void initTexture(int width, int height);
};

#endif /* NUITRACKGLSAMPLE_H_ */