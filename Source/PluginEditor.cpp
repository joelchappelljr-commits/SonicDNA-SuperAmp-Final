#include "PluginEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <cmath>

namespace
{
const auto purple=juce::Colour(0xff7d86d8);
const auto purpleDark=juce::Colour(0xff5c65b7);
const auto silver=juce::Colour(0xffd9dde7);

void drawPanel(juce::Graphics& g,juce::Rectangle<float> r)
{
    g.setColour(juce::Colours::black.withAlpha(.65f));g.fillRoundedRectangle(r.translated(2.f,3.f),8.f);
    juce::ColourGradient grad(juce::Colour(0xff3f4249),r.getX(),r.getY(),juce::Colour(0xff17191d),r.getX(),r.getBottom(),false);
    grad.addColour(.35,juce::Colour(0xff2c2f35));g.setGradientFill(grad);g.fillRoundedRectangle(r,8.f);
    g.setColour(purpleDark);g.drawRoundedRectangle(r,8.f,1.2f);
}
}

SDNASuperAmpLookAndFeel::SDNASuperAmpLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,juce::Colours::whitesmoke);
    setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff111216));
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff4a4e59));
    setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff111216));
    setColour(juce::ComboBox::textColourId,juce::Colours::whitesmoke);
    setColour(juce::ComboBox::outlineColourId,purpleDark);
    setColour(juce::ToggleButton::textColourId,silver);
}

void SDNASuperAmpLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float a0,float a1,juce::Slider&)
{
    const auto c=juce::Point<float>((float)x+w*.5f,(float)y+h*.45f);const float radius=juce::jmin((float)w,(float)h)*.31f;const float angle=a0+pos*(a1-a0);
    const auto outer=juce::Rectangle<float>(c.x-radius-4,c.y-radius-4,2*(radius+4),2*(radius+4));
    g.setColour(juce::Colours::black.withAlpha(.8f));g.fillEllipse(outer.translated(2,3));
    juce::ColourGradient bezel(juce::Colour(0xfff0f2f6),outer.getX(),outer.getY(),juce::Colour(0xff565b64),outer.getRight(),outer.getBottom(),false);bezel.addColour(.5,juce::Colour(0xffa7abb2));g.setGradientFill(bezel);g.fillEllipse(outer);
    auto body=outer.reduced(5.f);juce::ColourGradient knob(juce::Colour(0xff595d66),body.getX(),body.getY(),juce::Colour(0xff090a0d),body.getRight(),body.getBottom(),false);knob.addColour(.45,juce::Colour(0xff252831));g.setGradientFill(knob);g.fillEllipse(body);g.setColour(purple.withAlpha(.65f));g.drawEllipse(body,1.f);
    const auto tip=c+juce::Point<float>(std::sin(angle),-std::cos(angle))*(radius-5.f);g.setColour(juce::Colour(0xffeef2ff));g.drawLine(c.x,c.y,tip.x,tip.y,2.4f);
}

void SDNASuperAmpLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool)
{
    auto r=b.getLocalBounds().toFloat().reduced(2);const bool on=b.getToggleState();auto led=juce::Rectangle<float>(r.getX()+3,r.getCentreY()-6,12,12);g.setColour(juce::Colour(0xff050609));g.fillEllipse(led.expanded(2));g.setColour(on?juce::Colour(0xff8f9cff):juce::Colour(0xff302f3f));g.fillEllipse(led);g.setColour(silver);g.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)));g.drawFittedText(b.getButtonText(),r.withTrimmedLeft(20).toNearestInt(),juce::Justification::centredLeft,1);
}

