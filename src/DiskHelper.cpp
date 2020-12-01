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

		bool time = false;
		bool type = false;
		bool confidence = false;
		bool x = false;
		bool y = false;

		unsigned int p1 = 0;
		unsigned int p2 = 0;

		uint8_t index = 0;

		while (p2 < line.size())
		{
			if (checkType)
			{
				while (line.at(p2) != ',')
				{
					p2++;
				}

				std::string typeString = line.substr(p1, p2 - p1);

				if (typeString.compare("Time") == 0)
				{
					time = true;
				}
				else if (typeString.compare("Type") == 0)
				{
					type = true;
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

				if (time)
				{
					time = false;
					jointFrame.timeStamp = std::stoi(dataString);
				}
				else if (type)
				{
					type = false;
					// Ignore type
				}
				else if (confidence)
				{
					confidence = false;
					jointFrame.confidence[index] = std::stof(dataString);
				}
				else if (x)
				{
					x = false;
					jointFrame.joints[index].x = std::stof(dataString);
				}
				else if (y)
				{
					y = false;
					jointFrame.joints[index].y = std::stof(dataString);
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

void DiskHelper::writeDataToDisk(const std::string& path, const std::vector<JointFrame>& buffer)
{
	int size = 0;
	size += sizeof(std::time_t) * buffer.size();
	size += sizeof(Vector2) * 25 * buffer.size();

	std::cout << "Size of the file is: " << size << " bytes" << std::endl;

	std::ofstream file(path, std::ofstream::trunc);
	for (int j = 0; j < buffer.size(); j++)
	{
		file << "Time," << buffer[j].timeStamp << ",";
		for (int i = 0; i < 25; i++)
			file << "Type," << i << ",Confidence," << buffer[j].confidence[i] << ",x," << buffer[j].joints[i].x << ",y," << buffer[j].joints[i].y << ",";

		file << std::endl;
	}
	file.close();
}
