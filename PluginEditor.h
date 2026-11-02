#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace pwui
{
    namespace col
    {
        const juce::Colour bg       { 0xff111317 };
        const juce::Colour chrome   { 0xff181b20 };
        const juce::Colour rack     { 0xff14171b };
        const juce::Colour dot      { 0xff232830 };
        const juce::Colour panel    { 0xff1d2127 };
        const juce::Colour panelHi  { 0xff252a31 };
        const juce::Colour plate    { 0xff2a3038 };
        const juce::Colour line     { 0xff30363f };
        const juce::Colour fg       { 0xffdfe4ea };
        const juce::Colour muted    { 0xff7f8994 };
        const juce::Colour accent   { 0xff3cc8e8 };
        const juce::Colour knob     { 0xff2c323a };
        const juce::Colour track    { 0xff383f49 };
        const juce::Colour hole     { 0xff07090b };
        const juce::Colour ring     { 0xff8a939e };
        const juce::Colour ringIn   { 0xff3a414a };
        const juce::Colour gateRing { 0xffc99a3e };
        const juce::Colour keyW     { 0xffc8cdd4 };
        const juce::Colour keyB     { 0xff1a1d22 };
    }

    struct JackRef
    {
        int uid = -1;
        bool out = false;
        int port = -1;
        bool valid() const { return uid >= 0 && port >= 0; }
        bool operator== (const JackRef& o) const { return uid == o.uid && out == o.out && port == o.port; }
    };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
    };

    class RackComponent;
    class CableLayer;

    class ModuleComponent : public juce::Component
    {
    public:
        ModuleComponent (RackComponent&, std::shared_ptr<pw::Module>);
        ~ModuleComponent() override;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        JackRef jackAtLocal (juce::Point<int>) const;
        juce::Point<int> jackCentreLocal (bool out, int port) const;
        void tick();

        std::shared_ptr<pw::Module> mod;
        pw::ModuleLayout layout;

    private:
        void drawJack (juce::Graphics&, juce::Point<int>, bool gate, const JackRef&);

        RackComponent& rack;
        juce::Colour cat;
        std::vector<std::unique_ptr<juce::Slider>> sliders;
        std::vector<std::unique_ptr<juce::ComboBox>> combos;
        std::vector<std::unique_ptr<juce::Component>> toggles;
        std::unique_ptr<juce::Component> extra;
        juce::Rectangle<int> closeRect;
        int dragMode = 0;
        juce::Point<int> dragStart;
        float lastLed = -2.0f;
        int lastStep = -2, lastLen = -1;
    };

    class CableLayer : public juce::Component
    {
    public:
        explicit CableLayer (RackComponent& r) : rack (r) {}
        void paint (juce::Graphics&) override;
        bool hitTest (int x, int y) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        int cableAt (juce::Point<float>) const;
        RackComponent& rack;
        std::vector<juce::Path> hits;
        int hover = -1, pressed = -1;
    };

    class RackComponent : public juce::Component
    {
    public:
        RackComponent (PatchwerkProcessor&, LookAndFeel&);
        LookAndFeel& getLnf() { return lnf; }
        ~RackComponent() override;

        void sync();
        void paint (juce::Graphics&) override;
        void fitSize (juce::Rectangle<int> viewArea = {});
        void tick();
        void cablesChanged();
        void removeModuleLater (int uid);
        void setCableOpacity (float a);

        JackRef jackAt (juce::Point<int>) const;
        juce::Point<float> jackPos (const JackRef&) const;
        bool isCompatible (const JackRef& a, const JackRef& b) const;
        juce::Colour jackColour (const JackRef&) const;

        void beginCableDrag (const JackRef&, juce::Point<int>);
        void dragCable (juce::Point<int>);
        void endCableDrag (juce::Point<int>);

        PatchwerkProcessor& proc;
        LookAndFeel& lnf;
        bool dragging = false;
        JackRef anchor;
        juce::Point<float> dragPos;
        juce::Colour dragColour;
        int knownVersion = -1;

    private:
        ModuleComponent* find (int uid) const;
        std::vector<std::unique_ptr<ModuleComponent>> comps;
        std::unique_ptr<CableLayer> cableLayer;
        juce::Rectangle<int> lastViewArea;
        std::vector<pw::Cable> cableCache;
        friend class CableLayer;
    };
}

class PatchwerkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PatchwerkEditor (PatchwerkProcessor&);
    ~PatchwerkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PatchwerkProcessor& proc;
    pwui::LookAndFeel lnf;
    juce::ComboBox presetBox, addBox;
    juce::Slider opacity;
    juce::Viewport viewport;
    pwui::RackComponent rack;
    juce::Rectangle<int> presetLabel, addLabel, opacityLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchwerkEditor)
};
