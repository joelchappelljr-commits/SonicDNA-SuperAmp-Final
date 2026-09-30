#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <NAM/get_dsp.h>
#include <NAM/container.h>
#include <NAM/convnet.h>
#include <NAM/lstm.h>
#include <NAM/model_config.h>
#include <NAM/wavenet/model.h>
#include <chrono>
#include <cmath>
#include <filesystem>

namespace
{
void ensureNamParsers()
{
    static std::once_flag once;
    std::call_once(once, []
    {
        auto& registry = nam::ConfigParserRegistry::instance();
        if (!registry.has("SlimmableContainer")) registry.registerParser("SlimmableContainer", nam::container::create_config);
        if (!registry.has("WaveNet")) registry.registerParser("WaveNet", nam::wavenet::create_config);
        if (!registry.has("LSTM")) registry.registerParser("LSTM", nam::lstm::create_config);
        if (!registry.has("ConvNet")) registry.registerParser("ConvNet", nam::convnet::create_config);
        if (!registry.has("Linear")) registry.registerParser("Linear", nam::linear::create_config);
    });
}

constexpr std::array<double, 10> eqFrequencies { 31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 };
}

SDNAProcessor::SDNAProcessor()
    : AudioProcessor(BusesProperties().withInput("Guitar", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Amp", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", makeParameters()) {}

juce::AudioProcessorEditor* SDNAProcessor::createEditor() { return new SDNAEditor(*this); }

juce::AudioProcessorValueTreeState::ParameterLayout SDNAProcessor::makeParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&p](const char* id, const char* name, float lo, float hi, float initial, float step = 0.1f)
    { p.push_back(std::make_unique<juce::AudioParameterFloat>(id, name, juce::NormalisableRange<float>(lo, hi, step), initial)); };

    p.push_back(std::make_unique<juce::AudioParameterChoice>("ampMode", "NAM Mode",
        juce::StringArray { "Paired NAM", "Single NAM" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("ampOn", "Amp On", true));
    add("preInput", "NAM Input dB", -12, 12, 0);
    add("drive", "Preamp Clean Boost dB", 0, 10, 0);
    add("low", "Preamp Low dB", -12, 12, 0);
    add("mid", "Preamp Mid dB", -12, 12, 0);
    add("high", "Preamp High dB", -12, 12, 0);
    add("preOutput", "Preamp Output Trim dB", -30, 6, 0);
    add("presence", "Power Presence dB", -12, 12, 0);
    add("depth", "Power Depth dB", -12, 12, 0);
    add("master", "Amp Master dB", -30, 30, 0);

    p.push_back(std::make_unique<juce::AudioParameterBool>("pedalOn", "Drive On", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("pedalType", "Drive Type",
        juce::StringArray { "Tube Screamer", "Klon", "Timmy" }, 1));
    add("pedalGain", "Drive Gain %", 0, 100, 26);
    add("pedalTone", "Drive Tone %", 0, 100, 72);
    add("pedalBlend", "Drive Blend %", 0, 100, 75);
    add("pedalLevel", "Drive Level dB", -18, 12, 0);

    p.push_back(std::make_unique<juce::AudioParameterBool>("eqOn", "10 Band EQ On", false));
    const char* eqIds[] = { "eq31", "eq62", "eq125", "eq250", "eq500", "eq1k", "eq2k", "eq4k", "eq8k", "eq16k" };
    const char* eqNames[] = { "31.25 Hz", "62.5 Hz", "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz" };
    for (int i = 0; i < 10; ++i) add(eqIds[i], eqNames[i], -12, 12, 0);

    p.push_back(std::make_unique<juce::AudioParameterBool>("irOn", "User IR On", false));
    add("irLevel", "IR Level dB", -24, 12, 0);
    add("irLowCut", "IR Low Cut Hz", 20, 500, 80, 1.0f);
    add("irHighCut", "IR High Cut Hz", 3000, 20000, 12000, 1.0f);

    p.push_back(std::make_unique<juce::AudioParameterBool>("compOn", "Studio Comp On", false));
    add("compThreshold", "Comp Threshold dB", -48, 0, -22);
    add("compRatio", "Comp Ratio", 1, 12, 3, 0.01f);
    add("compAttack", "Comp Attack ms", 1, 100, 15);
    add("compRelease", "Comp Release ms", 20, 600, 160);
    add("compMakeup", "Comp Makeup dB", -6, 18, 0);
    add("compMix", "Comp Mix %", 0, 100, 100);

    p.push_back(std::make_unique<juce::AudioParameterBool>("echoOn", "Delay On", false));
    add("echoTime", "Delay Time ms", 20, 2400, 420);
    add("echoFeedback", "Delay Feedback %", 0, 90, 36);
    add("echoMix", "Delay Mix %", 0, 100, 20);
    add("echoTone", "Delay Tone %", 0, 100, 65);
    add("echoDiffusion", "Delay Diffusion %", 0, 100, 0);
    add("echoMod", "Delay Modulation %", 0, 100, 0);
    add("echoWidth", "Delay Width %", 0, 100, 55);

    p.push_back(std::make_unique<juce::AudioParameterBool>("verbOn", "Space On", false));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("verbMode", "Space Type",
        juce::StringArray { "Hall", "Plate", "Room", "Cloud", "Lush Hall", "Modulated Hall" }, 4));
    add("verbMix", "Space Mix %", 0, 100, 22);
    add("verbDecay", "Space Decay seconds", 0.5f, 30, 5.5f, 0.01f);
    add("verbPre", "Space Pre Delay ms", 0, 200, 24);
    add("verbDamp", "Space Damping Hz", 800, 16000, 6500, 1.0f);
    add("verbMod", "Space Movement %", 0, 100, 30);
    add("verbWidth", "Space Width %", 0, 150, 100);

    p.push_back(std::make_unique<juce::AudioParameterChoice>("inputChannel", "Guitar Input",
        juce::StringArray { "Input 1", "Input 2" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>("tunerMute", "Tuner Mute", false));
    return { p.begin(), p.end() };
}

bool SDNAProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    const auto in = b.getMainInputChannelSet();
    const auto out = b.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo())
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void SDNAProcessor::prepareToPlay(double sr, int block)
{
    rate = sr; maxBlock = juce::jmax(1, block);
    const juce::dsp::ProcessSpec monoSpec { sr, (juce::uint32) maxBlock, 1 };
    for (auto* f : { &lowShelf, &midPeak, &highShelf, &presence, &depth }) f->prepare(monoSpec);
    for (auto& channel : graphicEQ) for (auto& f : channel) f.prepare(monoSpec);
    for (auto& f : irLowCut) f.prepare(monoSpec);
    for (auto& f : irHighCut) f.prepare(monoSpec);
    cabinetIRL.prepare(monoSpec); cabinetIRR.prepare(monoSpec);
    pedalDrive.prepare(sr); delay.prepare(sr); reverb.prepare(sr); tuner.prepare(sr);
    tunerHz.store(0.0f); cpuLevel.store(0.0f); compEnvelope = 0.0f; compGain = 1.0f; compReduction.store(0.0f);
    ampFiltersReady = false; graphicFiltersReady = false; lastIRFilters = { -1.0f, -1.0f };
    std::lock_guard<std::mutex> lock(modelMutex);
    for (auto* s : { &preOrSingle, &power }) if (s->model) s->model->Reset(sr, maxBlock);
}

void SDNAProcessor::updateAmpFilters()
{
    std::array<float, 5> gains {
        parameters.getRawParameterValue("low")->load(), parameters.getRawParameterValue("mid")->load(),
        parameters.getRawParameterValue("high")->load(), parameters.getRawParameterValue("presence")->load(),
        parameters.getRawParameterValue("depth")->load() };
    if (ampFiltersReady && gains == lastAmpEQ) return;
    lastAmpEQ = gains; ampFiltersReady = true;
    const auto db = [](float x) { return juce::Decibels::decibelsToGain(x); };
    *lowShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(rate, 140.0, 0.707, db(gains[0]));
    *midPeak.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(rate, 750.0, 0.85, db(gains[1]));
    *highShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(rate, 2800.0, 0.707, db(gains[2]));
    *presence.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(rate, 3800.0, 0.707, db(gains[3]));
    *depth.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(rate, 110.0, 0.707, db(gains[4]));
}

void SDNAProcessor::updateGraphicFilters()
{
    const char* ids[] = { "eq31", "eq62", "eq125", "eq250", "eq500", "eq1k", "eq2k", "eq4k", "eq8k", "eq16k" };
    std::array<float, 10> gains{};
    for (int i = 0; i < 10; ++i) gains[(size_t)i] = parameters.getRawParameterValue(ids[i])->load();
    if (graphicFiltersReady && gains == lastGraphicEQ) return;
    lastGraphicEQ = gains; graphicFiltersReady = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 10; ++i)
        {
            const double frequency = juce::jmin(eqFrequencies[(size_t)i], rate * 0.45);
            *graphicEQ[(size_t)ch][(size_t)i].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                rate, frequency, 1.35, juce::Decibels::decibelsToGain(gains[(size_t)i]));
        }
}

