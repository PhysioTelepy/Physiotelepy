#include <string>
#include <vector>
#include <iostream>

#include "DBApi.h"
#include "sqlite3/sqlite3.h"

DBApi::DBApi() 
{
}

DBApi::~DBApi()
{
}

sqlite3* DBApi::db;

void DBApi::InitializeDB() {
	sqlite3_open("C:/Physiotelepy/Database/Physiotelepy.db", &db);

	sqlite3_stmt* createStmt;
	std::string query = "CREATE TABLE IF NOT EXISTS Therapist (\
		therapist_id INTEGER PRIMARY KEY AUTOINCREMENT,\
		username TEXT UNIQUE NOT NULL,\
		password TEXT NOT NULL,\
		name TEXT NOT NULL,\
		rating INT NULL,\
		age INT NOT NULL\
		); ";
	sqlite3_prepare(db, query.c_str(), query.size(), &createStmt, NULL);
	if (sqlite3_step(createStmt) != SQLITE_DONE) std::cout << "Table creation failed!" << std::endl;
	sqlite3_finalize(createStmt);

	query = "CREATE TABLE IF NOT EXISTS Patient (\
		patient_id INTEGER PRIMARY KEY AUTOINCREMENT,\
		therapist_id INT NOT NULL,\
		username TEXT UNIQUE NOT NULL,\
		password TEXT NOT NULL,\
		name TEXT NOT NULL,\
		age INT NOT NULL\
		); ";
	sqlite3_prepare(db, query.c_str(), query.size(), &createStmt, NULL);
	if (sqlite3_step(createStmt) != SQLITE_DONE) std::cout << "Table creation failed!" << std::endl;
	sqlite3_finalize(createStmt);

	query = "CREATE TABLE IF NOT EXISTS Exercise (\
		exercise_id INTEGER PRIMARY KEY AUTOINCREMENT,\
		name TEXT NOT NULL,\
		description TEXT NOT NULL,\
		model_uri TEXT NOT NULL,\
		therapist_id INT NOT_NULL,\
		patient_id INT NON_NULL,\
		session_analytics TEXT NULL,\
		therapist_feedback TEXT NULL,\
		patient_session_model_uri TEXT NULL\
		); ";

	sqlite3_prepare(db, query.c_str(), query.size(), &createStmt, NULL);
	if (sqlite3_step(createStmt) != SQLITE_DONE) std::cout << "Table creation failed!" << std::endl;
	sqlite3_finalize(createStmt);
}

void DBApi::CloseDB() {
	sqlite3_close(db);
}

void DBApi::InsertData(std::string table, std::string columns, std::string data) {
	sqlite3_stmt* insertStmt;
	std::string query = "INSERT INTO " + table + "(" + columns + ")" \
		" VALUES " + data + \
		";";

	sqlite3_prepare(db, query.c_str(), query.size(), &insertStmt, NULL);
	if (sqlite3_step(insertStmt) != SQLITE_DONE) std::cout << "Insertion into " + table + " failed!" << std::endl;
	sqlite3_finalize(insertStmt);
}

void DBApi::SelectData(std::string columns, int num_columns, std::string table, std::string predicate, std::vector<std::vector<std::string>>& result)
{
	sqlite3_stmt* selectStmt;
	std::string query;
	if (predicate != "")
	{
		query = "SELECT " + columns + \
			" FROM " + table + \
			" WHERE " + predicate + \
			";";
	}
	else
	{
		query = "SELECT " + columns + \
			" FROM " + table + \
			";";
	}
	

	std::cout << query << std::endl;

	sqlite3_prepare(db, query.c_str(), query.size(), &selectStmt, NULL);

	while (sqlite3_step(selectStmt) != SQLITE_DONE)
	{
		std::vector<std::string> entry;
		for (int i = 0; i < num_columns; i++)
		{	
			const unsigned char* c = sqlite3_column_text(selectStmt, i);
			if (c == NULL)
			{
				entry.push_back("");
			}
			else
			{
				entry.push_back(std::string(reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, i))));
			}
		}
		result.push_back(entry);
	}

	sqlite3_finalize(selectStmt);
}

void DBApi::CreateNewTherapist(std::string username, std::string password, std::string name, int age)
{
	std::string table = "Therapist";
	std::string columns = "username, password, name, age";
	std::string data = "(" + std::string("'") + username + "'" + ", " + "'" + password + "'" + ", " + "'" + name + "'" + ", " + std::to_string(age) + ")";

	InsertData(table, columns, data);
}

bool DBApi::LoginTherapist(std::string username, std::string password, int &therapistId)
{
	std::string table = "Therapist";
	std::string columns = "therapist_id";
	std::string predicate = "username='" + username + "' AND password='" + password + "'";

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 1, table, predicate, res);

	for (int i = 0; i < res.size(); i++)
	{
		therapistId = std::stoi(res[i][0]);
		return true;
	}

	return false;
}

void DBApi::GetTrainerDetails(int trainerId, TrainerDetails *&dt)
{
	std::string table = "Therapist";
	std::string columns = "name, age, rating, therapist_id";
	std::string predicate = "therapist_id = " + std::to_string(trainerId);

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 4, table, predicate, res);

	dt->name = res[0][0];
	dt->age = std::stoi(res[0][1]);
	dt->trainerID = std::stoi(res[0][3]);
}

std::vector<int> DBApi::GetPatientIdsForTrainer(int trainerId)
{
	std::string table = "Patient";
	std::string columns = "patient_id";
	std::string predicate = "therapist_id = " + std::to_string(trainerId);

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 1, table, predicate, res);

	std::vector<int> retVals;

	for (int i = 0; i < res.size(); i++)
	{
		retVals.push_back(std::stoi(res[i][0]));
	}

	return retVals;
}