void SDNAEditor::ModuleIcon::paint(juce::Graphics& g)
{
    auto r=getLocalBounds().toFloat().reduced(3);g.setColour(juce::Colours::black.withAlpha(.65f));g.fillRoundedRectangle(r.translated(1,2),7.f);g.setColour(on?juce::Colour(0xff242830):juce::Colour(0xff15171b));g.fillRoundedRectangle(r,7.f);
    g.setColour(selected?juce::Colour(0xffa6b0ff):juce::Colour(0xff7079c5));g.drawRoundedRectangle(r,7.f,selected?2.2f:1.3f);
    const float cx=r.getCentreX(),cy=r.getCentreY()-4;g.setColour(on?accent:accent.withAlpha(.26f));
    switch(module)
    {
        case SDNAProcessor::Drive:{auto p=juce::Rectangle<float>(cx-19,cy-18,38,32);g.drawRoundedRectangle(p,4,2);g.fillEllipse(cx-3,cy+4,6,6);g.drawLine(cx-10,cy-8,cx+10,cy-8,2);break;}
        case SDNAProcessor::Amp:{g.drawRoundedRectangle(cx-25,cy-15,50,25,4,2);g.drawLine(cx-20,cy-7,cx+20,cy-7,2);for(int i=0;i<4;++i)g.fillEllipse(cx-16+i*10,cy,4,4);break;}
        case SDNAProcessor::EQ:{for(int i=0;i<5;++i){float x=cx-20+i*10;g.drawLine(x,cy-16,x,cy+14,1.5f);float y=cy+((i%3)-1)*7;g.fillEllipse(x-3,y-3,6,6);}break;}
        case SDNAProcessor::IR:{g.drawRoundedRectangle(cx-19,cy-18,38,35,3,2);g.drawEllipse(cx-12,cy-11,24,24,2);g.fillEllipse(cx-4,cy-3,8,8);break;}
        case SDNAProcessor::Comp:{g.drawRoundedRectangle(cx-23,cy-15,46,28,3,2);juce::Path arc;arc.addCentredArc(cx,cy+1,13,10,0.f,3.65f,5.78f,true);g.strokePath(arc,juce::PathStrokeType(2.f));g.drawLine(cx,cy+2,cx+7,cy-6,2);break;}
        case SDNAProcessor::Delay:{g.drawEllipse(cx-18,cy-18,36,36,2);g.drawLine(cx,cy,cx,cy-11,2);g.drawLine(cx,cy,cx+9,cy+5,2);g.drawArrow({cx-27,cy+14,cx-14,cy+14},1.8,6,6);break;}
        case SDNAProcessor::Space:{for(int i=0;i<3;++i){juce::Path arc;const float rr=9.f+i*7.f;arc.addCentredArc(cx,cy,rr,rr,0.f,-.8f,.8f,true);g.strokePath(arc,juce::PathStrokeType(2.f));}g.fillEllipse(cx-4,cy-4,8,8);break;}
        default:break;
    }
    g.setColour(on?silver:silver.withAlpha(.4f));g.setFont(juce::Font(juce::FontOptions(10.5f,juce::Font::bold)));g.drawText(title,5,getHeight()-23,getWidth()-10,18,juce::Justification::centred);
    auto pa=powerArea();g.setColour(juce::Colour(0xff07080b));g.fillEllipse(pa.expanded(2));g.setColour(on?accent:juce::Colour(0xff30323a));g.fillEllipse(pa);g.setColour(juce::Colour(0xffc9cbd2));g.drawEllipse(pa,1);
}

void SDNAEditor::ModuleIcon::mouseDown(const juce::MouseEvent& e)
{
    if(powerArea().contains(e.position)){if(onToggle)onToggle(module);return;} dragging=true;if(onSelect)onSelect(module);
}
void SDNAEditor::ModuleIcon::mouseDrag(const juce::MouseEvent& e){if(dragging&&onDrag)onDrag(module,e.getScreenPosition().x);}

