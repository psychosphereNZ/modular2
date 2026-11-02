#include "PluginProcessor.h"
#include "PluginEditor.h"

PatchwerkProcessor::PatchwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    engine.loadPreset (0);
}

void PatchwerkProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    keyboardState.reset();
}

bool PatchwerkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PatchwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                engine.hostBpm.store (*bpm);

    engine.process (buffer, midi);
}

juce::AudioProcessorEditor* PatchwerkProcessor::createEditor()
{
    return new PatchwerkEditor (*this);
}

void PatchwerkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto v = engine.toState();
    v.setProperty ("cableOpacity", cableOpacity, nullptr);
    v.setProperty ("editorW", editorWidth, nullptr);
    v.setProperty ("editorH", editorHeight, nullptr);
    if (auto xml = v.createXml())
        copyXmlToBinary (*xml, destData);
}

void PatchwerkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto v = juce::ValueTree::fromXml (*xml);
        if (v.hasType ("PATCH"))
        {
            cableOpacity = (float) v.getProperty ("cableOpacity", 0.9f);
            editorWidth  = (int) v.getProperty ("editorW", 1360);
            editorHeight = (int) v.getProperty ("editorH", 860);
            engine.fromState (v);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PatchwerkProcessor();
}