int DBApi::GetTrainerIdForPatient(int patientId)
{
	std::string table = "Patient";
	std::string columns = "therapist_id";
	std::string predicate = "patient_id = " + std::to_string(patientId);

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 1, table, predicate, res);

	return std::stoi(res[0][0]);
}

void DBApi::GetPatientDetails(int patientId, PatientDetails*& dt)
{
	std::string table = "Patient";
	std::string columns = "therapist_id, patient_id, name, age";
	std::string predicate = "patient_id = " + std::to_string(patientId);

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 4, table, predicate, res);

	int therapist_id = std::stoi(res[0][0]);
	dt->patientID = std::stoi(res[0][1]);
	dt->name = res[0][2];
	dt->age = std::stoi(res[0][3]);

	table = "Exercise";
	columns = "exercise_id, name, description, model_uri, session_analytics, therapist_feedback, patient_session_model_uri";
	predicate = "therapist_id =" + std::to_string(therapist_id) + " AND patient_id=" + std::to_string(patientId);

	res = std::vector<std::vector<std::string>>();
	SelectData(columns, 7, table, predicate, res);

	for (int i = 0; i < res.size(); i++)
	{
		int exerciseId = std::stoi(res[i][0]);
		std::string name = res[i][1];
		std::string description = res[i][2];
		std::string model_uri = res[i][3];


		std::string sessionAnalytics = res[i][4];
		std::string therapist_feedback = res[i][5];
		std::string patient_session_model_uri = res[i][6];

		if (sessionAnalytics == "" && therapist_feedback == "" && patient_session_model_uri == "")
		{
			dt->assignedExercisesMap.insert(std::pair<int, std::pair<std::string, std::string>>(exerciseId, std::pair<std::string, std::string>(name, model_uri)));
		}

		else if (sessionAnalytics != "" && patient_session_model_uri != "" && therapist_feedback == "")
		{
			dt->compeltedExercisesMap.insert(std::pair<int, std::tuple<std::string, std::string, std::string>>(exerciseId, std::make_tuple(name, sessionAnalytics, patient_session_model_uri)));
		}

		else if (sessionAnalytics != "" && patient_session_model_uri != "" && therapist_feedback != "")
		{
			dt->completedExercisesWithFeedbackAvailableMap.insert(std::pair<int, std::tuple<std::string, std::string, std::string>>(exerciseId, std::make_tuple(name, sessionAnalytics, therapist_feedback)));
		}

		else
		{
			throw "Weird DB state";
		}
	}

}

void DBApi::CreateNewPatient(std::string username, std::string password, std::string name, int age)
{
	std::string table = "Therapist";
	std::string columns = "therapist_id";

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 1, table, "", res);

	if (res.size() == 0)
	{
		throw "Create a therapist first";
	}

	int therapist_id = std::stoi(res[std::rand() % res.size()][0]);

	table = "Patient";
	columns = "therapist_id, username, password, name, age";
	std::string data = "(" + std::to_string(therapist_id) + ", " + std::string("'") + username + "'" + ", " + "'" + password + "'" + ", " + "'" + name + "'" + ", " + std::to_string(age) + ")";

	InsertData(table, columns, data);
}

bool DBApi::LoginPatient(std::string username, std::string password, int& patientId)
{
	std::string table = "Patient";
	std::string columns = "patient_id";
	std::string predicate = "username='" + username + "' AND password='" + password + "'";

	std::vector<std::vector<std::string>> res;
	SelectData(columns, 1, table, predicate, res);

	for (int i = 0; i < res.size(); i++)
	{
		patientId = std::stoi(res[i][0]);
		return true;
	}

	return false;
}

void DBApi::AssignExerciseToPatient(std::string name, std::string description, std::string model_uri, int patientId, int therapist_id)
{
	std::string table = "Exercise";
	std::string columns = "name, description, model_uri, therapist_id, patient_id";
	std::string data = "('" + name + "', '" + description + "', '"  + model_uri + "', " + std::to_string(patientId) + ", " + std::to_string(therapist_id) + ")";
	
	InsertData(table, columns, data);
}

void DBApi::FeedbackForExercise(int exercise_id, std::string feedback)
{
	sqlite3_stmt* modifyStmt;
	std::string query = "UPDATE Exercise SET therapist_feedback = '" + feedback + "' WHERE exercise_id = " + std::to_string(exercise_id) + ";";
	std::cout << query << std::endl;

	sqlite3_prepare(db, query.c_str(), query.size(), &modifyStmt, NULL);
	if (sqlite3_step(modifyStmt) != SQLITE_DONE) std::cout << "Insertion into Exercise failed!" << std::endl;
	sqlite3_finalize(modifyStmt);
}

void DBApi::AnalysisForExercise(int exercise_id, std::string analysis, std::string patient_model_uri)
{
	sqlite3_stmt* modifyStmt;
	std::string query = "UPDATE Exercise SET session_analytics = '" + analysis + "', patient_session_model_uri = '" + patient_model_uri + "' WHERE exercise_id = " + std::to_string(exercise_id) + ";";
	std::cout << query << std::endl;

	sqlite3_prepare(db, query.c_str(), query.size(), &modifyStmt, NULL);
	if (sqlite3_step(modifyStmt) != SQLITE_DONE) std::cout << "Insertion into Exercise failed!" << std::endl;
	sqlite3_finalize(modifyStmt);
}
