#include <iostream>
#include "NuitrackGL.h"

class DiskHelper final
{
public:
	DiskHelper();
	~DiskHelper();

	static void readDatafromDisk(const std::string &path, std::vector<JointFrame>& buffer);
	static void writeDataToDisk(std::string *path, const std::vector<JointFrame>& buffer);
};