SDNAEditor::SDNAEditor(SDNAProcessor& p):AudioProcessorEditor(&p),processor(p)
{
    setLookAndFeel(&lf);setResizable(true,true);setResizeLimits(1040,620,1600,1000);
    logo=juce::ImageFileFormat::loadFrom(BinaryData::SonicDNA_png,BinaryData::SonicDNA_pngSize);
    moduleOrder=processor.getModuleOrder();

    const std::array<juce::String,7> names{"DRIVE","AMP","10-BAND EQ","USER IR","STUDIO COMP","DELAY","SPACE"};
    const std::array<juce::Colour,7> colors{juce::Colour(0xff8c78ff),juce::Colour(0xff6cb4ff),juce::Colour(0xffb8c0d5),juce::Colour(0xff7990e8),juce::Colour(0xff9ba4c8),juce::Colour(0xff677bff),juce::Colour(0xff9b68e8)};
    for(int i=0;i<7;++i){icons[(size_t)i]=std::make_unique<ModuleIcon>(i,names[(size_t)i],colors[(size_t)i]);icons[(size_t)i]->onSelect=[this](int m){selectModule(m);};icons[(size_t)i]->onToggle=[this](int m){toggleModule(m);};icons[(size_t)i]->onDrag=[this](int m,int x){dragModule(m,x);};addAndMakeVisible(*icons[(size_t)i]);}

    addKnob(preInput,"INPUT","preInput"," dB");addKnob(cleanBoost,"BOOST","drive"," dB");addKnob(low,"LOW","low"," dB");addKnob(mid,"MID","mid"," dB");addKnob(high,"HIGH","high"," dB");addKnob(preOutput,"PRE OUT","preOutput"," dB");addKnob(presence,"PRESENCE","presence"," dB");addKnob(depth,"DEPTH","depth"," dB");addKnob(master,"MASTER","master"," dB");
    addKnob(driveGain,"GAIN","pedalGain","%");addKnob(driveTone,"TONE","pedalTone","%");addKnob(driveBlend,"BLEND","pedalBlend","%");addKnob(driveLevel,"LEVEL","pedalLevel"," dB");
    addKnob(irLevel,"LEVEL","irLevel"," dB");addKnob(irLowCut,"LOW CUT","irLowCut"," Hz");addKnob(irHighCut,"HIGH CUT","irHighCut"," Hz");
    addKnob(compThreshold,"THRESHOLD","compThreshold"," dB");addKnob(compRatio,"RATIO","compRatio",":1");addKnob(compAttack,"ATTACK","compAttack"," ms");addKnob(compRelease,"RELEASE","compRelease"," ms");addKnob(compMakeup,"MAKEUP","compMakeup"," dB");addKnob(compMix,"MIX","compMix","%");
    addKnob(delayTime,"TIME","echoTime"," ms");addKnob(delayFeedback,"FEEDBACK","echoFeedback","%");addKnob(delayMix,"MIX","echoMix","%");addKnob(delayTone,"TONE","echoTone","%");addKnob(delayDiffusion,"DIFFUSION","echoDiffusion","%");addKnob(delayMod,"MOD","echoMod","%");addKnob(delayWidth,"WIDTH","echoWidth","%");
    addKnob(verbMix,"MIX","verbMix","%");addKnob(verbDecay,"DECAY","verbDecay"," s");addKnob(verbPre,"PRE-DELAY","verbPre"," ms");addKnob(verbDamp,"DAMPING","verbDamp"," Hz");addKnob(verbMod,"MOD","verbMod","%");addKnob(verbWidth,"WIDTH","verbWidth","%");

    setupCombo(ampMode,ampModeLabel,"MODE",{"PAIRED NAM","SINGLE NAM"},"ampMode",ampModeAtt);ampMode.onChange=[this]{updateAmpModeUI();};
    setupCombo(driveType,driveTypeLabel,"DRIVE TYPE",{"Tube Screamer","Klon","Timmy"},"pedalType",driveTypeAtt);
    setupCombo(verbMode,verbModeLabel,"SPACE TYPE",{"Hall","Plate","Room","Cloud","Lush Hall","Modulated Hall"},"verbMode",verbModeAtt);
    inputSelect.addItem("Input 1",1);inputSelect.addItem("Input 2",2);addAndMakeVisible(inputSelect);inputAtt=std::make_unique<CA>(processor.parameters,"inputChannel",inputSelect);

    for(auto* c:{(juce::Component*)&directButton,(juce::Component*)&audioOptions,(juce::Component*)&muteButton,(juce::Component*)&tunerButton,(juce::Component*)&tunerMute,(juce::Component*)&loadPre,(juce::Component*)&loadPower,(juce::Component*)&loadIRButton,(juce::Component*)&eqReset,(juce::Component*)&preName,(juce::Component*)&powerName,(juce::Component*)&irLabel,(juce::Component*)&statusLabel,(juce::Component*)&panelTitle})addAndMakeVisible(*c);
    buttonAtts.push_back(std::make_unique<BA>(processor.parameters,"tunerMute",tunerMute));

    panelTitle.setColour(juce::Label::textColourId,silver);panelTitle.setFont(juce::Font(juce::FontOptions(17.f,juce::Font::bold)));preName.setColour(juce::Label::textColourId,silver);powerName.setColour(juce::Label::textColourId,silver);irLabel.setColour(juce::Label::textColourId,silver);statusLabel.setColour(juce::Label::textColourId,juce::Colour(0xffd5d8e3));
    loadPre.onClick=[this]{chooseNAM(true);};loadPower.onClick=[this]{chooseNAM(false);};loadIRButton.onClick=[this]{chooseIR();};eqReset.onClick=[this]{resetEQ();};
    directButton.onClick=[this]{processor.setDirectMonitor(!processor.isDirectMonitor());timerCallback();};muteButton.onClick=[this]{processor.setAudioMuted(!processor.isAudioMuted());timerCallback();};tunerButton.onClick=[this]{tunerPanel=true;updateModuleVisibility();repaint();};
    audioOptions.setVisible(juce::JUCEApplication::isStandaloneApp());audioOptions.onClick=[](){if(juce::JUCEApplication::isStandaloneApp())if(auto* holder=juce::StandalonePluginHolder::getInstance())holder->showAudioSettingsDialog();};

    const char* eqIds[]={"eq31","eq62","eq125","eq250","eq500","eq1k","eq2k","eq4k","eq8k","eq16k"};const char* eqNames[]={"31","62","125","250","500","1k","2k","4k","8k","16k"};
    for(int i=0;i<10;++i){auto& s=eqSliders[(size_t)i];s.setSliderStyle(juce::Slider::LinearVertical);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,48,20);s.setColour(juce::Slider::thumbColourId,juce::Colour(0xff8895ee));s.setColour(juce::Slider::trackColourId,juce::Colour(0xff666c82));addAndMakeVisible(s);eqAttachments[(size_t)i]=std::make_unique<SA>(processor.parameters,eqIds[i],s);auto& l=eqLabels[(size_t)i];l.setText(eqNames[i],juce::dontSendNotification);l.setJustificationType(juce::Justification::centred);l.setColour(juce::Label::textColourId,silver);addAndMakeVisible(l);}

    setSize(1120,700);selectModule(SDNAProcessor::Amp);updateAmpModeUI();startTimerHz(6);timerCallback();
}

