#ifndef NUITRACKGLSAMPLE_H_
#define NUITRACKGLSAMPLE_H_

#include <glad/glad.h>
#include <nuitrack/Nuitrack.h>

#ifdef _WIN32
#include <Windows.h>
#endif
#include <string>


typedef enum
{
	DEPTH_SEGMENT_MODE = 0,
	RGB_MODE = 1,
	MODES_MAX_COUNT
} ViewMode;



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
	bool update();
	
	// Release all sample resources
	void release();

	tdv::nuitrack::OutputMode getOutputMode() const
	{
		return _outputMode;
	}

private:
	int _width, _height;
	
	// GL data
	int shaderProgram;
	unsigned int VBO, VAO, EBO;
	GLuint _textureID;
	uint8_t* _textureBuffer;
	GLfloat _textureCoords[8];
	GLfloat _vertexes[8];
	std::vector<GLfloat> _lines;
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
	void renderLines();
	
	int power2(int n);
	void initTexture(int width, int height);
};

#endif /* NUITRACKGLSAMPLE_H_ */