void SDNAProcessor::updateIRFilters()
{
    std::array<float, 2> values { parameters.getRawParameterValue("irLowCut")->load(), parameters.getRawParameterValue("irHighCut")->load() };
    if (values == lastIRFilters) return;
    lastIRFilters = values;
    for (int ch = 0; ch < 2; ++ch)
    {
        *irLowCut[(size_t)ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(rate, values[0], 0.707);
        *irHighCut[(size_t)ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(rate, juce::jmin(values[1], (float)(rate * .45)), 0.707);
    }
}

void SDNAProcessor::processDrive(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("pedalOn")->load() < .5f) return;
    const auto value = [this](const char* id) { return parameters.getRawParameterValue(id)->load(); };
    pedalDrive.set((int)value("pedalType"), true, value("pedalGain") * .01f, value("pedalTone") * .01f,
                   value("pedalBlend") * .01f, value("pedalLevel"));
    auto* l = buffer.getWritePointer(0); auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int i = 0; i < buffer.getNumSamples(); ++i) { float a=l[i], b=r?r[i]:a; pedalDrive.process(a,b); l[i]=a; if(r)r[i]=b; }
}

void SDNAProcessor::processAmp(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("ampOn")->load() < .5f) return;
    std::unique_lock<std::mutex> lock(modelMutex, std::try_to_lock);
    if (!lock.owns_lock()) { buffer.clear(); return; }
    const auto value = [this](const char* id) { return parameters.getRawParameterValue(id)->load(); };
    const auto dbGain = [this](const char* id) { return juce::Decibels::decibelsToGain(parameters.getRawParameterValue(id)->load()); };
    const bool paired = value("ampMode") < .5f;
    auto* d = buffer.getWritePointer(0); const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() > 1)
    {
        auto* r = buffer.getReadPointer(1);
        for (int i=0;i<n;++i) d[i] = .5f * (d[i] + r[i]);
    }
    const float inGain = dbGain("preInput") * dbGain("drive");
    for (int i=0;i<n;++i) d[i] *= inGain;
    if (preOrSingle.model) { NAM_SAMPLE* input[] = { d }; NAM_SAMPLE* output[] = { d }; preOrSingle.model->process(input, output, n); }
    if (paired)
    {
        updateAmpFilters();
        const float preOut = dbGain("preOutput");
        for (int i=0;i<n;++i) d[i] = highShelf.processSample(midPeak.processSample(lowShelf.processSample(d[i]))) * preOut;
        float feedPeak=0.f; for(int i=0;i<n;++i) feedPeak=juce::jmax(feedPeak,std::abs(d[i])); powerFeedLevel.store(feedPeak);
        if (power.model) { NAM_SAMPLE* input[] = { d }; NAM_SAMPLE* output[] = { d }; power.model->process(input, output, n); }
        for (int i=0;i<n;++i) d[i] = depth.processSample(presence.processSample(d[i]));
    }
    else powerFeedLevel.store(0.f);
    const float master = dbGain("master");
    float peak=0.f; for(int i=0;i<n;++i){ d[i]*=master; if(!std::isfinite(d[i]))d[i]=0.f; peak=juce::jmax(peak,std::abs(d[i])); }
    ampOutputLevel.store(peak);
    for (int ch=1; ch<buffer.getNumChannels(); ++ch) buffer.copyFrom(ch,0,buffer,0,0,n);
}