SDNAEditor::~SDNAEditor(){stopTimer();setLookAndFeel(nullptr);}

void SDNAEditor::addKnob(Knob& k,const juce::String& title,const char* id,const juce::String& suffix)
{
    k.label.setText(title,juce::dontSendNotification);k.label.setJustificationType(juce::Justification::centred);k.label.setColour(juce::Label::textColourId,silver);k.label.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)));k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,20);k.slider.setTextValueSuffix(suffix);addAndMakeVisible(k.label);addAndMakeVisible(k.slider);k.att=std::make_unique<SA>(processor.parameters,id,k.slider);
}

void SDNAEditor::setupCombo(juce::ComboBox& box,juce::Label& label,const juce::String& title,const juce::StringArray& items,const char* id,std::unique_ptr<CA>& att)
{
    int n=1;for(const auto& item:items)box.addItem(item,n++);addAndMakeVisible(box);label.setText(title,juce::dontSendNotification);label.setJustificationType(juce::Justification::centredLeft);label.setColour(juce::Label::textColourId,silver);label.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)));addAndMakeVisible(label);att=std::make_unique<CA>(processor.parameters,id,box);
}

const char* SDNAEditor::moduleParameter(int m) const
{
    switch(m){case SDNAProcessor::Drive:return "pedalOn";case SDNAProcessor::Amp:return "ampOn";case SDNAProcessor::EQ:return "eqOn";case SDNAProcessor::IR:return "irOn";case SDNAProcessor::Comp:return "compOn";case SDNAProcessor::Delay:return "echoOn";case SDNAProcessor::Space:return "verbOn";default:return "";}
}

