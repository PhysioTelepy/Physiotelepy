
//#include "NuitrackGLSample.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <iostream>

#include <direct.h>
#define GetCurrentDir _getcwd


//NuitrackGLSample sample;

void showHelpInfo()
{
	std::cout << "Usage: nuitrack_gl_sample [path/to/nuitrack.config]\n"
		"Press Esc to close window." << std::endl;
}

std::string get_current_dir() {
	char buff[FILENAME_MAX]; //create string buffer to hold path
	GetCurrentDir(buff, FILENAME_MAX);
	std::string current_working_dir(buff);
	return current_working_dir;
} 

int main2(int argc, char* argv[])
{
	/*
	showHelpInfo();

	// Prepare sample to work
	sample.init("../../nuitrack/data/nuitrack.config");

	auto outputMode = sample.getOutputMode();

	/* Initialize the library */
	/*
	if (!glfwInit())
	{
		return -1;
	}

	const char* glsl_version = "#version 460";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE); //Todo: Set this to core and implement shaders at some point.

	GLFWwindow* window = glfwCreateWindow(outputMode.xres, outputMode.yres, "Physiotelepy", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Maybe query system refresh rate to set video to 30fps

	if (!gladLoadGL())
	{
		return -1;
	}

	std::cout << glGetString(GL_VERSION) << std::endl;

	// Setup OpenGL
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	//glEnableClientState(GL_VERTEX_ARRAY);
	//glDisableClientState(GL_COLOR_ARRAY);

	//glOrtho(0, outputMode.xres, outputMode.yres, 0, -1.0, 1.0);
	//glMatrixMode(GL_PROJECTION);
	//glPushMatrix();
	//glLoadIdentity();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	bool showDemoWindow = true;

	// Start main loop
	while (!glfwWindowShouldClose(window))
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (showDemoWindow)
			ImGui::ShowDemoWindow(&showDemoWindow);

		{
			ImGui::Begin("Test Window");
			ImGui::Checkbox("Demo window", &showDemoWindow);
			ImGui::End();
		}

		// Delegate this action to example's main class
		bool update = sample.update();

		if (!update)
		{
			// End the work if update failed
			sample.release();
			glfwTerminate();
			exit(EXIT_FAILURE);
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Do flush and swap buffers to update viewport
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	*/
	return 0;
}
