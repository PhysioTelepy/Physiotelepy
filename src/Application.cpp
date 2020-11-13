
#include "NuitrackGLSample.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <iostream>

#include <direct.h>
#define GetCurrentDir _getcwd

#include <string>
#include <windows.h>

std::string getexepath()
{
	char result[MAX_PATH];
	return std::string(result, GetModuleFileName(NULL, result, MAX_PATH));
}

NuitrackGLSample sample;

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

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

int main(int argc, char* argv[])
{
	std::cout << get_current_dir() << std::endl;

	std::cout << "yuh" + get_current_dir() << std::endl;

	// Prepare sample to work
	sample.init("../nuitrack/data/nuitrack.config");
	auto outputMode = sample.getOutputMode();
	

	/* Initialize the library */
	if (!glfwInit())
	{
		return -1;
	}

	const char* glsl_version = "#version 460";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //Todo: Set this to core and implement shaders at some point.

	GLFWwindow* window = glfwCreateWindow(outputMode.xres, outputMode.yres, "Physiotelepy", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		std::cout << "window ptr broke" << std::endl;
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	std::cout << glGetString(GL_VERSION) << std::endl;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	bool showDemoWindow = false;

	// Start main loop
	while (!glfwWindowShouldClose(window))
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (showDemoWindow)
			ImGui::ShowDemoWindow(&showDemoWindow);

		{
			ImGui::Begin("Debug Window");
			//ImGui::Checkbox("Demo window", &showDemoWindow);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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

	return 0;
}
