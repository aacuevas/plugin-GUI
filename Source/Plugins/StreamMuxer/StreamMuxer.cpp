#include "StreamMuxer.h"
#include "StreamMuxerEditor.h"
#include <math.h>

using namespace StreamMuxerPlugin;

bool StreamGroup::operator==(const StreamGroup& other) const
{
	return (numChannels == other.numChannels && sampleRate == other.sampleRate && !isnan(sampleRate));
}

StreamGroup::StreamGroup() : sampleRate(NAN), numChannels(0)
{
}

StreamGroup::StreamGroup(float s, int n) : sampleRate(s), numChannels(n)
{
}

StreamMuxer::StreamMuxer() : GenericProcessor("Stream Muxer")
{
	m_selectedGroup = -1;
	m_selectedStream.set(-1);
	m_selectedGroupChanged = false;
}


StreamMuxer::~StreamMuxer()
{
}

AudioProcessorEditor* StreamMuxer::createEditor()
{
	editor = new StreamMuxerEditor(this, true);
	return editor;
}


void StreamMuxer::updateSettings()
{
	//if the update came from other part of the signal chain, update everything
	//we do not do this process when updating from the same processor to avoid wasting resources
	if (!m_selectedGroupChanged)
	{
		//save current selections in case the inputs remain valid
		StreamGroup savedGroup;
		if (m_selectedGroup >= 0 && m_selectedGroup < streamGroups.size())
		{
			savedGroup.numChannels = streamGroups[m_selectedGroup].numChannels;
			savedGroup.sampleRate = streamGroups[m_selectedGroup].sampleRate;
		}

		streamGroups.clear();

		int numChannels = getNumInputs();
		uint16 curProc = 0;
		uint16 curSub = 0;
		StreamGroup workingGroup;
		int startOffset = -1;
		for (int i = 0; i < numChannels; i++)
		{
			const DataChannel* channel = dataChannelArray[i];
			//stream change
			//this assumes that all channels from the same stream are contiguous. A channel mapper mixing streams will break this
			if (curProc != channel->getSourceNodeID() || curSub != channel->getSubProcessorIdx())
			{
				curProc = channel->getSourceNodeID();
				curSub = channel->getSubProcessorIdx();

				insertGroup(workingGroup, startOffset);

				//reset group
				workingGroup.numChannels = 0;
				workingGroup.sampleRate = channel->getSampleRate();
				workingGroup.startOffsets.clear();
				startOffset = i;
			}
			workingGroup.numChannels++;
		}
		insertGroup(workingGroup, startOffset); //last group insertion

		//now check if the selected group is still valid
		if (m_selectedGroup >= 0 && m_selectedGroup < streamGroups.size() && streamGroups[m_selectedGroup] == savedGroup)
		{
			//group is valid, check for stream validity
			int stream = m_selectedStream.get();
			if (stream > 0 && stream >= streamGroups[m_selectedGroup].startOffsets.size())
			{
				//not valid, setting stream to first one
				m_selectedStream.set(0);
			}
		}
		else
		{
			//chose the first option, to be safe
			m_selectedGroup = 0;
			m_selectedStream.set(0);
		}
		//update editor
		static_cast<StreamMuxerEditor*>(getEditor())->setStreamGroups(streamGroups, m_selectedGroup, m_selectedStream.get());
	}
#if 0
	{
		for (int g = 0; g < streamGroups.size(); g++)
		{
			std::cout << "g " << g << " s " << streamGroups[g].sampleRate << " n " << streamGroups[g].numChannels << " on " << streamGroups[g].startOffsets.size() << std::endl;
			for (int o = 0; o < streamGroups[g].startOffsets.size(); o++)
			{
				std::cout << "o " << o << " - " << streamGroups[g].startOffsets[o] << std::endl;
			}
		}
	}
#endif
	//Now we update the output channels
	{
		OwnedArray<DataChannel> oldChannels;
		oldChannels.swapWith(dataChannelArray);
		dataChannelArray.clear();

		originalChannels.clear();

		int numChannels = 0;
		float sampleRate = 0;
		if (m_selectedGroup >= 0 && m_selectedGroup < streamGroups.size())
		{
			numChannels = streamGroups[m_selectedGroup].numChannels;
			settings.numOutputs = numChannels;

			sampleRate = streamGroups[m_selectedGroup].sampleRate;

			//store original channels
			std::vector<int>& offsets = streamGroups[m_selectedGroup].startOffsets;
			int numStreams = offsets.size();

			for (int i = 0; i < numStreams; i++)
			{
				originalChannels.push_back(OwnedArray<DataChannel>());
				int offset = offsets[i];
				for (int c = 0; c < numChannels; c++)
				{
					DataChannel* chan = oldChannels[offset+c];
					oldChannels.set(offset+c, nullptr, false);
					originalChannels.back().add(chan);
				}
			}

			//create metadata structures
			String historic = "{";
			HeapBlock<uint16> idArray;
			idArray.allocate(numChannels * 2, true);
			for (int i = 0; i < numStreams; i++)
			{
				DataChannel* chan = originalChannels[i][0]; //take first channel as sample
				historic += "[" + chan->getHistoricString() + "]";
				idArray[2*i] = chan->getSourceNodeID();
				idArray[2 * i + 1] = chan->getSubProcessorIdx();
			}
			historic += "}";
			MetaDataDescriptor mdNumDesc = MetaDataDescriptor(MetaDataDescriptor::UINT32, 1, "Number of muxed streams",
				"Number of streams muxed into this channel",
				"stream.mux.count");
			MetaDataDescriptor mdSourceDesc = MetaDataDescriptor(MetaDataDescriptor::UINT16, 3, "Source processors",
				"2xN array of uint16 that specifies the nodeID and Stream index of the possible sources for this channel",
				"source.identifier.full.array");

			MetaDataValue mdNum = MetaDataValue(mdNumDesc);
			mdNum.setValue((uint32)numStreams);
			MetaDataValue mdSource = MetaDataValue(mdSourceDesc, idArray.getData());

			for (int i = 0; i < numChannels; i++)
			{
				
				//lets assume the values are consistent between streams and take the first stream as sample
				DataChannel* orig = originalChannels.front()[i];
				DataChannel::DataChannelTypes type = orig->getChannelType();
				DataChannel* chan = new DataChannel(type, sampleRate, this);
				chan->setBitVolts(orig->getBitVolts());
				chan->setDataUnits(orig->getDataUnits());
				
				chan->addToHistoricString(historic);
				chan->addMetaData(mdNumDesc, mdNum);
				chan->addMetaData(mdSourceDesc, mdSource);
				
				dataChannelArray.add(chan);
			}
			m_selectedSampleRate = sampleRate;
			m_selectedBitVolts = dataChannelArray[0]->getBitVolts(); //to get some value
		}
	}
	m_selectedGroupChanged = false; //always reset this
	//now create event channel

	EventChannel* ech = new EventChannel(EventChannel::UINT32_ARRAY, 1, 1, m_selectedSampleRate, this);
	ech->setName("Stream Selected");
	ech->setDescription("Value of the selected stream each time it changes");
	ech->setIdentifier("stream.mux.index.selected");
}

