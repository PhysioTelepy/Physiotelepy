
#include "NuitrackGL.h"
#include "GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <iostream>
#include <fstream>
#include <direct.h>
#include <Windows.h>
#include "ApplicationState.h"


#include <ios>
#include "sqlite3/sqlite3.h"

#define GetCurrentDir _getcwd
#define MENU_LENGTH 500

NuitrackGL sample;

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

std::string openfilename(char* filter = "All Files (*.*)\0*.*\0", HWND owner = NULL) {
	OPENFILENAME ofn;
	char fileName[MAX_PATH] = "";
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = owner;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "";

	std::string fileNameStr;

	if (GetOpenFileName(&ofn))
		fileNameStr = fileName;

	return fileNameStr;
}

void sql_test() {
	sqlite3* db;
	sqlite3_open("C:/dev/Database/test.db", &db);

	std::string query2 = "INSERT INTO Therapist (\"name\", \"rating\", \"age\") VALUES (\"Test\", \"2\", \"21\");";

	sqlite3_stmt* insertStmt;
	std::cout << "Insertng Table Statement" << std::endl;
	sqlite3_prepare(db, query2.c_str(), query2.size(), &insertStmt, NULL);
	std::cout << "Stepping Table Statement" << std::endl;
	if (sqlite3_step(insertStmt) != SQLITE_DONE) std::cout << "Didn't Insert into Table!" << std::endl;
}

ApplicationState state;