void SDNAProcessor::processGraphicEQ(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("eqOn")->load() < .5f) return;
    updateGraphicFilters();
    for(int ch=0; ch<juce::jmin(2,buffer.getNumChannels()); ++ch)
    {
        auto* d=buffer.getWritePointer(ch);
        for(int i=0;i<buffer.getNumSamples();++i){ float x=d[i]; for(auto& f:graphicEQ[(size_t)ch]) x=f.processSample(x); d[i]=x; }
    }
}

void SDNAProcessor::processIR(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("irOn")->load() < .5f) return;
    std::unique_lock<std::mutex> lock(cabinetMutex, std::try_to_lock);
    if (!lock.owns_lock() || cabinetIRL.getCurrentIRSize() <= 0) return;
    updateIRFilters();
    const float gain=juce::Decibels::decibelsToGain(parameters.getRawParameterValue("irLevel")->load());
    const int channels=juce::jmin(2,buffer.getNumChannels());
    for(int ch=0; ch<channels; ++ch)
    {
        juce::dsp::AudioBlock<float> block(buffer); auto one=block.getSingleChannelBlock((size_t)ch); juce::dsp::ProcessContextReplacing<float> ctx(one);
        (ch==0 ? cabinetIRL : cabinetIRR).process(ctx);
        auto* d=buffer.getWritePointer(ch); for(int i=0;i<buffer.getNumSamples();++i) d[i]=irHighCut[(size_t)ch].processSample(irLowCut[(size_t)ch].processSample(d[i]))*gain;
    }
    float peak=0.f; for(int ch=0;ch<channels;++ch){auto* d=buffer.getReadPointer(ch);for(int i=0;i<buffer.getNumSamples();++i)peak=juce::jmax(peak,std::abs(d[i]));} cabinetOutputLevel.store(peak);
}

