#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer()
	: m_secondsPerCount(0.0), m_deltaTime(-1.0), m_baseTime(0), m_pausedTime(0), m_prevTime(0), m_currentTime(0), m_stopped(false) 
{
	__int64 countsPersec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPersec);
	m_secondsPerCount = 1.0 / (double)countsPersec;
}

float GameTimer::TotalTime() const {
	if (m_stopped) {
		return (float)(((m_stopTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
	}
	else {
		if (m_currentTime != 0) {
			float result = (float)(((m_currentTime - m_pausedTime) - m_baseTime) * m_secondsPerCount);
			return result;
		}
	}
}

float GameTimer::DeltaTime() const {
	return (float)m_deltaTime;
}

void GameTimer::Reset() {
	__int64 currentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);

	m_baseTime = currentTime;
	m_prevTime = currentTime;
	m_stopTime = 0;
	m_stopped = false;
}

void GameTimer::Start() {
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	if (m_stopped) {
		m_pausedTime += (startTime - m_stopTime);
		
		m_prevTime = startTime;
		m_stopTime = 0;
		m_stopped = false;
	}
}

void GameTimer::Stop() {
	if (!m_stopped) {
		__int64 currentTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);

		m_stopTime = currentTime;
		m_stopped = true;
	}
}

void GameTimer::Tick() {
	if (m_stopped) {
		m_deltaTime = 0.0;
		return;
	}

	__int64 currentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);
	m_currentTime = currentTime;

	// Time diff between this fame and the previous
	m_deltaTime = (m_currentTime - m_prevTime) * m_secondsPerCount;

	// Prepare for next frame
	m_prevTime = m_currentTime;

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then mDeltaTime can be negative.
	if (m_deltaTime < 0.0) {
		m_deltaTime = 0.0;
	}
}