#pragma once
#include "PluginProcessor.h"

class SDNASuperAmpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SDNASuperAmpLookAndFeel();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
};

class SDNAEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SDNAEditor(SDNAProcessor&);
    ~SDNAEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider; juce::Label label; std::unique_ptr<SA> att;
    };

    struct ModuleIcon : juce::Component
    {
        int module=0; juce::String title; juce::Colour accent; bool selected=false,on=true,dragging=false;
        std::function<void(int)> onSelect; std::function<void(int)> onToggle; std::function<void(int,int)> onDrag;
        ModuleIcon(int id,juce::String name,juce::Colour c):module(id),title(std::move(name)),accent(c){}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override { dragging=false; }
        juce::Rectangle<float> powerArea() const { return { (float)getWidth()-25.f,7.f,17.f,17.f }; }
    };

    void timerCallback() override;
    void chooseNAM(bool preampOrSingle);
    void chooseIR();
    void addKnob(Knob&,const juce::String&,const char*,const juce::String& suffix={});
    void setupCombo(juce::ComboBox&,juce::Label&,const juce::String&,const juce::StringArray&,const char*,std::unique_ptr<CA>&);
    void selectModule(int module);
    void updateModuleVisibility();
    void toggleModule(int module);
    bool moduleIsOn(int module) const;
    const char* moduleParameter(int module) const;
    void dragModule(int module,int screenX);
    void updateAmpModeUI();
    void resetEQ();

    SDNAProcessor& processor;
    SDNASuperAmpLookAndFeel lf;
    juce::Image logo;
    std::unique_ptr<juce::FileChooser> chooser;
    int selectedModule=SDNAProcessor::Amp;
    bool tunerPanel=false;
    std::array<int,SDNAProcessor::ModuleCount> moduleOrder{};
    std::array<std::unique_ptr<ModuleIcon>,SDNAProcessor::ModuleCount> icons;

    juce::ComboBox inputSelect,ampMode,driveType,verbMode;
    juce::Label ampModeLabel,driveTypeLabel,verbModeLabel;
    std::unique_ptr<CA> inputAtt,ampModeAtt,driveTypeAtt,verbModeAtt;
    juce::TextButton directButton{"DIRECT: OFF"},audioOptions{"AUDIO OPTIONS"},muteButton{"ENABLE AUDIO"},tunerButton{"TUNER"};
    juce::ToggleButton tunerMute{"MUTE"};
    std::vector<std::unique_ptr<BA>> buttonAtts;

    juce::TextButton loadPre{"LOAD PREAMP .NAM"},loadPower{"LOAD POWER AMP .NAM"},loadIRButton{"LOAD USER IR"},eqReset{"FLAT RESET"};
    juce::Label preName,powerName,irLabel,statusLabel,panelTitle;

    Knob preInput,cleanBoost,low,mid,high,preOutput,presence,depth,master;
    Knob driveGain,driveTone,driveBlend,driveLevel;
    Knob irLevel,irLowCut,irHighCut;
    Knob compThreshold,compRatio,compAttack,compRelease,compMakeup,compMix;
    Knob delayTime,delayFeedback,delayMix,delayTone,delayDiffusion,delayMod,delayWidth;
    Knob verbMix,verbDecay,verbPre,verbDamp,verbMod,verbWidth;

    std::array<juce::Slider,10> eqSliders;
    std::array<juce::Label,10> eqLabels;
    std::array<std::unique_ptr<SA>,10> eqAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDNAEditor)
};