void SDNAProcessor::processComp(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("compOn")->load() < .5f) { compReduction.store(0.f); return; }
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    const float threshold=value("compThreshold"), ratio=value("compRatio");
    const float attack=std::exp(-1.0f/(juce::jmax(1.0f,value("compAttack"))*.001f*(float)rate));
    const float release=std::exp(-1.0f/(juce::jmax(1.0f,value("compRelease"))*.001f*(float)rate));
    const float mix=value("compMix")*.01f, makeup=juce::Decibels::decibelsToGain(value("compMakeup"));
    auto* l=buffer.getWritePointer(0); auto* r=buffer.getNumChannels()>1?buffer.getWritePointer(1):nullptr;
    for(int i=0;i<buffer.getNumSamples();++i){float rr=r?r[i]:l[i];const float detector=juce::jmax(std::abs(l[i]),std::abs(rr));const float c=detector>compEnvelope?attack:release;compEnvelope=c*compEnvelope+(1.f-c)*detector;const float levelDb=juce::Decibels::gainToDecibels(compEnvelope,-120.f);const float target=juce::Decibels::decibelsToGain(juce::jmin(0.f,(threshold-levelDb)*(1.f-1.f/ratio)));compGain=.97f*compGain+.03f*target;const float wl=l[i]*compGain*makeup,wr=rr*compGain*makeup;l[i]=l[i]*(1.f-mix)+wl*mix;if(r)r[i]=rr*(1.f-mix)+wr*mix;}
    compReduction.store(-juce::Decibels::gainToDecibels(compGain,-60.f));
}

void SDNAProcessor::processDelay(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("echoOn")->load() < .5f) return;
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    delay.set(value("echoTime"),value("echoFeedback")*.01f,value("echoMix")*.01f,value("echoTone")*.01f,value("echoWidth")*.01f,value("echoDiffusion")*.01f,value("echoMod")*.01f,.35f);
    auto* l=buffer.getWritePointer(0); auto* r=buffer.getNumChannels()>1?buffer.getWritePointer(1):nullptr;
    for(int i=0;i<buffer.getNumSamples();++i){float a=l[i],b=r?r[i]:a;delay.process(a,b);l[i]=a;if(r)r[i]=b;}
}

