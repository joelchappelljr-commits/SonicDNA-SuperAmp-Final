#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace sdna {
// Three original drive voicings inspired by familiar pedal EQ curves.
// State is separate for each channel so a mono guitar routed to stereo stays centered.
class StereoDrive {
public:
    void prepare(double sampleRate) {
        fs=std::max(8000.f,static_cast<float>(sampleRate));
        smoothing=1.f-std::exp(-1.f/(.018f*fs));
        channels={}; enabled=0.f; drive=.3f; tone=.5f; blend=1.f; level=0.f;
    }
    void set(int voiceIn,bool on,float driveIn,float toneIn,float blendIn,float levelDbIn) {
        voice=std::clamp(voiceIn,0,2); targetOn=on?1.f:0.f;
        targetDrive=std::clamp(driveIn,0.f,1.f); targetTone=std::clamp(toneIn,0.f,1.f);
        targetBlend=std::clamp(blendIn,0.f,1.f); targetLevel=std::clamp(levelDbIn,-18.f,12.f);
    }
    void process(float& left,float& right) {
        enabled+=(targetOn-enabled)*smoothing;
        drive+=(targetDrive-drive)*smoothing; tone+=(targetTone-tone)*smoothing;
        blend+=(targetBlend-blend)*smoothing; level+=(targetLevel-level)*smoothing;
        const float dryL=left,dryR=right;
        left=processChannel(dryL,channels[0]);right=processChannel(dryR,channels[1]);
        const float wetMix=enabled*blend;
        left=dryL+(left-dryL)*wetMix; right=dryR+(right-dryR)*wetMix;
    }
private:
    struct Channel { float highpassInput=0.f,highpassOutput=0.f,preLowpass=0.f,postLowpass=0.f; };
    std::array<Channel,2> channels{};
    float fs=48000.f,smoothing=.001f;
    int voice=0;
    float enabled=0.f,drive=.3f,tone=.5f,blend=1.f,level=0.f;
    float targetOn=0.f,targetDrive=.3f,targetTone=.5f,targetBlend=1.f,targetLevel=0.f;
    static float pole(float hz,float rate) { return std::exp(-6.28318530718f*hz/rate); }
    float processChannel(float input,Channel& ch) {
        const float highpassHz=voice==0?620.f:(voice==1?150.f:65.f);
        const float hp=pole(highpassHz,fs);
        ch.highpassOutput=hp*(ch.highpassOutput+input-ch.highpassInput);
        ch.highpassInput=input;
        const float preCut=voice==0?4400.f:(voice==1?8400.f:11500.f);
        const float prePole=pole(preCut,fs);
        ch.preLowpass=prePole*ch.preLowpass+(1.f-prePole)*ch.highpassOutput;
        const float gain=(voice==0?1.9f:(voice==1?1.35f:1.15f))
                         *std::pow(10.f,drive*(voice==0?1.45f:1.7f));
        const float soft=std::tanh(ch.preLowpass*gain);
        // Mix a little input back inside the Klon-style voice for a more open attack.
        const float character=voice==1 ? .78f*soft+.22f*input : soft;
        const float toneHz=voice==0 ? 1400.f+tone*6200.f : 1800.f+tone*11500.f;
        const float lp=pole(std::min(toneHz,fs*.43f),fs);
        ch.postLowpass=lp*ch.postLowpass+(1.f-lp)*character;
        const float makeup=voice==0?1.55f:(voice==1?1.15f:1.3f);
        const float compensation=std::pow(10.f,-drive*(voice==0?.42f:.5f));
        const float trim=std::pow(10.f,level/20.f);
        return ch.postLowpass*makeup*compensation*trim;
    }
};
}
