#include "DiskHelper.h"
#include <fstream>

DiskHelper::DiskHelper()
{
}

DiskHelper::~DiskHelper()
{
}

void DiskHelper::readDatafromDisk(const std::string& path, std::vector<JointFrame> &buffer)
{
	std::ifstream file(path);

	if (!file.is_open())
	{
		std::cout << "Cannot open file" << std::endl;
		return;
	}

	std::string line;

	buffer.clear();

	while (std::getline(file, line))
	{
		JointFrame jointFrame;

		bool checkType = true;

		bool requiredJoints = false;
		bool time = false;
		bool type = false;
		bool confidence = false;
		bool x = false;
		bool y = false;
		bool z = false;
		bool x_proj = false;
		bool y_proj = false;
		bool angle = false;

		unsigned int p1 = 0;
		unsigned int p2 = 0;

		uint8_t index = 0;
		uint8_t index2 = 0;

		while (p2 < line.size())
		{
			if (checkType)
			{
				while (line.at(p2) != ',')
				{
					p2++;
				}

				std::string typeString = line.substr(p1, p2 - p1);

				if (typeString.compare("RJ") == 0)
				{
					requiredJoints = true;
				}
				else if (typeString.compare("Time") == 0)
				{
					time = true;
				}
				else if (typeString.compare("Confidence") == 0)
				{
					confidence = true;
				}
				else if (typeString.compare("x") == 0)
				{
					x = true;
				}
				else if (typeString.compare("y") == 0)
				{
					y = true;
				}
				else if (typeString.compare("z") == 0)
				{
					z = true;
				}
				else if (typeString.compare("x_proj") == 0)
				{
					x_proj = true;
				}
				else if (typeString.compare("y_proj") == 0)
				{
					y_proj = true;
				}
				else if (typeString.compare("Angle") == 0) 
				{
					angle = true;
				}
				else {
					std::cout << "Error when trying to read file" << std::endl;
					return;
				}

				p2 = p2 + 1;
				p1 = p2;

				checkType = false;

			}
			else
			{
				while (line.at(p2) != ',')
				{
					p2++;
				}

				std::string dataString = line.substr(p1, p2 - p1);

				if (requiredJoints)
				{
					requiredJoints = false;
					jointFrame.requiredJoints = std::stoi(dataString);
				}
				else if (time)
				{
					time = false;
					jointFrame.timeStamp = std::stoi(dataString);
				}
				else if (confidence)
				{
					confidence = false;
					jointFrame.confidence[index] = std::stof(dataString);
				}
				else if (x)
				{
					x = false;
					jointFrame.realJoints[index].x = std::stof(dataString);
				}
				else if (y)
				{
					y = false;
					jointFrame.realJoints[index].y = std::stof(dataString);
				}
				else if (z)
				{
					z = false;
					jointFrame.realJoints[index].z = std::stof(dataString);
				}
				else if (x_proj)
				{
					x_proj = false;
					jointFrame.relativeJoints[index].x = std::stof(dataString);
				}
				else if (y_proj)
				{
					y_proj = false;
					jointFrame.relativeJoints[index].y = std::stof(dataString);
					jointFrame.relativeJoints[index].z = 0;
				}
				else if (angle)
				{
					angle = false;
					jointFrame.angles[index] = std::stof(dataString);
					index++;
				}
				else {
					std::cout << "Error when trying to read file" << std::endl;
					return;
				}

				p2 = p2 + 1;
				p1 = p2;

				checkType = true;
			}
		}

		buffer.push_back(jointFrame);
	}

	std::cout << "File read into memory" << std::endl;
}

std::string gen_random(const int len) {

	std::string tmp_s;
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	srand((unsigned)time(NULL) * getpid());

	tmp_s.reserve(len);

	for (int i = 0; i < len; ++i)
		tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];

	return tmp_s;
}


void DiskHelper::writeDataToDisk(std::string* path, const std::vector<JointFrame>& buffer)
{

	int size = 0;
	size += sizeof(std::time_t) * buffer.size();
	size += sizeof(Vector2) * 25 * buffer.size();

	std::cout << "Size of the file is: " << size << " bytes" << std::endl;

	std::string salt_path = "C:/dev/JointData/" + gen_random(10) + ".txt";

	*path = salt_path;

	std::ofstream file(salt_path, std::ofstream::trunc);
	

	for (int j = 0; j < buffer.size(); j++)
	{
		file << "Time," << buffer[j].timeStamp << ",";
		file << "RJ," << buffer[j].requiredJoints << ",";

		for (int i = 0; i < 25; i++) {
			file << "Confidence," << buffer[j].confidence[i] << ",x," << buffer[j].realJoints[i].x << ",y," << buffer[j].realJoints[i].y << ",z," << buffer[j].realJoints[i].z << ",";
			file << "x_proj," << buffer[j].relativeJoints[i].x << ",y_proj," << buffer[j].relativeJoints[i].y << ",";
			file << "Angle," << buffer[j].angles[i] << ",";
		}

		file << std::endl;
	}

	file.close();

	std::cout << "File written to disk at: " << *path << std::endl;
}
