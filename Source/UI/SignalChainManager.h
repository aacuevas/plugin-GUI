/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

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

#ifndef __SIGNALCHAINMANAGER_H_948769B9__
#define __SIGNALCHAINMANAGER_H_948769B9__

#include "../../JuceLibraryCode/JuceHeader.h"
#include "../AccessClass.h"
#include <vector>

class GenericProcessor;
class Port;
class OutPort;
class InPort;

/**
	Class that holds connections to other elements of the graph
*/
class Port
{
public:
	Port();
	virtual ~Port();
	Port* getConnection() const;
	virtual unsigned int getNumChannels() const = 0;

	Port(const Port&) = delete;
	Port& operator=(const Port&) = delete;
	Port(Port&&) noexcept;
	Port& operator=(Port&&) noexcept;

protected:
	/*
		Connects the port to another. This disconnects the destination port from any other port it might be connected
		Returns the port the destination port was connected to if any, nullptr if it was unconnected
	*/
	Port* connect(Port* dest);
	Port* disconnect();

private:
	Port* m_connection;
};

/**
Output port for a graph element
*/
class OutPort
	: public Port
{
public:
	OutPort(unsigned int numChannels);
	virtual ~OutPort();
	unsigned int getNumChannels() const override;
	void updateChannelCount(unsigned int numChannels);
	InPort* connect(InPort* dest);
	InPort* disconnect();
private:
	unsigned int m_numChannels;
};

/** 
	Input port for a graph element
*/

class InPort
	: public Port
{
public:
	InPort();
	virtual ~InPort();
	unsigned int getNumChannels() const override;
	OutPort* connect(OutPort* dest);
	OutPort* disconnect();

	virtual bool acceptsConnections() const;
};

/**
	Dummy input port for source nodes
*/
class SourcePort
	: public InPort
{
public:
	SourcePort();
	virtual ~SourcePort();
	bool acceptsConnections() const override;
};

/**
	Graph element that points to each processor
*/
class SignalElement
{
public:
	SignalElement(GenericProcessor* proc);
	~SignalElement();
	unsigned int getInPorts() const;
	unsigned int getOutPorts() const;
	InPort* getInPort(unsigned int) const;
	OutPort* getOutPort(unsigned int) const;
	void update();

private:
	const GenericProcessor const* m_processor;
	OwnedArray<InPort> m_inputPorts;
	OwnedArray<OutPort> m_outputPorts;
};

enum RelativeProcessorPosition { AFTER, BEFORE };

/**

  Provides helper functions for editing the signal chain.

  Created and owned by the EditorViewport.

  @see EditorViewport.

*/
class SignalChainManager
{
public:
	SignalChainManager(EditorViewport* ev);
	~SignalChainManager();

	//Assorted methods to manipulate the signal chain

	void addProcessor(GenericProcessor* processor, SignalElement* other, RelativeProcessorPosition pos = AFTER);
	void moveProcessor(GenericProcessor* processor, SignalElement* other, RelativeProcessorPosition pos = AFTER);
	
	void addProcessor(GenericProcessor* processor, SignalElement* other, unsigned int porNum, RelativeProcessorPosition pos = AFTER);
	void moveProcessor(GenericProcessor* processor, SignalElement* other, unsigned int porNum, RelativeProcessorPosition pos = AFTER);

	void addProcessor(GenericProcessor* processor, OutPort* afterPort);
	void addProcessor(GenericProcessor* processor, InPort* beforePort);

	void moveProcessor(GenericProcessor* processor, OutPort* afterPort);
	void moveProcessor(GenericProcessor* processor, InPort* beforePort);

	void removeProcessor(GenericProcessor* processor);
	void connectProcessor(GenericProcessor* processorFrom, unsigned int streamFrom, GenericProcessor* processorTo, unsigned int streamTo = 0);

	
	void updateProcessorSettings();
   
private:
	SignalElement* createElement(GenericProcessor* processor);
	SignalElement* detachElement(GenericProcessor* processor);

	void placeElement(SignalElement* element, OutPort* afterPort);
	void placeElement(SignalElement* element, InPort* beforePort);

	void sanitizeChain();
	OwnedArray<OutPort> m_startNodes;
	OwnedArray<SignalElement> m_elements;
	EditorViewport const* m_ev;
};


#endif  // __SIGNALCHAINMANAGER_H_948769B9__
