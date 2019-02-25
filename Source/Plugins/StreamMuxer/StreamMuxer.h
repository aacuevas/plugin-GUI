#include <ProcessorHeaders.h>

namespace StreamMuxerPlugin
{

	class StreamGroup
	{
	public:
		float sampleRate;
		unsigned int numChannels;
		std::vector<int> startOffsets;
		StreamGroup();
		StreamGroup(float s, int n);
		bool operator==(const StreamGroup&) const;
	};

	class StreamMuxer : public GenericProcessor
	{
	public:
		StreamMuxer();
		~StreamMuxer();

		AudioProcessorEditor* createEditor() override;

		void process(AudioSampleBuffer& buffer) override;

		bool hasEditor() const override { return true; }

		void updateSettings() override;

		void setParameter(int parameterIndex, float value) override;

		float getDefaultSampleRate() const override;
		float getDefaultBitVolts() const override;

	private:
		void insertGroup(StreamGroup& workingGroup, int startOffset);
		std::vector<StreamGroup> streamGroups;
		std::vector<OwnedArray<DataChannel>> originalChannels;
		int selectedGroup;
		Atomic<int> selectedStream;
		bool selectedGroupChanged;

		float m_selectedSampleRate;
		float m_selectedBitVolts;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StreamMuxer);
	};

}