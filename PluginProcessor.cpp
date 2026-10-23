#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        float octave, focus, sparkle, unison, detune, drift, strum, width, dry, output;
    };

    // octave%, focus Hz, sparkle dB, unison%, detune c, drift%, strum ms, width%, dry%, out dB
    const Preset presets[] =
    {
        { "Classic 12-String",  55.f, 1200.f,  6.f, 40.f,  6.f, 35.f, 4.f,  70.f, 100.f,  0.f },
        { "Jangle Pop",         70.f, 2000.f,  9.f, 35.f,  5.f, 25.f, 2.f,  60.f, 100.f, -1.f },
        { "Folk Strummer",      50.f, 1000.f,  4.f, 50.f,  8.f, 45.f, 6.f,  80.f, 100.f,  0.f },
        { "Subtle Doubler",     30.f,  900.f,  3.f, 30.f,  4.f, 30.f, 3.f,  50.f, 100.f,  0.f },
        { "Wide Shimmer",       65.f, 3000.f, 10.f, 60.f, 12.f, 60.f, 8.f, 100.f,  90.f, -2.f },
        { "Octaves Only",       80.f, 1500.f,  6.f,  0.f,  0.f, 30.f, 3.f,  70.f, 100.f, -1.f },
    };

    juce::String percent (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
}

//==============================================================================
TwelveStringerProcessor::TwelveStringerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TwelveStringer", createLayout())
{
    pOctave  = apvts.getRawParameterValue (ParamID::octave);
    pFocus   = apvts.getRawParameterValue (ParamID::focus);
    pSparkle = apvts.getRawParameterValue (ParamID::sparkle);
    pUnison  = apvts.getRawParameterValue (ParamID::unison);
    pDetune  = apvts.getRawParameterValue (ParamID::detune);
    pDrift   = apvts.getRawParameterValue (ParamID::drift);
    pStrum   = apvts.getRawParameterValue (ParamID::strum);
    pWidth   = apvts.getRawParameterValue (ParamID::width);
    pDry     = apvts.getRawParameterValue (ParamID::dry);
    pOutput  = apvts.getRawParameterValue (ParamID::output);
    pLive    = apvts.getRawParameterValue (ParamID::live);
}

juce::AudioProcessorValueTreeState::ParameterLayout TwelveStringerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                   NormalisableRange<float> (0.f, 100.f, 0.1f), def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (percent));
    };

    p.push_back (pct (ParamID::octave, "Octave Mix", 55.f));

    NormalisableRange<float> focusRange (300.f, 8000.f, 1.f);
    focusRange.setSkewForCentre (1500.f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::focus, 1 }, "Octave Range",
                   focusRange, 1200.f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   {
                       return v >= 1000.f ? String (v / 1000.f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
                   })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::sparkle, 1 }, "Sparkle",
                   NormalisableRange<float> (-6.f, 12.f, 0.1f), 6.f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    p.push_back (pct (ParamID::unison, "Unison Mix", 40.f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::detune, 1 }, "Detune",
                   NormalisableRange<float> (0.f, 25.f, 0.1f), 6.f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return String (v, 1) + " ct"; })));

    p.push_back (pct (ParamID::drift, "Drift", 35.f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::strum, 1 }, "Strum Offset",
                   NormalisableRange<float> (0.f, 20.f, 0.1f), 4.f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return String (v, 1) + " ms"; })));

    p.push_back (pct (ParamID::width, "Width", 70.f));
    p.push_back (pct (ParamID::dry, "Dry", 100.f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
                   NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                   { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::live, 1 }, "Live Mode", false));

    return { p.begin(), p.end() };
}

twelve::Params TwelveStringerProcessor::readParams() const
{
    twelve::Params p;
    p.octaveMix     = pOctave->load() * 0.01f;
    p.octaveFocusHz = pFocus->load();
    p.sparkleDb     = pSparkle->load();
    p.unisonMix     = pUnison->load() * 0.01f;
    p.detuneCents   = pDetune->load();
    p.drift         = pDrift->load() * 0.01f;
    p.strumMs       = pStrum->load();
    p.width         = pWidth->load() * 0.01f;
    p.dryLevel      = pDry->load() * 0.01f;
    p.outputDb      = pOutput->load();
    p.liveMode      = pLive->load() > 0.5f;
    return p;
}

//==============================================================================
void TwelveStringerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    engine.reset();
    setLatencySamples (engine.getLatencySamples());
}

bool TwelveStringerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TwelveStringerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (numIn == 0 || numOut < 2)
        return;

    engine.setParams (readParams());

    const int latency = engine.getLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples (latency);   // Live mode toggled

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    engine.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), n);
}

//==============================================================================
int TwelveStringerProcessor::getNumPrograms() { return (int) std::size (presets); }

const juce::String TwelveStringerProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets[index].name : "";
}

void TwelveStringerProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    const auto& pr = presets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set (ParamID::octave,  pr.octave);
    set (ParamID::focus,   pr.focus);
    set (ParamID::sparkle, pr.sparkle);
    set (ParamID::unison,  pr.unison);
    set (ParamID::detune,  pr.detune);
    set (ParamID::drift,   pr.drift);
    set (ParamID::strum,   pr.strum);
    set (ParamID::width,   pr.width);
    set (ParamID::dry,     pr.dry);
    set (ParamID::output,  pr.output);
}

//==============================================================================
void TwelveStringerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TwelveStringerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* TwelveStringerProcessor::createEditor()
{
    return new TwelveStringerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TwelveStringerProcessor();
}
