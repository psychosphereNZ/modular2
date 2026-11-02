#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static double rms (const juce::AudioBuffer<float>& b)
{
    double s = 0; int n = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) { s += b.getSample (c, i) * b.getSample (c, i); ++n; }
    return std::sqrt (s / n);
}

static double renderSeconds (PatchwerkProcessor& p, double secs, juce::MidiBuffer* first = nullptr)
{
    juce::AudioBuffer<float> buf (2, 512);
    double total = 0; int blocks = (int) (secs * 48000 / 512);
    for (int i = 0; i < blocks; ++i)
    {
        juce::MidiBuffer m;
        if (i == 0 && first) m = *first;
        buf.clear();
        p.processBlock (buf, m);
        total += rms (buf);
        for (int c = 0; c < 2; ++c) for (int s = 0; s < 512; ++s) if (! std::isfinite (buf.getSample (c, s))) { printf ("NaN!\n"); return -1; }
    }
    return total / blocks;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    PatchwerkProcessor p;
    p.setPlayConfigDetails (0, 2, 48000, 512);
    p.prepareToPlay (48000, 512);

    printf ("silence before note: %.5f\n", renderSeconds (p, 0.3));
    juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 10);
    printf ("note held:           %.5f\n", renderSeconds (p, 1.0, &on));
    juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    renderSeconds (p, 3.0, &off);
    printf ("after release+tail:  %.5f\n", renderSeconds (p, 0.5));

    // state round trip
    juce::MemoryBlock mb; p.getStateInformation (mb);
    PatchwerkProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
    printf ("modules %d/%d cables %d/%d\n", (int) p.engine.getModules().size(), (int) p2.engine.getModules().size(),
            (int) p.engine.getCables().size(), (int) p2.engine.getCables().size());

    for (int pr = 0; pr < 4; ++pr)
    {
        p.engine.loadPreset (pr);
        juce::MidiBuffer n; n.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
        printf ("preset %d (%s): rms %.5f\n", pr, pw::Engine::presetNames()[pr].toRawUTF8(), renderSeconds (p, 1.5, &n));
        juce::MidiBuffer o; o.addEvent (juce::MidiMessage::noteOff (1, 57), 0); renderSeconds (p, 0.1, &o);
    }

    p.engine.loadPreset (0);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1440, 880);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argc > 1 ? argv[1] : "editor.png");
    out.deleteFile();
    juce::FileOutputStream fos (out);
    juce::PNGImageFormat().writeImageToStream (img, fos);
    printf ("snapshot written\n");
    ed.reset();
    return 0;
}
