#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace pw
{

constexpr int kMaxParams = 24;
constexpr int kMaxPorts  = 6;

//==============================================================================
// Module descriptions (shared by DSP and UI)

enum class Cat { Ctl, Src, Mod, Proc, Fx, Out };
juce::Colour catColour (Cat c);

enum class PKind { Knob, Choice, Toggle };

struct ParamSpec
{
    juce::String id, label;
    PKind kind = PKind::Knob;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    bool  log = false;
    float step = 0.0f;
    std::function<juce::String (float)> fmt;
    juce::StringArray choices;
    bool small = false;   // small knob
    bool grid  = false;   // goes into the sequencer step grid

    float toNorm (float v) const;
    float fromNorm (float n) const;
};

struct PortSpec
{
    juce::String id, label;
    bool gate = false;
};

enum class Extra { None, KeysBelow, EnvAbove, ScopeAbove };

struct ModuleSpec
{
    juce::String type, title, sub;
    Cat cat = Cat::Src;
    int width = 190;
    Extra extra = Extra::None;
    std::vector<ParamSpec> params;
    std::vector<PortSpec> ins, outs;

    int paramIndex (const juce::String& id) const;
    int inIndex (const juce::String& id) const;
    int outIndex (const juce::String& id) const;
};

const std::vector<ModuleSpec>& allSpecs();
const ModuleSpec* findSpec (const juce::String& type);

//==============================================================================
// Geometry of a module panel. Used by the editor and by preset placement.

struct ModuleLayout
{
    int height = 0;
    std::vector<juce::Rectangle<int>> params;   // one per param (cell rect)
    juce::Rectangle<int> extra;
    std::vector<juce::Point<int>> inJacks, outJacks;
    juce::Rectangle<int> outPlate;
    int jackRowTop = 0;
    int gridTop = -1;
};

ModuleLayout computeLayout (const ModuleSpec& spec);

//==============================================================================
// DSP modules — processed one sample at a time.

struct Module
{
    Module (const ModuleSpec& s, int id);
    virtual ~Module() = default;

    const ModuleSpec& spec;
    const int uid;
    int x = 0, y = 0;                       // panel position (message thread)

    std::atomic<float> p[kMaxParams];
    float in[kMaxPorts]  {};
    float out[kMaxPorts] {};
    bool  inConn[kMaxPorts] {};
    double sr = 48000.0;
    const std::atomic<double>* hostBpm = nullptr;

    // telemetry for the UI
    std::atomic<float> led { 0.0f };
    std::atomic<int>   curStep { -1 };

    float P (int i) const noexcept { return p[i].load (std::memory_order_relaxed); }
    void  setParam (int i, float v) noexcept { p[i].store (v, std::memory_order_relaxed); }

    virtual void prepare (double sampleRate) { sr = sampleRate; }
    virtual void process() noexcept = 0;

    // keyboard only
    virtual void noteOn (int) noexcept {}
    virtual void noteOff (int) noexcept {}
    virtual void allNotesOff() noexcept {}
    virtual void pitchBend (float) noexcept {}

    // output only: scope ring buffer
    static constexpr int scopeSize = 2048;
    float scope[scopeSize] {};
    std::atomic<int> scopeWrite { 0 };
};

//==============================================================================

struct Cable
{
    int srcUid, srcPort, dstUid, dstPort;
    juce::Colour colour;
    Module* src = nullptr;
    Module* dst = nullptr;
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept;

    // message thread
    std::shared_ptr<Module> addModule (const juce::String& type, int x, int y, int uid = -1);
    void removeModule (int uid);
    bool canConnect (int srcUid, int srcPort, int dstUid, int dstPort) const;
    bool addCable (int srcUid, int srcPort, int dstUid, int dstPort, juce::Colour c);
    void removeCable (size_t index);
    void clear();
    void loadPreset (int index);
    static juce::StringArray presetNames();

    juce::ValueTree toState() const;
    void fromState (const juce::ValueTree& v);

    std::vector<std::shared_ptr<Module>> getModules() const { return modules; }
    std::vector<Cable> getCables() const { return cables; }
    Module* findModule (int uid) const;

    juce::Colour nextColour();
    juce::Colour peekColour() const;
    static juce::Colour cableColour (int i);

    std::atomic<int> version { 0 };          // bumps on structural change
    std::atomic<double> hostBpm { 0.0 };

private:
    void rebuildCaches();                    // call with lock held

    juce::SpinLock lock;
    std::vector<std::shared_ptr<Module>> modules;
    std::vector<Cable> cables;
    std::vector<Module*> raw, keyboards, outputs;
    double sampleRate = 48000.0;
    int nextUid = 1;
    int colourIndex = 0;
    float limGain = 1.0f;
};

} // namespace pw