void SDNAProcessor::processSpace(juce::AudioBuffer<float>& buffer)
{
    if (parameters.getRawParameterValue("verbOn")->load() < .5f) return;
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    sdna::Params rp; rp.mix=value("verbMix")*.01f;rp.decay=value("verbDecay");rp.size=1.f;rp.preDelayMs=value("verbPre");rp.dampingHz=value("verbDamp");rp.lowCutHz=145.f;rp.modulation=value("verbMod")*.01f;rp.modulationRate=1.f;rp.width=value("verbWidth")*.01f;
    reverb.setMode((int)value("verbMode"));reverb.setParams(rp);reverb.setEnabled(true);
    auto* l=buffer.getWritePointer(0); auto* r=buffer.getNumChannels()>1?buffer.getWritePointer(1):nullptr;
    for(int i=0;i<buffer.getNumSamples();++i){float a=l[i],b=r?r[i]:a;reverb.process(a,b);l[i]=a;if(r)r[i]=b;}
}

void SDNAProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard; if(buffer.getNumChannels()<1)return; const auto started=std::chrono::steady_clock::now();
    const auto value=[this](const char* id){return parameters.getRawParameterValue(id)->load();};
    const int selected=(value("inputChannel")>.5f&&getTotalNumInputChannels()>1&&buffer.getNumChannels()>1)?1:0; const int n=buffer.getNumSamples();
    if(selected==1)buffer.copyFrom(0,0,buffer,1,0,n); for(int ch=1;ch<buffer.getNumChannels();++ch)buffer.copyFrom(ch,0,buffer,0,0,n);
    auto* in=buffer.getWritePointer(0);float inPeak=0.f;for(int i=0;i<n;++i){if(!std::isfinite(in[i]))in[i]=0.f;inPeak=juce::jmax(inPeak,std::abs(in[i]));tuner.push(in[i]);}inputLevel.store(juce::jmax(inPeak,inputLevel.load()*.88f));tunerHz.store(tuner.frequency());
    if(directMonitor.load()){const bool muted=audioMuted.load()||value("tunerMute")>.5f;if(muted)buffer.clear();outputLevel.store(muted?0.f:inPeak);return;}

    const auto order=getModuleOrder();
    for(const int module:order)
    {
        switch(module){case Drive:processDrive(buffer);break;case Amp:processAmp(buffer);break;case EQ:processGraphicEQ(buffer);break;case IR:processIR(buffer);break;case Comp:processComp(buffer);break;case Delay:processDelay(buffer);break;case Space:processSpace(buffer);break;default:break;}
    }
    float outPeak=0.f;for(int ch=0;ch<juce::jmin(2,buffer.getNumChannels());++ch){auto* d=buffer.getWritePointer(ch);for(int i=0;i<n;++i){if(!std::isfinite(d[i]))d[i]=0.f;outPeak=juce::jmax(outPeak,std::abs(d[i]));}}
    if(audioMuted.load()||value("tunerMute")>.5f){buffer.clear();outPeak=0.f;}outputLevel.store(juce::jmax(outPeak,outputLevel.load()*.88f));
    const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count(),budget=n/rate;if(budget>0.0)cpuLevel.store(juce::jlimit(0.f,999.f,.85f*cpuLevel.load()+.15f*(float)(100.0*elapsed/budget)));
}

bool SDNAProcessor::loadIR(const juce::File& file, juce::String& error)
{
    if(!file.existsAsFile()||!file.hasFileExtension(".wav;.aif;.aiff")){error="Select a WAV or AIFF cabinet IR";return false;}
    std::lock_guard<std::mutex> lock(cabinetMutex);
    cabinetIRL.loadImpulseResponse(file,juce::dsp::Convolution::Stereo::no,juce::dsp::Convolution::Trim::yes,0,juce::dsp::Convolution::Normalise::yes);
    cabinetIRR.loadImpulseResponse(file,juce::dsp::Convolution::Stereo::no,juce::dsp::Convolution::Trim::yes,0,juce::dsp::Convolution::Normalise::yes);
    irFile=file; if(auto* p=parameters.getParameter("irOn"))p->setValueNotifyingHost(1.f); return true;
}

