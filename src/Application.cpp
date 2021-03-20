
#include "NuitrackGL.h"
#include "GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include <iostream>
#include <fstream>
#include <direct.h>
#include <Windows.h>

#include <ios>
#include "sqlite3/sqlite3.h"
#include "DBApi.h"

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

ApplicationState state;

int main(int argc, char* argv[])
{

	DBApi::InitializeDB();

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
	char userNameCreate[128] = "";
	char password[128] = "";
	char passwordCreate[128] = "";
	char exerciseName[128] = "";
	char feedback[256] = "";
	int age = 18;
	char name[128] = "";

	const char* items[] = { "Upper Body", "Lower Body", "Full Body" };
	static int item_current = 1;

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
		
		{
			if (!state.mainSelection) 
			{
				ImGui::Begin("Are you a therapist or a Patient?");
				if (ImGui::Button("Physiotherapist")) 
				{
					state.mainSelection = true;
					state.isPatient = false;
					state.trainerState.currentScreen = LOGIN_TRAINER;
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
								if (userName != "" && password != "")
								{
									PatientDetails* pd = new PatientDetails();
									TrainerDetails* td = new TrainerDetails();

									std::string un = std::string(userName);
									std::string pw = std::string(password);

									bool login = DBApi::LoginPatient(un, pw, pd->patientID);

									if (login)
									{
										int trainerId = DBApi::GetTrainerIdForPatient(pd->patientID);
										DBApi::GetTrainerDetails(trainerId, td);
										DBApi::GetPatientDetails(pd->patientID, pd);
										state.patientState.trainerDetails = td;
										state.patientState.patientDetails = pd;
										state.patientState.currentScreen = EXERCISE_HUB;
									}
								}
							}

							ImGui::SameLine();

							if (ImGui::Button("Create New Account"))
							{
								state.patientState.currentScreen = CREATE_PATIENT;
							}

							if (ImGui::Button("Back")) {
								state.mainSelection = false;
							}
							ImGui::End();
							break;
						}

						case CREATE_PATIENT:
						{
							ImGui::Begin("Create account");
							ImGui::InputTextWithHint("User Name", "Enter text here", userNameCreate, IM_ARRAYSIZE(userNameCreate));
							ImGui::InputTextWithHint("Password", "Enter text here", passwordCreate, IM_ARRAYSIZE(passwordCreate));
							ImGui::InputTextWithHint("Name", "Enter text here", name, IM_ARRAYSIZE(name));
							ImGui::SliderInt("Age", &age, 18, 60);

							if (ImGui::Button("Create"))
							{
								if (passwordCreate != "" && userNameCreate != "" && name != "")
								{
									DBApi::CreateNewPatient(std::string(userNameCreate), std::string(passwordCreate), std::string(name), age);
									state.patientState.currentScreen = LOGIN_PATIENT;
									userNameCreate[0] = '\0';
									passwordCreate[0] = '\0';
								}
							}

							if (ImGui::Button("Back"))
							{
								state.patientState.currentScreen = LOGIN_PATIENT;
								userNameCreate[0] = '\0';
								passwordCreate[0] = '\0';
							}
							ImGui::End();
							break;
						}
						
						case EXERCISE_HUB:
						{
							ImGui::Begin("Exercise Hub");

							if (ImGui::Button("Refresh"))
							{
								DBApi::GetTrainerDetails(state.patientState.trainerDetails->trainerID, state.patientState.trainerDetails);
								DBApi::GetPatientDetails(state.patientState.patientDetails->patientID, state.patientState.patientDetails);
							}
							
							ImGui::Text("Assigned Exercises............");
							for (auto it = state.patientState.patientDetails->assignedExercisesMap.begin(); it != state.patientState.patientDetails->assignedExercisesMap.end(); ++it)
							{
								int key = it->first;
								std::pair<std::string, std::string> value = it->second;

								ImGui::Text(value.first.c_str()); ImGui::SameLine();
								if (ImGui::Button("Perform Exercise")) {
									state.patientState.currentExerciseKey = key;
									
									state.patientState.analysisInProgress = false;
									state.patientState.analysisComplete = false;
									state.patientState.replayInProgress = false;
									state.patientState.replayComplete = false;
									state.patientState.recordInProgress = false;
									state.patientState.recordComplete = false;
									state.patientState.currentScreen = REPLAY_TRAINER_EXERCISE;
									sample.displayVideo(false, true);
								}
							}

							ImGui::Text("Completed Exercises............");
							for (auto it = state.patientState.patientDetails->compeltedExercisesMap.begin(); it != state.patientState.patientDetails->compeltedExercisesMap.end(); ++it)
							{
								int key = it->first;
								std::tuple<std::string, std::string, std::string> value = it->second;

								ImGui::Text(std::get<0>(value).c_str());
							}

							ImGui::Text("Completed Exercises With Feedback");
							for (auto it = state.patientState.patientDetails->completedExercisesWithFeedbackAvailableMap.begin(); it != state.patientState.patientDetails->completedExercisesWithFeedbackAvailableMap.end(); ++it)
							{
								int key = it->first;
								std::tuple<std::string, std::string, std::string> value = it->second;

								ImGui::Text(std::get<0>(value).c_str()); ImGui::SameLine();
								if (ImGui::Button("View feedback"))
								{
									state.patientState.currentExerciseKey = key;
									state.patientState.currentScreen = FEEDBACK_HUB;
								}	
							}

							ImGui::Text("Completed Exercises...........");
							// List of compelted exercises

							if (ImGui::Button("Logout")) {
								sample.displayVideo(false, false);
								userName[0] = '\0';
								password[0] = '\0';
								delete state.patientState.patientDetails;
								delete state.patientState.trainerDetails;
								state.patientState.patientDetails = NULL;
								state.patientState.trainerDetails = NULL;
								state.patientState.currentScreen = LOGIN_PATIENT;
								sample.displayVideo(false, false);
							}
							ImGui::End();
							break;
						}

						case REPLAY_TRAINER_EXERCISE:
						{
							ImGui::Begin("Trainer replay");
							ImGui::Text("Watch trainer performing the exercise.");

							if (!state.patientState.replayInProgress)
							{
								sample.displayVideo(false, true);
								std::string path = state.patientState.patientDetails->assignedExercisesMap.at(state.patientState.currentExerciseKey).second;
								state.patientState.replayInProgress = true;
								state.patientState.replayComplete = false;
								sample.loadDiskToBuffer(path);
								sample.replayLoadedData(state.patientState.replayComplete);
							}

							else if (state.patientState.replayComplete)
							{
								state.patientState.replayInProgress = false;
								state.patientState.replayComplete = false;
								state.patientState.analysisInProgress = false;
								state.patientState.analysisComplete = false;
								state.patientState.currentScreen = PERFORMING_EXERCISE_CORRECTNESS;
								sample.displayVideo(true, true);
							}

							ImGui::End();

							break;
						}

						case PERFORMING_EXERCISE_CORRECTNESS:
						{
							ImGui::Begin("Correctness Analyzer");
							if (!state.patientState.analysisInProgress) 
							{
								ImGui::Text("Press the button below to start performing the exercise");
								if (ImGui::Button("Start"))
								{
									state.patientState.analysisInProgress = true;
									state.patientState.analysisComplete = false;
									sample.analyzeLoadedData(state.patientState.analysisComplete, state.patientState.analysis);
									sample.displayVideo(true, true);
									
								}
							}
							else if (!state.patientState.analysisComplete)
							{
								ImGui::Text("Follow along the trainer");
								// Some more explanation text
								ImGui::Text("Analysis in progress...");
							}
							else if (state.patientState.analysisComplete) {
								// Analysis stored in analysis path
								state.patientState.analysisInProgress = false;
								state.patientState.analysisComplete = false;
								state.patientState.recordInProgress = false;
								state.patientState.recordComplete = false;
								state.patientState.currentScreen = PERFORMING_EXERCISE_RECORDING;
								sample.displayVideo(true, true);
							}
							ImGui::End();
							break;
						}

						case PERFORMING_EXERCISE_RECORDING:
						{
							ImGui::Begin("Record Exercise");

							if (!state.patientState.recordInProgress)
							{
								sample.displayVideo(true, true);
								ImGui::Text("Now that you know how to perform the exercise");
								ImGui::Text("Press the button below to start a recording of you performing the exercise");
								ImGui::Text("This will be sent to the Physiotherapist for analysis");

								ImGui::SliderInt("Duration", &recordDuration, 20, 60);

								if (ImGui::Button("Start Recording")) {
									state.patientState.recordInProgress = true;
									state.patientState.recordComplete = false;
									sample.startRecording(recordDuration, state.patientState.exercisePath, state.patientState.recordComplete, 0);
									sample.displayVideo(true, true);
								}
							}

							else if (state.patientState.recordComplete)
							{
								sample.displayVideo(false, false);

								if (ImGui::Button("Send recording")) 
								{
									DBApi::AnalysisForExercise(state.patientState.currentExerciseKey, state.patientState.analysis, state.patientState.exercisePath);

									state.patientState.analysisInProgress = false;
									state.patientState.analysisComplete = false;
									state.patientState.replayComplete = false;
									state.patientState.replayComplete = false;
									state.patientState.recordInProgress = false;
									state.patientState.recordComplete = false;

									state.patientState.currentScreen = EXERCISE_HUB;

									sample.displayVideo(false, false);

								}

								if (ImGui::Button("Redo recording")) {
									state.patientState.recordInProgress = false;
									state.patientState.recordComplete = false;
									sample.displayVideo(true, true);
								}
							}

							ImGui::End();

							break;
						}

						case FEEDBACK_HUB:
						{
							ImGui::Begin("Feedback Hub");

							ImGui::Text("Feedback");
							ImGui::Text(std::get<2>(state.patientState.patientDetails->completedExercisesWithFeedbackAvailableMap.at(state.patientState.currentExerciseKey)).c_str());
							if (ImGui::Button("Go Back"))
							{
								state.patientState.currentScreen = EXERCISE_HUB;
							}
							ImGui::End();
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

								int id;
								bool loggedIn = DBApi::LoginTherapist(std::string(userName), std::string(password), id);

								if (loggedIn == true)
								{
									state.trainerState.trainerDetails = new TrainerDetails();
									DBApi::GetTrainerDetails(id, state.trainerState.trainerDetails);

									std::vector<int> patientIds = DBApi::GetPatientIdsForTrainer(state.trainerState.trainerDetails->trainerID);
									state.trainerState.patientDetails = new std::vector<PatientDetails>();

									for (int patientId : patientIds)
									{
										PatientDetails pd;
										PatientDetails* pointer = &pd;
										DBApi::GetPatientDetails(patientId, pointer);
										state.trainerState.patientDetails->push_back(pd);
									}

									state.trainerState.currentScreen = PATIENT_DETAILS;
									sample.displayVideo(false, false);
								}
							}

							ImGui::SameLine();

							if (ImGui::Button("Create New Account"))
							{
								state.trainerState.currentScreen = CREATE_TRAINER;
							}
							if (ImGui::Button("Back")) {
								state.mainSelection = false;
							}
							ImGui::End();
							break;
						}

						case CREATE_TRAINER:
						{
							ImGui::Begin("Create account");
							ImGui::InputTextWithHint("User Name", "Enter text here", userNameCreate, IM_ARRAYSIZE(userNameCreate));
							ImGui::InputTextWithHint("Password", "Enter text here", passwordCreate, IM_ARRAYSIZE(passwordCreate));
							ImGui::InputTextWithHint("Name", "Enter text here", name, IM_ARRAYSIZE(name));
							ImGui::SliderInt("Age", &age, 18, 60);

							if (ImGui::Button("Create"))
							{
								if (passwordCreate != "" && userNameCreate != "" && name != "")
								{
									DBApi::CreateNewTherapist(std::string(userNameCreate), std::string(passwordCreate), std::string(name), age);
									state.trainerState.currentScreen = LOGIN_TRAINER;
									userNameCreate[0] = '\0';
									passwordCreate[0] = '\0';
								}
							}

							if (ImGui::Button("Back"))
							{
								state.trainerState.currentScreen = LOGIN_TRAINER;
								userNameCreate[0] = '\0';
								passwordCreate[0] = '\0';
							}
							ImGui::End();
							break;
						}

						case PATIENT_DETAILS:
						{
							ImGui::Begin("Patient Details");
							if (ImGui::Button("Refresh")) 
							{
								DBApi::GetTrainerDetails(state.trainerState.trainerDetails->trainerID, state.trainerState.trainerDetails);

								std::vector<int> patientIds = DBApi::GetPatientIdsForTrainer(state.trainerState.trainerDetails->trainerID);
								state.trainerState.patientDetails = new std::vector<PatientDetails>();

								for (int patientId : patientIds)
								{
									PatientDetails pd;
									PatientDetails* pointer = &pd;
									DBApi::GetPatientDetails(patientId, pointer);
									state.trainerState.patientDetails->push_back(pd);
								}

								state.trainerState.currentScreen = PATIENT_DETAILS;
							}
							
							for (auto it = state.trainerState.patientDetails->begin(); it != state.trainerState.patientDetails->end(); it++) 
							{
								PatientDetails patientDetails = *it;
								int index = it - state.trainerState.patientDetails->begin();
								std::string title = "Patient ";
								title.append(std::to_string(index + 1));
								title.append(".................");
								ImGui::Text(title.c_str());
								
								ImGui::Text(("Name - " + patientDetails.name).c_str());
								if (ImGui::Button("Create & Assign an exercise")) {
									state.trainerState.currentScreen = EXERCISE_CREATOR;
									state.trainerState.exerciseCreatorIndex = index;
									state.trainerState.exerciseCreated = false;
									state.trainerState.exerciseCreation = false;

									sample.displayVideo(true, true);
								}
								ImGui::Text("Assigned Exercises");
								for (auto it2 = patientDetails.assignedExercisesMap.begin(); it2 != patientDetails.assignedExercisesMap.end(); ++it2)
								{
									int key = it2->first;
									std::pair<std::string, std::string> value = it2->second;

									ImGui::Text(value.first.c_str());
								}

								ImGui::Text("Completed Exercises");
								int index2 = 0;
								for (auto it2 = patientDetails.compeltedExercisesMap.begin(); it2 != patientDetails.compeltedExercisesMap.end(); ++it2) 
								{
									int key = it2->first;
									std::tuple<std::string, std::string, std::string> value = it2->second;
									
									ImGui::Text(std::get<0>(value).c_str()); ImGui::SameLine();
									if (ImGui::Button("Analysis and Feedback"))
									{
										state.trainerState.currentScreen = EXERCISE_EXAMINOR;
										state.trainerState.exerciseExaminorKey = key;
										state.trainerState.patientReplayInProgress = false;
										state.trainerState.patientReplayComplete = false;
										sample.displayVideo(false, true);
									};
									index++;
								}

								ImGui::Text("Completed & Feedback Provided");
								for (auto it2 = patientDetails.completedExercisesWithFeedbackAvailableMap.begin(); it2 != patientDetails.completedExercisesWithFeedbackAvailableMap.end(); ++it2)
								{
									int key = it2->first;
									auto value = it2->second;

									ImGui::Text(std::get<0>(value).c_str());
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
								sample.displayVideo(false, false);

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
									sample.displayVideo(true, true);
									if (ImGui::Button("Back")) {
										state.trainerState.currentScreen = PATIENT_DETAILS;

										for (int i = 0; i < 25; i++) {
											joints[i] = true;
										}
										recordDuration = 20;
									}

									ImGui::InputInt("Duration of Exercise", &recordDuration);

									ImGui::Text("What type of exercise?"); ImGui::SameLine();
									ImGui::ListBox("", &item_current, items, IM_ARRAYSIZE(items), 1);

									if (ImGui::Button("Start Recording"))
									{
										//send command that starts recording, output should be path of recording, and the 2 exercise create booleans needs to be updated
										sample.startRecording(recordDuration, state.trainerState.exercisePath, state.trainerState.exerciseCreated, item_current + 1);
										state.trainerState.exerciseCreation = true;
										state.trainerState.replayComplete = true;
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
								if (!state.trainerState.replayComplete)
								{
									ImGui::Text("Replay in progress");
									ImGui::End();
									break;
								}

								ImGui::Text("Exercise creation complete.");
								ImGui::InputTextWithHint("Exercise Name", "Enter text here", exerciseName, IM_ARRAYSIZE(exerciseName)); ImGui::SameLine();

								if (ImGui::Button("Assign to patient"))
								{
									sample.displayVideo(false, false);
									int patientId = state.trainerState.patientDetails->at(state.trainerState.exerciseCreatorIndex).patientID;
	
									DBApi::AssignExerciseToPatient(exerciseName, "Test Description", state.trainerState.exercisePath, patientId, state.trainerState.trainerDetails->trainerID);

									state.trainerState.exerciseCreated = false;
									state.trainerState.exerciseCreation = false;
									state.trainerState.currentScreen = PATIENT_DETAILS;

									for (int i = 0; i < 25; i++) {
										joints[i] = true;
									}
									recordDuration = 20;
								}

								if (ImGui::Button("Re-play recording"))
								{
									sample.displayVideo(false, true);
									state.trainerState.replayComplete = false;
									sample.replayRecording(state.trainerState.replayComplete);
								}

								if (ImGui::Button("Redo")) 
								{
									remove(state.trainerState.exercisePath.c_str());
									state.trainerState.exerciseCreation = false;
									state.trainerState.exerciseCreated = false;
									sample.displayVideo(true, true);
								}

								if (ImGui::Button("Delete Recording & Exit")) 
								{
									remove(state.trainerState.exercisePath.c_str());
									state.trainerState.exerciseCreated = false;
									state.trainerState.exerciseCreation = false;
									state.trainerState.currentScreen = PATIENT_DETAILS;
									sample.displayVideo(false, false);

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

							ImGui::Begin("Exercise Examinor");

							PatientDetails* patientDetails = &state.trainerState.patientDetails->at(state.trainerState.exerciseCreatorIndex);

							if (!state.trainerState.patientReplayInProgress)
							{
								if (ImGui::Button("Start Patient Replay")) {
									state.trainerState.patientReplayInProgress = true;
									sample.loadDiskToBuffer(std::get<2>(patientDetails->compeltedExercisesMap.at(state.trainerState.exerciseExaminorKey)));
									sample.replayLoadedData(state.trainerState.patientReplayComplete);
								}
							}

							else if (!state.trainerState.patientReplayComplete)
							{
								ImGui::Text("Patient exercise in progress");
							}

							else if (state.trainerState.patientReplayComplete)
							{
								ImGui::Text("Analysis");
								ImGui::Text(std::get<1>(patientDetails->compeltedExercisesMap.at(state.trainerState.exerciseExaminorKey)).c_str());

								ImGui::InputTextWithHint("Feedback", "Enter", feedback, IM_ARRAYSIZE(feedback));

								if (ImGui::Button("Submit"))
								{
									state.trainerState.currentScreen = PATIENT_DETAILS;
									state.trainerState.patientReplayComplete = false;

									DBApi::FeedbackForExercise(state.trainerState.exerciseExaminorKey, std::string(feedback));
								}
							}

							ImGui::End();
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

	DBApi::CloseDB();
	sample.release();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();

	return 0;
}
