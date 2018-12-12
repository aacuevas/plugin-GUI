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

#include "SignalChainManager.h"
#include "../Processors/GenericProcessor/GenericProcessor.h"

//Port 
Port::Port()
	: m_connection(nullptr)
{
}

Port::~Port()
{
	disconnect();
}

Port* Port::getConnection() const
{
	return m_connection;
}

Port* Port::disconnect()
{
	Port* old = m_connection;
	m_connection->m_connection = nullptr;
	m_connection = nullptr;
	return old;
}

Port::Port(Port&& other)
{
	other.m_connection->disconnect();
	other.m_connection = this;
	m_connection = &other;
}

Port& Port::operator=(Port&& other)
{
	other.m_connection->disconnect();
	other.m_connection = this;
	m_connection = &other;
	return *this;
}

Port* Port::connect(Port* dest)
{
	if (dest == m_connection) return dest;
	Port* last = m_connection;
	disconnect();
	dest->disconnect();
	m_connection = dest;
	dest->m_connection = this;
	return last;
}

//OutPort

OutPort::OutPort(unsigned int numChannels) : m_numChannels (numChannels)
{}

OutPort::~OutPort()
{}

unsigned int OutPort::getNumChannels() const
{
	return m_numChannels;
}

void OutPort::updateChannelCount(unsigned int numChannels)
{
	m_numChannels = numChannels;
}

InPort* OutPort::connect(InPort* dest)
{
	return dynamic_cast<InPort*>(Port::connect(dest));
}

InPort* OutPort::disconnect()
{
	return dynamic_cast<InPort*>(Port::disconnect());
}

//InPort

InPort::InPort()
{}

InPort::~InPort()
{}

unsigned int InPort::getNumChannels() const
{
	Port* conn = getConnection();
	if (conn)
		return conn->getNumChannels();
	else return 0;
}

OutPort* InPort::connect(OutPort* dest)
{
	return dynamic_cast<OutPort*>(Port::connect(dest));
}

OutPort* InPort::disconnect()
{
	return dynamic_cast<OutPort*>(Port::disconnect());
}

bool InPort::acceptsConnections() const
{
	return true;
}

//SourcePort

SourcePort::SourcePort()
{}

SourcePort::~SourcePort()
{}

bool SourcePort::acceptsConnections() const
{
	return false;
}

//SignalElement

SignalElement::SignalElement(GenericProcessor* proc) : m_processor(proc)
{
	if (proc)
		proc->m_signalElement = this;
	updateConnections();
}

SignalElement::~SignalElement()
{
	if (m_processor)
		m_processor->m_signalElement = nullptr;
}

unsigned int SignalElement::getInPorts() const
{
	return m_inputPorts.size();
}

unsigned int SignalElement::getOutPorts() const
{
	return m_outputPorts.size();
}

InPort* SignalElement::getInPort(unsigned int idx) const
{
	return m_inputPorts[idx];
}

OutPort* SignalElement::getOutPort(unsigned int idx) const
{
	return m_outputPorts[idx];
}

void SignalElement::updateConnections()
{
	if (m_processor)
	{
		m_processor->updateStreamCount();
		unsigned int numInputs = m_processor->getNumInputStreams();
		unsigned int curInputs = m_inputPorts.size();

		if (numInputs > 0) //not a source node
		{
			if (curInputs > 0 && !m_inputPorts[0]->acceptsConnections) //if somehow this was a source node but not anymore
			{
				m_inputPorts.remove(0);
				curInputs -= 1;
			}

			if (numInputs < curInputs)
				m_inputPorts.removeLast(curInputs - numInputs);
			else
			{
				for (unsigned int i = curInputs; i < numInputs; i++)
					m_inputPorts.add(new InPort());
			}
		}
		else
		{
			if (curInputs > 0) //there were input ports
			{
				if (m_inputPorts[0]->acceptsConnections) //if somehow this wasn't a source node but is now
				{
					m_inputPorts.clear();
					m_inputPorts.add(new SourcePort());
				}
				//if this was a source node and still is, keep the existing source port
			}
			else
			{
				m_inputPorts.add(new SourcePort());
			}
		}

		unsigned int numOutputs = m_processor->getNumStreams();
		unsigned int curOutputs = m_outputPorts.size();

		if (numOutputs < curOutputs)
			m_outputPorts.removeLast(curOutputs - numOutputs);
		else
		{
			for (unsigned int i = curOutputs; i < numOutputs; i++)
				m_outputPorts.add(new OutPort(0));
		}
	}
	else //Start nodes have no processor, just indicate the beggining of a graph
	{
		m_inputPorts.clear(); //just in case
		if (m_outputPorts.size() != 1)
		{
			m_outputPorts.clear();
			m_outputPorts.add(new OutPort(0));
		}
	}
}

void SignalElement::updateChannelCounts()
{
	if (m_processor)
	{
		int numOutputs = m_processor->getNumStreams();
		for (unsigned int i = 0; i < numOutputs; i++)
			m_outputPorts[i]->updateChannelCount(m_processor->getNumOutputs(i));
	}
}

const GenericProcessor* SignalElement::getProcessor()
{
	return m_processor;
}

