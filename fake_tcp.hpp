#pragma once
#include <iostream>

#include <string>
#include <random>
#include <ctime>

class FakeTcp
{
using uint = unsigned int;
public:
	FakeTcp(std::string msg)
		: index_(0)
		, msg_(msg)
	{
		srand(std::time(NULL));
	}

	int Read(char* buff, uint bufferSize)
	{
		if (index_ == -1)
			return index_;

		int readable = std::rand() % bufferSize;

		int left = msg_.size() - index_;
		if (readable >= left)
			readable = left;

		for (int i = 0; i != readable; i++)
		{
			buff[i] = msg_[index_ + i];
		}

		index_ += readable;
		if (index_ >= msg_.size())
			index_ = -1;

		return readable;
	}
private:
	std::string  msg_;
	uint		 index_;
};

//#define ON
void TestFakeTcp()
{
	const char* mfg = "1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";
	const char* msg = "1234567890qwert";

	FakeTcp ftcp(msg);

	const int BUFF_SIZE = 10;
	char buffer[BUFF_SIZE] = { 0 };

	while (true)
	{
		int size = ftcp.Read(buffer, BUFF_SIZE-1);
		if (size == -1)
		{
			std::cout << "read end" << std::endl;
			break;
		}
#ifdef ON
		buffer[size] = 0;
#endif
		std::cout << "Read from fake tcp, size:"<< size << ",msg:" << buffer << std::endl;
	}
}