bool SDNAEditor::moduleIsOn(int m) const { if(auto* v=processor.parameters.getRawParameterValue(moduleParameter(m)))return v->load()>.5f;return true; }

void SDNAEditor::toggleModule(int m)
{
    if(auto* p=processor.parameters.getParameter(moduleParameter(m))){const bool on=moduleIsOn(m);p->beginChangeGesture();p->setValueNotifyingHost(on?0.f:1.f);p->endChangeGesture();}timerCallback();
}

void SDNAEditor::selectModule(int m){selectedModule=m;tunerPanel=false;updateModuleVisibility();repaint();}

void SDNAEditor::updateModuleVisibility()
{
    auto hideKnob=[](Knob& k,bool show){k.slider.setVisible(show);k.label.setVisible(show);};
    const bool amp=!tunerPanel&&selectedModule==SDNAProcessor::Amp,drive=!tunerPanel&&selectedModule==SDNAProcessor::Drive,eq=!tunerPanel&&selectedModule==SDNAProcessor::EQ,ir=!tunerPanel&&selectedModule==SDNAProcessor::IR,comp=!tunerPanel&&selectedModule==SDNAProcessor::Comp,del=!tunerPanel&&selectedModule==SDNAProcessor::Delay,space=!tunerPanel&&selectedModule==SDNAProcessor::Space;
    ampMode.setVisible(amp);ampModeLabel.setVisible(amp);loadPre.setVisible(amp);loadPower.setVisible(amp);preName.setVisible(amp);powerName.setVisible(amp);for(auto* k:{&preInput,&cleanBoost,&low,&mid,&high,&preOutput,&presence,&depth,&master})hideKnob(*k,amp);
    driveType.setVisible(drive);driveTypeLabel.setVisible(drive);for(auto* k:{&driveGain,&driveTone,&driveBlend,&driveLevel})hideKnob(*k,drive);
    for(int i=0;i<10;++i){eqSliders[(size_t)i].setVisible(eq);eqLabels[(size_t)i].setVisible(eq);}eqReset.setVisible(eq);
    loadIRButton.setVisible(ir);irLabel.setVisible(ir);for(auto* k:{&irLevel,&irLowCut,&irHighCut})hideKnob(*k,ir);
    for(auto* k:{&compThreshold,&compRatio,&compAttack,&compRelease,&compMakeup,&compMix})hideKnob(*k,comp);
    for(auto* k:{&delayTime,&delayFeedback,&delayMix,&delayTone,&delayDiffusion,&delayMod,&delayWidth})hideKnob(*k,del);
    verbMode.setVisible(space);verbModeLabel.setVisible(space);for(auto* k:{&verbMix,&verbDecay,&verbPre,&verbDamp,&verbMod,&verbWidth})hideKnob(*k,space);
    tunerMute.setVisible(tunerPanel);
    static const char* titles[]={"DRIVE","AMP / NAM","10-BAND GRAPHIC EQ","USER CABINET IR","STUDIO COMPRESSOR","DELAY RACK","SDNA SPACE"};panelTitle.setText(tunerPanel?"SDNA TUNER":titles[selectedModule],juce::dontSendNotification);
    for(int i=0;i<7;++i){if(icons[(size_t)i]){icons[(size_t)i]->selected=!tunerPanel&&i==selectedModule;icons[(size_t)i]->repaint();}}
    updateAmpModeUI();resized();
}

void SDNAEditor::updateAmpModeUI()
{
    const bool paired=ampMode.getSelectedId()!=2;loadPre.setButtonText(paired?"LOAD PREAMP .NAM":"LOAD SINGLE .NAM");loadPower.setEnabled(paired);powerName.setAlpha(paired?1.f:.35f);for(auto* k:{&low,&mid,&high,&preOutput,&presence,&depth}){k->slider.setEnabled(paired);k->label.setAlpha(paired?1.f:.35f);}
}