void StreamMuxer::insertGroup(StreamGroup& workingGroup, int startOffset)
{
	//find if there is a group with the characteristics we had
	int numGroups = streamGroups.size();
	for (int i = 0; i < numGroups; i++)
	{
		if (streamGroups[i] == workingGroup)
		{
			streamGroups[i].startOffsets.push_back(startOffset);
			startOffset = -1;
			break;
		}
	}
	//if new group or first group
	if (startOffset > -1)
	{
		workingGroup.startOffsets.push_back(startOffset);
		streamGroups.push_back(workingGroup);
	}
}

void StreamMuxer::setParameter(int parameterIndex, float value)
{
	if (parameterIndex == 0) //stream group
	{
		m_selectedGroup = value;
		m_selectedGroupChanged = true;
	}
	else if (parameterIndex == 1)
	{
		m_selectedStream.set(value);
	}
}

void StreamMuxer::process(AudioSampleBuffer& buffer)
{
	int channelOffset = streamGroups[m_selectedGroup].startOffsets[m_selectedStream.get()];
	if (channelOffset > 0) //copying over the same channels can be problematic
	{
		for (int i = 0; i < settings.numOutputs; i++)
		{
			buffer.copyFrom(i, //destChannel
				0, //destStartSample
				buffer, //sourceBuffer
				channelOffset + i, //sourceChannel
				0, //sourceStartSample
				buffer.getNumSamples());
		}
	}

}