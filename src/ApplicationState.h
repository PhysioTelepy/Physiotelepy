#include <iostream>

typedef enum { LOGIN_PATIENT, EXERCISE_HUB, PERFORMING_EXERCISE, FEEDBACK_HUB} PatientScreen; 
typedef enum { LOGIN_TRAINER, PATIENT_DETAILS, EXERCISE_CREATOR, EXERCISE_EXAMINOR} TrainerScreen;

struct PatientDetails {
	std::string patientID;
	std::string name;
	int age;
	std::map<std::string, std::string> assignedExercisesMap;
	std::map<std::string, std::pair<std::string, std::string>> compeltedExercisesMap;
};

struct TrainerDetails {
	std::string trainerID;
	std::string name;
	int age;
	int rating;
};

struct PatientState {
	PatientScreen currentScreen;
	PatientDetails *details;
	TrainerDetails *trainerDetails;
	bool exerciseInProgress;
	std::string currentExercise;
};

struct TrainerState {
	TrainerScreen currentScreen;
	TrainerDetails *trainerDetails;
	std::vector<PatientDetails> *patientDetails;
	int exerciseCreatorIndex; // Index of the Patient in the vector for exercise created;
	bool exerciseCreated;
	bool exerciseCreation;
	std::string exercisePath;
};

struct ApplicationState {
	bool mainSelection;
	bool isPatient;
	PatientState patientState;
	TrainerState trainerState;
};