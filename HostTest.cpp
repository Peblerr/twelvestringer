// HostTest.cpp - loads the built TwelveStringer.vst3 as a host would and checks it.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what)
{
    std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

static std::unique_ptr<AudioPluginInstance> load (AudioPluginFormatManager& fm, const PluginDescription& d, double sr, int bs)
{
    String err;
    auto p = fm.createPluginInstance (d, sr, bs, err);
    if (! p) std::printf ("load error: %s\n", err.toRawUTF8());
    return p;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("usage: HostTest <plugin.vst3> <snapshot.png>\n"); return 2; }

    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> types;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (types, argv[1]);
    check (types.size() == 1, "VST3 scanned, " + String (types.size()) + " plugin found");
    if (types.isEmpty()) return 1;
    const auto& desc = *types[0];
    std::printf ("  name=%s  vendor=%s  category=%s  ins=%d outs=%d\n", desc.name.toRawUTF8(),
                 desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8(), desc.numInputChannels, desc.numOutputChannels);

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("\n-- %.0f Hz --\n", sr);
        auto plug = load (fm, desc, sr, 512);
        check (plug != nullptr, "instance created");
        if (! plug) continue;

        // mono in -> stereo out (typical guitar track), and stereo -> stereo
        for (int inCh : { 1, 2 })
        {
            AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (inCh == 1 ? AudioChannelSet::mono() : AudioChannelSet::stereo());
            layout.outputBuses.add (AudioChannelSet::stereo());
            const bool ok = plug->setBusesLayout (layout);
            check (ok, String (inCh) + "-in / 2-out layout accepted");

            plug->prepareToPlay (sr, 512);
            const int lat = plug->getLatencySamples();
            check (lat > 0, "HQ latency reported: " + String (lat) + " samples (" + String (1000.0 * lat / sr, 1) + " ms)");

            auto& params = plug->getParameters();
            AudioProcessorParameter* liveParam = nullptr;
            Array<AudioProcessorParameter*> automatable;   // the plugin's own parameters only
            for (auto* p : params)
            {
                const auto nm = p->getName (32);
                if (nm == "Live Mode") liveParam = p;
                else if (nm != "Bypass" && nm != "Program") automatable.add (p);
            }

            AudioBuffer<float> buf (2, 512);
            Random rng (42);
            MidiBuffer midi;
            bool finite = true; float peak = 0.f; double phase = 0;
            const int blocks = int (sr * 6 / 512);
            int latAfterLive = -1;
            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                for (int i = 0; i < 512; ++i)
                {
                    // decaying chord stabs every 0.5 s plus a little noise
                    const double t = (b * 512 + i) / sr;
                    const double env = std::exp (-6.0 * std::fmod (t, 0.5));
                    phase += 1.0 / sr;
                    float x = float (env * 0.2 * (std::sin (MathConstants<double>::twoPi * 98.0 * phase)
                                                + std::sin (MathConstants<double>::twoPi * 146.83 * phase)
                                                + std::sin (MathConstants<double>::twoPi * 246.94 * phase)))
                              + 0.001f * (rng.nextFloat() - 0.5f);
                    buf.setSample (0, i, x);
                    if (inCh == 2) buf.setSample (1, i, x);
                }
                // stress: randomise every parameter (except Live) in the 2nd third, toggle Live in the last third
                if (b > blocks / 3 && b < 2 * blocks / 3 && b % 7 == 0)
                    for (auto* p : automatable) p->setValue (rng.nextFloat());
                if (b == 2 * blocks / 3 && liveParam) liveParam->setValue (1.f);

                plug->processBlock (buf, midi);
                if (b == 2 * blocks / 3 + 1) latAfterLive = plug->getLatencySamples();

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                    {
                        const float v = buf.getSample (ch, i);
                        finite &= std::isfinite (v);
                        peak = jmax (peak, std::abs (v));
                    }
            }
            check (finite, "6 s of audio with parameter automation: all samples finite");
            check (peak < 4.f, "output peak sane: " + String (peak, 3));
            check (latAfterLive == 0, "Live Mode switches latency to 0 (got " + String (latAfterLive) + ")");
            if (liveParam) liveParam->setValue (0.f);
            plug->releaseResources();
        }
    }

    // ---- programs, parameter text, state round trip ----
    std::printf ("\n-- presets & state --\n");
    {
        auto a = load (fm, desc, 44100, 512);
        a->prepareToPlay (44100, 512);
        StringArray names;
        for (int i = 0; i < a->getNumPrograms(); ++i) names.add (a->getProgramName (i));
        check (a->getNumPrograms() == 6, "factory presets: " + names.joinIntoString (", "));

        for (auto* p : a->getParameters())
            std::printf ("    %-14s = %s\n", p->getName (32).toRawUTF8(), p->getCurrentValueAsText().toRawUTF8());

        a->setCurrentProgram (4);
        MemoryBlock state;
        a->getStateInformation (state);

        auto b = load (fm, desc, 44100, 512);
        b->setStateInformation (state.getData(), (int) state.getSize());
        bool same = true;
        for (int i = 0; i < a->getParameters().size(); ++i)
            same &= std::abs (a->getParameters()[i]->getValue() - b->getParameters()[i]->getValue()) < 1.0e-5f;
        check (same, "state save/restore round-trips all parameters (preset 'Wide Shimmer')");
        check (b->getCurrentProgram() == 4, "restored instance remembers the preset");

        // ---- editor snapshot ----
        a->setCurrentProgram (0);
        std::unique_ptr<AudioProcessorEditor> ed (a->createEditorIfNeeded());
        check (ed != nullptr, "editor opens: " + String (ed ? ed->getWidth() : 0) + "x" + String (ed ? ed->getHeight() : 0));
        if (ed)
        {
            a->editorBeingDeleted (ed.get());
            ed.reset();
        }
        check (true, "editor closed cleanly");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