juce::String SDNAProcessor::irName() const { std::lock_guard<std::mutex> lock(cabinetMutex); return irFile.existsAsFile()?irFile.getFileName():"No user IR loaded"; }

bool SDNAProcessor::loadModel(bool isPre, const juce::File& file, juce::String& error)
{
    if(!file.existsAsFile()||!file.hasFileExtension(".nam")){error="Select an existing .nam file";return false;}
    try{ensureNamParsers();auto candidate=nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));if(!candidate||candidate->NumInputChannels()!=1||candidate->NumOutputChannels()!=1){error="This version needs a mono-in, mono-out NAM model";return false;}const auto expected=candidate->GetExpectedSampleRate();if(expected>0&&std::abs(expected-rate)>1.0){error="Model expects "+juce::String(expected,0)+" Hz. Set your DAW/interface to that rate.";return false;}candidate->Reset(rate,maxBlock);std::lock_guard<std::mutex> lock(modelMutex);auto& slot=isPre?preOrSingle:power;slot.model=std::move(candidate);slot.file=file;warning.clear();return true;}catch(const std::exception& ex){error=ex.what();return false;}
}

juce::String SDNAProcessor::modelName(bool isPre) const { std::lock_guard<std::mutex> lock(modelMutex);const auto& slot=isPre?preOrSingle:power;return slot.model?slot.file.getFileNameWithoutExtension():"No capture loaded"; }

SDNAProcessor::ModuleOrder SDNAProcessor::getModuleOrder() const noexcept
{
    ModuleOrder result{}; const auto packed=packedOrder.load(std::memory_order_relaxed); for(int i=0;i<ModuleCount;++i)result[(size_t)i]=(int)((packed>>(i*4))&0xFu); return result;
}

void SDNAProcessor::setModuleOrder(const ModuleOrder& order) noexcept
{
    uint32_t packed=0; for(int i=0;i<ModuleCount;++i)packed|=(uint32_t)(order[(size_t)i]&0xF)<<(i*4); packedOrder.store(packed,std::memory_order_relaxed);
}

void SDNAProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto state=parameters.copyState();{std::lock_guard<std::mutex> lock(modelMutex);state.setProperty("preFile",preOrSingle.file.getFullPathName(),nullptr);state.setProperty("powerFile",power.file.getFullPathName(),nullptr);} {std::lock_guard<std::mutex> lock(cabinetMutex);state.setProperty("irFile",irFile.getFullPathName(),nullptr);} auto order=getModuleOrder();juce::String orderText;for(int i=0;i<ModuleCount;++i){if(i)orderText<<",";orderText<<order[(size_t)i];}state.setProperty("moduleOrder",orderText,nullptr);if(auto xml=state.createXml())copyXmlToBinary(*xml,data);
}

void SDNAProcessor::restoreModels(const juce::ValueTree& state)
{
    for(bool isPre:{true,false}){const auto path=state.getProperty(isPre?"preFile":"powerFile").toString();if(path.isEmpty())continue;const juce::File f(path);if(f.existsAsFile()){juce::String err;if(!loadModel(isPre,f,err))warning=err;}else warning="Saved NAM file missing: "+f.getFileName();}
    const juce::File savedIR(state.getProperty("irFile").toString());if(savedIR.existsAsFile()){juce::String err;if(!loadIR(savedIR,err))warning=err;}
    auto text=state.getProperty("moduleOrder").toString();juce::StringArray parts;parts.addTokens(text,",","");if(parts.size()==ModuleCount){ModuleOrder o{};std::array<bool,ModuleCount> seen{};bool ok=true;for(int i=0;i<ModuleCount;++i){int v=parts[i].getIntValue();if(v<0||v>=ModuleCount||seen[(size_t)v]){ok=false;break;}seen[(size_t)v]=true;o[(size_t)i]=v;}if(ok)setModuleOrder(o);}
}

void SDNAProcessor::setStateInformation(const void* data,int size)
{
    if(auto xml=getXmlFromBinary(data,size)){auto state=juce::ValueTree::fromXml(*xml);if(state.hasType(parameters.state.getType())){parameters.replaceState(state);restoreModels(state);}}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new SDNAProcessor();}
