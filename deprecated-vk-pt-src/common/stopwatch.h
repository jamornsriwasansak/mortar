#pragma once

#include <ctime>
#include <chrono>

struct StopWatch
{
	std::chrono::high_resolution_clock::time_point m_start_time;

	StopWatch()
	{
		reset();
	}

	void reset()
	{
		m_start_time = std::chrono::high_resolution_clock::now();
	}

	long long timeMilliSec()
	{
		std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_start_time).count();
	}

	long long timeNanoSec()
	{
		std::chrono::high_resolution_clock::time_point end_time = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - m_start_time).count();
	}
};