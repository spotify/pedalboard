/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
UnityGainNChannelAudioProcessor::UnityGainNChannelAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::discreteChannels(32), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::discreteChannels(32), true)
                     #endif
                       )
#endif
{
}

UnityGainNChannelAudioProcessor::~UnityGainNChannelAudioProcessor()
{
}

//==============================================================================
const juce::String UnityGainNChannelAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool UnityGainNChannelAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool UnityGainNChannelAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool UnityGainNChannelAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double UnityGainNChannelAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int UnityGainNChannelAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int UnityGainNChannelAudioProcessor::getCurrentProgram()
{
    return 0;
}

void UnityGainNChannelAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String UnityGainNChannelAudioProcessor::getProgramName (int index)
{
    return {};
}

void UnityGainNChannelAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void UnityGainNChannelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void UnityGainNChannelAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool UnityGainNChannelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void UnityGainNChannelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	juce::ScopedNoDenormals noDenormals;

	for (auto i = 0; i < buffer.getNumSamples(); ++i)
	{
		for (auto j = 0; j < buffer.getNumChannels(); ++j)
		{
			*buffer.getWritePointer(j, i) = *buffer.getReadPointer(j, i);
		}
	}
}

//==============================================================================
bool UnityGainNChannelAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* UnityGainNChannelAudioProcessor::createEditor()
{
    return new UnityGainNChannelAudioProcessorEditor (*this);
}

//==============================================================================
void UnityGainNChannelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void UnityGainNChannelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UnityGainNChannelAudioProcessor();
}
