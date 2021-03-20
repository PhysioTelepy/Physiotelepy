#include <string>
#include <vector>
#include "sqlite3/sqlite3.h"
#include "ApplicationState.h"

class DBApi
{
public:
	DBApi();
	~DBApi();

	static sqlite3* db;

	static void InitializeDB();
	static void InsertData(std::string table, std::string columns, std::string data);
	static void SelectData(std::string columns, int num_columns, std::string from, std::string where, std::vector<std::vector<std::string>>& result);
	static void CloseDB();

	//Getters
	
	static bool LoginTherapist(std::string username, std::string password, int &therapistId);
	static void GetTrainerDetails(int trainerId, TrainerDetails *&dt);
	static std::vector<int> GetPatientIdsForTrainer(int trainerId);

	static bool LoginPatient(std::string username, std::string password, int& patientId);
	static int GetTrainerIdForPatient(int patientId);
	static void GetPatientDetails(int patientId, PatientDetails*& dt);
	
	//Setters
	static void CreateNewPatient(std::string username, std::string password, std::string name, int age);
	static void AnalysisForExercise(int exercise_id, std::string analysis, std::string patient_model_uri);

	static void CreateNewTherapist(std::string username, std::string password, std::string name, int age);
	static void AssignExerciseToPatient(std::string name, std::string description, std::string model_uri, int patientId, int therapist_id);
	static void FeedbackForExercise(int exercise_id, std::string feedback);
	
};