void SDNAEditor::chooseNAM(bool isPre)
{
    const bool paired=ampMode.getSelectedId()!=2;if(!isPre&&!paired)return;const juce::String title=isPre?(paired?"Select preamp-only NAM":"Select single full-amp NAM"):"Select power-amp-only NAM";chooser=std::make_unique<juce::FileChooser>(title,juce::File(),"*.nam");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<SDNAEditor>(this),isPre](const juce::FileChooser& fc){if(!safe||!fc.getResult().existsAsFile())return;juce::String err;if(!safe->processor.loadModel(isPre,fc.getResult(),err))safe->statusLabel.setText(err,juce::dontSendNotification);else safe->statusLabel.setText("Loaded "+fc.getResult().getFileName(),juce::dontSendNotification);safe->timerCallback();});
}

void SDNAEditor::chooseIR()
{
    chooser=std::make_unique<juce::FileChooser>("Select user cabinet IR",juce::File(),"*.wav;*.aif;*.aiff");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<SDNAEditor>(this)](const juce::FileChooser& fc){if(!safe||!fc.getResult().existsAsFile())return;juce::String err;if(!safe->processor.loadIR(fc.getResult(),err))safe->statusLabel.setText(err,juce::dontSendNotification);else safe->statusLabel.setText("Loaded user IR "+fc.getResult().getFileName(),juce::dontSendNotification);safe->timerCallback();});
}

void SDNAEditor::resetEQ(){const char* ids[]={"eq31","eq62","eq125","eq250","eq500","eq1k","eq2k","eq4k","eq8k","eq16k"};for(auto* id:ids)if(auto* p=processor.parameters.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(0.5f);p->endChangeGesture();}}

void SDNAEditor::dragModule(int module,int screenX)
{
    int current=0;for(int i=0;i<7;++i)if(moduleOrder[(size_t)i]==module){current=i;break;}int target=current;int best=1000000;for(int i=0;i<7;++i){auto* icon=icons[(size_t)moduleOrder[(size_t)i]].get();if(icon==nullptr)continue;const int cx=icon->localPointToGlobal(icon->getLocalBounds().getCentre()).x;const int d=std::abs(cx-screenX);if(d<best){best=d;target=i;}}if(target!=current){const int value=moduleOrder[(size_t)current];if(target>current)for(int i=current;i<target;++i)moduleOrder[(size_t)i]=moduleOrder[(size_t)i+1];else for(int i=current;i>target;--i)moduleOrder[(size_t)i]=moduleOrder[(size_t)i-1];moduleOrder[(size_t)target]=value;processor.setModuleOrder(moduleOrder);resized();repaint();}
}

void SDNAEditor::timerCallback()
{
    preName.setText(processor.modelName(true),juce::dontSendNotification);powerName.setText(processor.modelName(false),juce::dontSendNotification);irLabel.setText(processor.irName(),juce::dontSendNotification);directButton.setButtonText(processor.isDirectMonitor()?"DIRECT: ON":"DIRECT: OFF");muteButton.setButtonText(processor.isAudioMuted()?"ENABLE AUDIO":"MUTE AUDIO");for(int i=0;i<7;++i){if(icons[(size_t)i]){icons[(size_t)i]->on=moduleIsOn(i);icons[(size_t)i]->repaint();}}if(processor.status().isNotEmpty())statusLabel.setText(processor.status(),juce::dontSendNotification);repaint();
}

void SDNAEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff08090c));juce::ColourGradient bg(juce::Colour(0xff282b31),0,0,juce::Colour(0xff0c0d10),0,(float)getHeight(),false);g.setGradientFill(bg);g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(5),12.f);g.setColour(juce::Colour(0xff41454f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(5),12.f,2.f);
    auto header=juce::Rectangle<float>(18,12,(float)getWidth()-36,78);drawPanel(g,header);if(logo.isValid())g.drawImageWithin(logo,25,19,210,58,juce::RectanglePlacement::centred);g.setColour(silver);g.setFont(juce::Font(juce::FontOptions(21.f,juce::Font::bold)));g.drawText("SUPERAMP",240,24,150,27,juce::Justification::centredLeft);g.setColour(juce::Colour(0xffaab1ef));g.setFont(juce::Font(juce::FontOptions(10.f,juce::Font::bold)));g.drawText("MODULAR NAM PLATFORM",241,52,175,18,juce::Justification::centredLeft);
    const int meterX=getWidth()-350;auto meter=[&](int x,const juce::String& name,float value){g.setColour(silver);g.setFont(juce::Font(juce::FontOptions(9.f,juce::Font::bold)));g.drawText(name,x,27,82,15,juce::Justification::centred);g.setColour(juce::Colour(0xff090a0c));g.fillRoundedRectangle((float)x,47.f,82.f,8.f,2.f);g.setColour(value>.98f?juce::Colour(0xffec4a55):purple);g.fillRoundedRectangle((float)x,47.f,82.f*juce::jlimit(0.f,1.f,value),8.f,2.f);};meter(meterX,"INPUT",processor.inputPeak());meter(meterX+90,"OUTPUT",processor.outputPeak());meter(meterX+180,"CPU "+juce::String(juce::roundToInt(processor.cpuPercent()))+"%",processor.cpuPercent()/100.f);

    auto chain=juce::Rectangle<float>(18,98,(float)getWidth()-36,130);drawPanel(g,chain);g.setColour(juce::Colour(0xff666fbe));g.drawLine(chain.getX()+42,chain.getCentreY()-2,chain.getRight()-42,chain.getCentreY()-2,3.f);g.setColour(juce::Colour(0xffb9c0ff));g.fillEllipse(chain.getX()+28,chain.getCentreY()-7,10,10);g.drawArrow({chain.getRight()-50,chain.getCentreY()-2,chain.getRight()-24,chain.getCentreY()-2},2,8,8);

    auto lower=juce::Rectangle<float>(18,240,(float)getWidth()-36,(float)getHeight()-292);drawPanel(g,lower);g.setColour(juce::Colour(0xff8993df));g.fillRoundedRectangle(lower.getX()+14,lower.getY()+14,4,24,2);g.setColour(silver);

    if(tunerPanel)
    {
        const float hz=processor.tunerFrequency();const float cx=lower.getCentreX(),cy=lower.getCentreY()+15;g.setColour(juce::Colour(0xff0b0c10));g.fillRoundedRectangle(cx-230,cy-95,460,190,10);g.setColour(purpleDark);g.drawRoundedRectangle(cx-230,cy-95,460,190,10,1.5f);juce::String note="PLAY A NOTE";float cents=0.f;if(hz>65.f&&hz<1200.f){const double midi=69.0+12.0*std::log2((double)hz/440.0);const int n=juce::roundToInt(midi);static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};note=juce::String(names[(n%12+12)%12])+juce::String(n/12-1);cents=(float)((midi-n)*100.0);}g.setColour(silver);g.setFont(juce::Font(juce::FontOptions(42.f,juce::Font::bold)));g.drawText(note,(int)cx-100,(int)cy-77,200,58,juce::Justification::centred);g.setFont(juce::Font(juce::FontOptions(15.f,juce::Font::bold)));g.drawText(juce::String(cents,1)+" cents",(int)cx-80,(int)cy-19,160,24,juce::Justification::centred);for(int i=-5;i<=5;++i){float x=cx+i*34.f;g.setColour(i==0?juce::Colour(0xffe5e8ff):juce::Colour(0xff646a79));g.drawLine(x,cy+20,x,cy+(i==0?55:44),i==0?2.f:1.f);}const float needle=cx+juce::jlimit(-50.f,50.f,cents)*3.4f;g.setColour(std::abs(cents)<4.f?juce::Colour(0xff4de18b):juce::Colour(0xffa594ff));g.fillRoundedRectangle(needle-3,cy+16,6,44,2);
    }

    g.setColour(juce::Colour(0xffa2a6b3));g.setFont(juce::Font(juce::FontOptions(10.f)));g.drawText("Drag modules to change the processing order. Click an icon to edit it; click its small lamp to bypass.",28,getHeight()-42,getWidth()-56,16,juce::Justification::centredLeft);
}