//SignalChainManager

SignalChainManager::SignalChainManager(EditorViewport* ev) : m_ev(ev)
{
}

SignalChainManager::~SignalChainManager()
{
}

void SignalChainManager::addProcessor(GenericProcessor* processor, SignalElement* other, RelativeProcessorPosition pos)
{
	addProcessor(processor, other, 0, pos);
}

void SignalChainManager::moveProcessor(GenericProcessor* processor, SignalElement* other, RelativeProcessorPosition pos)
{
	moveProcessor(processor, other, 0, pos);
}

void SignalChainManager::addProcessor(GenericProcessor* processor, SignalElement* other, unsigned int porNum, RelativeProcessorPosition pos)
{
	if (pos == AFTER)
	{
		OutPort* port = other ? other->getOutPort(0) : nullptr;
		addProcessor(processor, port);
	}
	else if (pos == BEFORE)
	{
		InPort* port = other ? other->getInPort(0) : nullptr;
		addProcessor(processor, port);
	}
}

void SignalChainManager::moveProcessor(GenericProcessor* processor, SignalElement* other, unsigned int porNum, RelativeProcessorPosition pos)
{
	if (pos == AFTER)
	{
		OutPort* port = other ? other->getOutPort(0) : nullptr;
		moveProcessor(processor, port);
	}
	else if (pos == BEFORE)
	{
		InPort* port = other ? other->getInPort(0) : nullptr;
		moveProcessor(processor, port);
	}
}

void SignalChainManager::placeElement(SignalElement* element, OutPort* afterPort)
{
	if (afterPort)
	{
		InPort* old = afterPort->connect(element->getInPort(0));
		old->connect(element->getOutPort(0));
	}
	sanitizeChain();
}

void SignalChainManager::placeElement(SignalElement* element, InPort* beforePort)
{
	if (beforePort)
	{
		OutPort* old = beforePort->connect(element->getOutPort(0));
		old->connect(element->getInPort(0));
	}
	sanitizeChain();
}

void SignalChainManager::addProcessor(GenericProcessor* processor, InPort* port)
{
	SignalElement* element = createElement(processor);
	placeElement(element, port);
}

void SignalChainManager::addProcessor(GenericProcessor* processor, OutPort* port)
{
	SignalElement* element = createElement(processor);
	placeElement(element, port);
}

void SignalChainManager::moveProcessor(GenericProcessor* processor, InPort* port)
{
	SignalElement* element = detachElement(processor);
	placeElement(element, port);
}

void SignalChainManager::moveProcessor(GenericProcessor* processor, OutPort* port)
{
	SignalElement* element = detachElement(processor);
	placeElement(element, port);
}

SignalElement* SignalChainManager::createElement(GenericProcessor* processor)
{
	SignalElement* element = new SignalElement(processor);
	m_elements.add(element);
	return element;
}

SignalElement* SignalChainManager::detachElement(GenericProcessor* processor)
{
	SignalElement* element = processor->getSignalElement();

	//connect the first input and output ports together. Anything else will be left dangling
	OutPort* previous = element->getInPort(0)->disconnect();
	InPort* next = element->getOutPort(0)->disconnect();
	previous->connect(next);

	//disconnect everything else
	unsigned int numInPorts = element->getInPorts();
	for (unsigned int i = 1; i < numInPorts; i++)
		element->getInPort(i)->disconnect();

	unsigned int numOutPorts = element->getOutPorts();
	for (unsigned int i = 1; i < numOutPorts; i++)
		element->getOutPort(i)->disconnect();

	return element;
	
}

void SignalChainManager::sanitizeChain()
{
	//remove empty chains
	unsigned int numStart = m_startNodes.size();
	for (unsigned int i = 0; i < numStart; i++)
	{
		if (m_startNodes[i]->getOutPort(0)->getConnection() == nullptr)
		{
			//this way of removing elements is not the most optimized way, but there will never be enough chains for this to be a performance issue
			m_startNodes.remove(i);
			--i;
			--numStart;
		}
	}

	//create a new chain for any dangling input
	unsigned int numNodes = m_elements.size();
	for (unsigned int i = 0; i < numNodes; i++)
	{
		SignalElement* element = m_elements[i];
		unsigned int numPorts = element->getInPorts();
		for (unsigned int j = 0; j < numPorts; j++)
		{
			InPort* port = element->getInPort(j);
			if (port->getConnection() == nullptr)
			{
				//Create new source
				SignalElement* start = new SignalElement(nullptr);
				m_startNodes.add(start);

				start->getOutPort(0)->connect(port);
			}
		}
	}
}

void SignalChainManager::removeProcessor(GenericProcessor* processor)
{
	SignalElement* element = detachElement(processor);
	m_elements.removeObject(element);
}

void SignalChainManager::updateSignalChain()
{
	updateChainConnectivity();
	updateProcessorSettings();
}

void SignalChainManager::updateChainConnectivity()
{
	int numElements = m_elements.size();
	for (int i = 0; i < numElements; i++)
	{
		SignalElement *elm = m_elements[i];
		elm->updateConnections();
	}
}

void SignalChainManager::updateProcessorSettings()
{

}