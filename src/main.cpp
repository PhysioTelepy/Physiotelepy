
#include "NuitrackGLSample.h"
#include "GLFW/glfw3.h"

#include <iostream>

NuitrackGLSample sample;

void showHelpInfo()
{
	std::cout << "Usage: nuitrack_gl_sample [path/to/nuitrack.config]\n"
		"Press Esc to close window." << std::endl;
}

int main(int argc, char* argv[])
{
	showHelpInfo();

	// Prepare sample to work
	if (argc < 2)
		sample.init();
	else
		sample.init(argv[1]);

	auto outputMode = sample.getOutputMode();

	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
	{
		return -1;
	}

	window = glfwCreateWindow(outputMode.xres, outputMode.yres, "Physiotelepy", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Maybe query system refresh rate to set video to 30fps


	// Setup OpenGL
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glOrtho(0, outputMode.xres, outputMode.yres, 0, -1.0, 1.0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	// Start main loop
	while (!glfwWindowShouldClose(window))
	{
		// Delegate this action to example's main class
		bool update = sample.update();

		if (!update)
		{
			// End the work if update failed
			sample.release();
			glfwTerminate();
			exit(EXIT_FAILURE);
		}

		// Do flush and swap buffers to update viewport
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}