void SDNAEditor::resized()
{
    inputSelect.setBounds(420,24,92,25);directButton.setBounds(420,54,92,23);audioOptions.setBounds(520,24,112,25);muteButton.setBounds(520,54,112,23);tunerButton.setBounds(640,24,90,25);
    const int chainX=42,chainY=116,chainW=getWidth()-84,slot=chainW/7,iconW=juce::jmin(118,slot-8),iconH=92;
    for(int pos=0;pos<7;++pos){int module=moduleOrder[(size_t)pos];if(auto* icon=icons[(size_t)module].get())icon->setBounds(chainX+pos*slot+(slot-iconW)/2,chainY,iconW,iconH);}
    auto panel=juce::Rectangle<int>(36,288,getWidth()-72,getHeight()-350);panelTitle.setBounds(47,249,350,30);
    auto knobRow=[&](juce::Rectangle<int> r,std::initializer_list<Knob*> list){int count=(int)list.size(),w=r.getWidth()/juce::jmax(1,count),i=0;for(auto* k:list){auto c=r.removeFromLeft(i==count-1?r.getWidth():w);k->label.setBounds(c.removeFromTop(24));k->slider.setBounds(c.reduced(4,0));++i;}};
    if(!tunerPanel&&selectedModule==SDNAProcessor::Amp){auto top=panel.removeFromTop(52);ampModeLabel.setBounds(top.removeFromLeft(46));ampMode.setBounds(top.removeFromLeft(145).reduced(3,10));loadPre.setBounds(top.removeFromLeft(150).reduced(4,10));preName.setBounds(top.removeFromLeft(210).reduced(4,10));loadPower.setBounds(top.removeFromLeft(165).reduced(4,10));powerName.setBounds(top.reduced(4,10));auto upper=panel.removeFromTop(panel.getHeight()/2);knobRow(upper,{&preInput,&cleanBoost,&low,&mid,&high,&preOutput});knobRow(panel,{&presence,&depth,&master});}
    else if(!tunerPanel&&selectedModule==SDNAProcessor::Drive){auto top=panel.removeFromTop(48);driveTypeLabel.setBounds(top.removeFromLeft(82));driveType.setBounds(top.removeFromLeft(170).reduced(3,9));knobRow(panel,{&driveGain,&driveTone,&driveBlend,&driveLevel});}
    else if(!tunerPanel&&selectedModule==SDNAProcessor::EQ){eqReset.setBounds(panel.getRight()-110,panel.getY()+2,100,26);auto r=panel.reduced(20,35);int w=r.getWidth()/10;for(int i=0;i<10;++i){auto c=r.removeFromLeft(i==9?r.getWidth():w);eqLabels[(size_t)i].setBounds(c.removeFromTop(22));eqSliders[(size_t)i].setBounds(c.reduced(4));}}
    else if(!tunerPanel&&selectedModule==SDNAProcessor::IR){auto top=panel.removeFromTop(48);loadIRButton.setBounds(top.removeFromLeft(130).reduced(3,8));irLabel.setBounds(top.reduced(8,8));knobRow(panel,{&irLevel,&irLowCut,&irHighCut});}
    else if(!tunerPanel&&selectedModule==SDNAProcessor::Comp)knobRow(panel,{&compThreshold,&compRatio,&compAttack,&compRelease,&compMakeup,&compMix});
    else if(!tunerPanel&&selectedModule==SDNAProcessor::Delay)knobRow(panel,{&delayTime,&delayFeedback,&delayMix,&delayTone,&delayDiffusion,&delayMod,&delayWidth});
    else if(!tunerPanel&&selectedModule==SDNAProcessor::Space){auto top=panel.removeFromTop(48);verbModeLabel.setBounds(top.removeFromLeft(82));verbMode.setBounds(top.removeFromLeft(190).reduced(3,9));knobRow(panel,{&verbMix,&verbDecay,&verbPre,&verbDamp,&verbMod,&verbWidth});}
    tunerMute.setBounds(getWidth()/2+145,getHeight()/2+88,75,24);statusLabel.setBounds(180,getHeight()-44,getWidth()-210,18);
}
