/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2018 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "MultiStreamTester.h"
#include <cmath>
#include <algorithm>
const long double PI = 3.141592653589793238L;

MultiStreamTester::MultiStreamTester(SourceNode* sn)
	: DataThread(sn)
{
	//Add as many streams as needed
	m_waves.add(WaveInfo(5, 30000, 1024, 600));
	m_waves.add(WaveInfo(10, 30000, 1024, 600));
	m_waves.add(WaveInfo(2, 30000, 1024, 600));
	//m_waves.add(WaveInfo(2, 2500, 16, 600));

	unsigned int maxChannels = 0;
	for (int i = 0; i < m_waves.size(); i++)
	{
		sourceBuffers.add(new DataBuffer(m_waves[i].numChannels, 8192));
		if (m_waves[i].numChannels > maxChannels)
			maxChannels = m_waves[i].numChannels;
	}
	m_tmp.malloc(maxChannels);
	m_fakeEvent = 0;
}

MultiStreamTester::~MultiStreamTester()
{

}

bool MultiStreamTester::updateBuffer()
{
	int nStreams = m_waves.size();
	int64 curTime = Time::getHighResolutionTicks();
	for (int s = 0; s < nStreams; s++)
	{
		int64 elapsed = curTime - m_lastTime[s];
		int numSamples = int(float(elapsed) / float(m_ticksPerSecond) * m_waves[s].sampleRate);
		if (numSamples >= m_waves[s].blockNumSamples)
		{
			m_lastTime.set(s, curTime);
			int64 lastSample = m_lastSample[s];
			//std::cout << " stream " << s << " last " << lastSample << " num " << numSamples << std::endl;
			for (int sample = 0; sample < numSamples; sample++)
			{
				int64 curSample = lastSample + sample;
				float value = 1000 * sinf(m_factors[s] * curSample);
				std::fill_n(m_tmp.getData(), m_waves[s].numChannels, value);
				sourceBuffers[s]->addToBuffer(m_tmp.getData(), &curSample, &m_fakeEvent, 1);
			}
			m_lastSample.set(s, lastSample + numSamples);
		}
	}
	wait(20);
	return true;
}

bool MultiStreamTester::foundInputSource()
{
	return true;
}

bool MultiStreamTester::startAcquisition()
{
	m_lastSample.clear();
	m_factors.clear();
	m_lastTime.clear();
	for (int i = 0; i < m_waves.size(); i++)
	{
		sourceBuffers[i]->clear();
	}

	int nStreams = m_waves.size();
	m_lastSample.insertMultiple(0, 0, nStreams);
	m_ticksPerSecond = Time::getHighResolutionTicksPerSecond();

	for (int i = 0; i < nStreams; i++)
	{
		m_factors.add(2 * PI*m_waves[i].frequency / m_waves[i].sampleRate);
	}

	m_lastTime.insertMultiple(0, Time::getHighResolutionTicks(), nStreams);
	startThread();
	
	return true;
}

bool MultiStreamTester::stopAcquisition()
{
	stopThread(200);
	return true;
}

int MultiStreamTester::getNumDataOutputs(DataChannel::DataChannelTypes type, int subProcessorIdx) const
{
	if (type == DataChannel::HEADSTAGE_CHANNEL)
	{
		return m_waves[subProcessorIdx].numChannels;
	}
	else return 0;
}

int MultiStreamTester::getNumTTLOutputs(int subProcessorIdx) const
{
	return 0;
}

float MultiStreamTester::getSampleRate(int subProcessorIdx) const
{
	return m_waves[subProcessorIdx].sampleRate;
}

float MultiStreamTester::getBitVolts(const DataChannel* chan) const
{
	return 0.02;
}


unsigned int MultiStreamTester::getNumSubProcessors() const
{
	return m_waves.size();
}

MultiStreamTester::WaveInfo::WaveInfo(float f, float s, int n, int b)
	: frequency(f), sampleRate(s), numChannels(n), blockNumSamples(b)
{}

MultiStreamTester::WaveInfo::WaveInfo(float f, float s, int n)
	: frequency(f), sampleRate(s), numChannels(n), blockNumSamples(1)
{}