int main(int argc, char* argv[])
{

	sql_test();

	state.mainSelection = false;

	std::cout << get_current_dir() << std::endl;

	// Prepare sample to work
	sample.init("../nuitrack/data/nuitrack.config");

	auto outputMode = sample.getOutputMode();

	/* Initialize the library */
	if (!glfwInit())
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	/*
	const char* glsl_version = "#version 330";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //Todo: Set this to core and implement shaders at some point.
	*/

	GLFWwindow* window = glfwCreateWindow(MENU_LENGTH + outputMode.xres, outputMode.yres, "Physiotelepy", NULL, NULL);
	if (!window)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// Setup OpenGL
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);


	glOrtho(-MENU_LENGTH, outputMode.xres, outputMode.yres, 0, -1.0, 1.0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glfwSwapInterval(0);

	std::cout << glGetString(GL_VERSION) << std::endl;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	bool showDemoWindow = false;
	bool overrideJointColour = false;
	float skeletonColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float jointColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float pointSize = 10.0f;
	float lineWidth = 4.0f;

	int recordDuration = 20; // In seconds

	char userName[128] = "";
	char password[128] = "";
	char exerciseName[128] = "";

	bool joints[25];

	for (int i = 0; i < 25; i++)
	{
		joints[i] = true;
	}

	// Start main loop
	while (!glfwWindowShouldClose(window))
	{
		//std::thread::id this_id = std::this_thread::get_id();
		//std::cout << "Main thread: " << this_id << std::endl;

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (showDemoWindow)
			ImGui::ShowDemoWindow(&showDemoWindow);

		ImGui::SetNextWindowSize({ MENU_LENGTH, (float) outputMode.yres });
		ImGui::SetNextWindowPos({ 0.0, 0.0 });

		/*
		{
			ImGui::Begin("Record");
			ImGui::InputInt("Number of Frames", &recordDuration);
			if (ImGui::Button("Start recording"))
			{
				sample.startRecording(recordDuration);
			}
			ImGui::End();
		}

		{
			ImGui::Begin("Load Joint Data from Disk");
			if (ImGui::Button("Load"))
			{
				std::string path = openfilename();
				sample.loadDataToBuffer(path);
			}

			if (ImGui::Button("Play loaded data"))
			{
				sample.playLoadedData();
			}
			ImGui::End();
		}
		*/
		
		{
			if (!state.mainSelection) 
			{
				ImGui::Begin("Are you a therapist or a Patient?");
				if (ImGui::Button("Physiotherapist")) 
				{
					state.mainSelection = true;
					state.isPatient = false;
				}

				if (ImGui::Button("Patient"))
				{
					state.mainSelection = true;
					state.isPatient = true;
					state.patientState.currentScreen = LOGIN_PATIENT;
				}
				ImGui::End();
			}
			else
			{
				if (state.isPatient) {
					switch (state.patientState.currentScreen) {
						case LOGIN_PATIENT: 
						{
							ImGui::Begin("Login");
							ImGui::InputTextWithHint("User Name", "Enter text here", userName, IM_ARRAYSIZE(userName));
							ImGui::InputTextWithHint("Password", "Enter text here", password, IM_ARRAYSIZE(password));
							if (ImGui::Button("Login")) {
								//Assuming successful login, populate patient details
								state.patientState.currentScreen = EXERCISE_HUB;
							}
							if (ImGui::Button("Back")) {
								state.mainSelection = false;
							}
							ImGui::End();
							break;
						}
						
						case EXERCISE_HUB:
						{
							ImGui::Begin("Exercise Hub");
							ImGui::Text("Assigned Exercises............");
							
							for (auto it = state.patientState.details->assignedExercisesMap.begin(); it != state.patientState.details->assignedExercisesMap.end(); ++it)
							{
								std::string key = it->first;
								std::string value = it->second;

								ImGui::Text(key.c_str()); ImGui::SameLine();
								if (ImGui::Button("Perform Exercise")) {
									state.patientState.currentExercise = key;
									state.patientState.currentScreen = EXERCISE_HUB;
								}
							}

							ImGui::Text("Completed Exercises...........");
							// List of compelted exercises

							if (ImGui::Button("Logout")) {
								userName[0] = '\0';
								password[0] = '\0';
								delete state.patientState.details;
								delete state.patientState.trainerDetails;
								state.patientState.details = NULL;
								state.patientState.trainerDetails = NULL;
								state.patientState.currentScreen = LOGIN_PATIENT;

							}
							ImGui::End();
							break;
						}

						case PERFORMING_EXERCISE:
						{

						}

						case FEEDBACK_HUB:
						{

						}
					}

				}
				else
				{
					switch (state.trainerState.currentScreen) {
						case LOGIN_TRAINER:
						{
							ImGui::Begin("Login");
							ImGui::InputTextWithHint("User Name", "Enter text here", userName, IM_ARRAYSIZE(userName));
							ImGui::InputTextWithHint("Password", "Enter text here", password, IM_ARRAYSIZE(password));

							


							if (ImGui::Button("Login")) { // 1 - username, pass -> true, false
								//Assuming successful login, populate patient details
								state.trainerState.currentScreen = PATIENT_DETAILS;

								// 2 - trainerId -> trainer details

								// 3 - trainer Id -> Patient Details

								//Dummy Data
								state.trainerState.patientDetails = new std::vector<PatientDetails>();
								PatientDetails patient1;
								patient1.patientID = "123";
								patient1.age = 24;
								patient1.name = "Dummy patient";
								patient1.assignedExercisesMap.insert(std::pair<std::string, std::string>("Exercise1", "Test"));
								patient1.compeltedExercisesMap.insert(std::pair<std::string, std::pair<std::string, std::string>>("Exercise2", std::pair<std::string, std::string>("test2", "test3")));
								state.trainerState.patientDetails->push_back(patient1);


							}
							if (ImGui::Button("Back")) {
								state.mainSelection = false;
							}
							ImGui::End();
							break;
						}

						case PATIENT_DETAILS:
						{
							ImGui::Begin("Patient Details");
							if (ImGui::Button("Refresh")) 
							{
								//reload patient detials (no need to do on seperate thread, as main thread isn't doing anything else now.
							}
							
							for (auto it = state.trainerState.patientDetails->begin(); it != state.trainerState.patientDetails->end(); it++) 
							{
								PatientDetails details = *it;
								int index = it - state.trainerState.patientDetails->begin();
								std::string title = "Patient ";
								title.append(std::to_string(index + 1));
								title.append(".................");
								ImGui::Text(title.c_str());
								
								ImGui::Text(("Name - " + details.name).c_str());
								if (ImGui::Button("Create & Assign an exercise")) {
									state.trainerState.currentScreen = EXERCISE_CREATOR;
									state.trainerState.exerciseCreatorIndex = index;
									state.trainerState.exerciseCreated = false;
									state.trainerState.exerciseCreation = false;
								}
								ImGui::Text("Assigned Exercises");
								for (auto it = details.assignedExercisesMap.begin(); it != details.assignedExercisesMap.end(); ++it)
								{
									std::string key = it->first;
									std::string value = it->second;

									ImGui::Text(key.c_str());
								}

								ImGui::Text("Completed Exercises");
								for (auto it = details.compeltedExercisesMap.begin(); it != details.compeltedExercisesMap.end(); ++it) 
								{
									std::string key = it->first;

									
									
									ImGui::Text(key.c_str()); ImGui::SameLine();
									ImGui::Button("Analysis and Feedback");
								}

							}

							if (ImGui::Button("Logout")) {
								userName[0] = '\0';
								password[0] = '\0';
								delete state.trainerState.patientDetails;
								delete state.trainerState.trainerDetails;
								state.trainerState.patientDetails = NULL;
								state.trainerState.trainerDetails = NULL;
								state.trainerState.currentScreen = LOGIN_TRAINER;

							}
							ImGui::End();
							break;
						}

						case EXERCISE_CREATOR:
						{
							ImGui::Begin("Exercise Creation");

							if (!state.trainerState.exerciseCreated)
							{
								if (!state.trainerState.exerciseCreation)
								{
									if (ImGui::Button("Back")) {
										state.trainerState.currentScreen = PATIENT_DETAILS;

										for (int i = 0; i < 25; i++) {
											joints[i] = true;
										}
										recordDuration = 20;
									}

									ImGui::InputInt("Duration of Exercise", &recordDuration);

									for (int i = 0; i < 25; i++)
									{
										std::string text = "Joint";
										text.append(std::to_string(i));
										ImGui::Checkbox(text.c_str(), &joints[i]);
									}

									if (ImGui::Button("Start Recording"))
									{
										//send command that starts recording, output should be path of recording, and the 2 exercise create booleans needs to be updated
										sample.startRecording(recordDuration, state.trainerState.exercisePath, state.trainerState.exerciseCreated);
										state.trainerState.exerciseCreation = true;
									}
								}
								else
								{
									// Otherwise do nothing, recording currently in progress
									ImGui::Text("Recording...");
								}
							}
							else
							{
								ImGui::Text("Exercise creation complete.");
								ImGui::InputTextWithHint("Exercise Name", "Enter text here", exerciseName, IM_ARRAYSIZE(exerciseName)); ImGui::SameLine();
								if (ImGui::Button("Assign to patient")) 
								{
									// Assign to patient in DB (in bg thread), change need not be reflected in patient details till refresh button is pressed.
									std::string patientId = state.trainerState.patientDetails->at(state.trainerState.exerciseCreatorIndex).patientID;
									//std::string trainerId = state.trainerState.trainerDetails->trainerID;
									// Exercise name is exerciseName

									state.trainerState.exerciseCreated = false;
									state.trainerState.exerciseCreation = false;
									state.trainerState.currentScreen = PATIENT_DETAILS;

									for (int i = 0; i < 25; i++) {
										joints[i] = true;
									}
									recordDuration = 20;
								}

								if (ImGui::Button("Redo")) 
								{
									remove(state.trainerState.exercisePath.c_str());
									state.trainerState.exerciseCreation = false;
									state.trainerState.exerciseCreated = false;
								}

								if (ImGui::Button("Delete Recording & Exit")) 
								{
									remove(state.trainerState.exercisePath.c_str());
									state.trainerState.exerciseCreated = false;
									state.trainerState.exerciseCreation = false;
									state.trainerState.currentScreen = PATIENT_DETAILS;

									for (int i = 0; i < 25; i++) {
										joints[i] = true;
									}
									recordDuration = 20;
								}
							}

							ImGui::End();
							break;
						}


						case EXERCISE_EXAMINOR:
						{
							//examine completed exercises for a patient, view the analytics and see patient recording and give feedback in a textbox.
						}
					}
				}
			}
		}

		
		// Delegate this action to example's main class
		bool update = sample.update(skeletonColor, jointColor, pointSize, lineWidth, overrideJointColour);

		if (!update)
		{
			// End the work if update failed
			std::cout << "Sample failed to update, ending program" << std::endl;
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

	sample.release();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();

	return 0;
}
