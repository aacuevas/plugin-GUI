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
	selectedGroup = -1;
	selectedStream.set(-1);
	selectedGroupChanged = false;
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
	if (!selectedGroupChanged)
	{
		//save current selections in case the inputs remain valid
		StreamGroup savedGroup;
		if (selectedGroup >= 0 && selectedGroup < streamGroups.size())
		{
			savedGroup.numChannels = streamGroups[selectedGroup].numChannels;
			savedGroup.sampleRate = streamGroups[selectedGroup].sampleRate;
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
		if (selectedGroup >= 0 && selectedGroup < streamGroups.size() && streamGroups[selectedGroup] == savedGroup)
		{
			//group is valid, check for stream validity
			int stream = selectedStream.get();
			if (stream > 0 && stream >= streamGroups[selectedGroup].startOffsets.size())
			{
				//not valid, setting stream to first one
				selectedStream.set(0);
			}
		}
		else
		{
			//chose the first option, to be safe
			selectedGroup = 0;
			selectedStream.set(0);
		}
		//update editor
		static_cast<StreamMuxerEditor*>(getEditor())->setStreamGroups(streamGroups, selectedGroup, selectedStream.get());
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

		int startChannel = 0;
		int numChannels = 0;
		if (selectedGroup >= 0 && selectedGroup < streamGroups.size())
		{
			startChannel = streamGroups[selectedGroup].startOffsets[selectedStream.get()];
			numChannels = streamGroups[selectedGroup].numChannels;
			settings.numOutputs = numChannels;
		}
		for (int i = 0; i < numChannels; i++)
		{
			int channelIndex = startChannel + i;
			DataChannel* chan = oldChannels[channelIndex];
			oldChannels.set(channelIndex, nullptr, false);
			dataChannelArray.add(chan);
		}
	}
	selectedGroupChanged = false; //always reset this
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
		selectedGroup = value;
		selectedGroupChanged = true;
	}
	else if (parameterIndex == 1)
	{
		selectedStream.set(value);
	}
}

void StreamMuxer::process(AudioSampleBuffer& buffer)
{
	int channelOffset = streamGroups[selectedGroup].startOffsets[selectedStream.get()];
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