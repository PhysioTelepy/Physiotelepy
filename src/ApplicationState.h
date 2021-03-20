#include <iostream>
#include <map>

typedef enum { LOGIN_PATIENT, CREATE_PATIENT, EXERCISE_HUB, REPLAY_TRAINER_EXERCISE, PERFORMING_EXERCISE_CORRECTNESS, PERFORMING_EXERCISE_RECORDING, FEEDBACK_HUB} PatientScreen; 
typedef enum { LOGIN_TRAINER, CREATE_TRAINER, PATIENT_DETAILS, EXERCISE_CREATOR, EXERCISE_EXAMINOR} TrainerScreen;

struct PatientDetails {
	int patientID;
	std::string name;
	int age;
	std::map<int, std::pair<std::string, std::string>> assignedExercisesMap; // exerciseID -> name,exercisePath... analysis = null && feedback == null && patient_model_uri == null
	std::map<int, std::tuple<std::string, std::string, std::string>> compeltedExercisesMap; // exerciseID -> (name, analysis, patient model uri), analysis != null && patient_session_model != null && feedback == null
	std::map<int, std::tuple<std::string, std::string, std::string>> completedExercisesWithFeedbackAvailableMap; // exerciseID -> (name, analysis, feedback) analysis != null && patient_session_model != null && feedback != null
};

struct TrainerDetails {
	int trainerID;
	std::string name;
	int age;
	int rating;
};

struct PatientState {
	PatientScreen currentScreen;
	PatientDetails *patientDetails;
	TrainerDetails *trainerDetails;
	bool replayInProgress;
	bool replayComplete;
	bool analysisInProgress;
	bool analysisComplete;
	bool recordInProgress;
	bool recordComplete;
	int currentExerciseKey;
	std::string analysis;
	std::string exercisePath;
};

struct TrainerState {
	TrainerScreen currentScreen;
	TrainerDetails *trainerDetails;
	std::vector<PatientDetails> *patientDetails;
	int exerciseCreatorIndex; // Index of the Patient in the vector for exercise creation
	int exerciseExaminorPatientIndex; // Index of Patient in the vector for exercise examinor
	int exerciseExaminorKey;
	bool exerciseCreated;
	bool exerciseCreation;
	bool replayComplete;
	bool patientReplayInProgress;
	bool patientReplayComplete;
	std::string exercisePath;
};

struct ApplicationState {
	bool mainSelection;
	bool isPatient;
	PatientState patientState;
	TrainerState trainerState;
};