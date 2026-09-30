#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace sdna {
class Diffuser {
public:
    void prepare(size_t samples) { buffer.assign(std::max<size_t>(samples,1),0.f); pos=0; }
    float process(float input) {
        const float stored=buffer[pos];
        const float output=stored-.55f*input;
        buffer[pos]=input+.55f*output;
        pos=(pos+1)%buffer.size();
        return output;
    }
private:
    std::vector<float> buffer;
    size_t pos=0;
};
class DiffusionTank {
public:
    void prepare(float sr) {
        constexpr float times[]={.031f,.043f,.059f,.071f};
        for(int i=0;i<4;++i){lines[i].assign(static_cast<size_t>(sr*times[i])+1,0.f);positions[i]=0;filtered[i]=0.f;}
    }
    void process(float left,float right,float density,float& outL,float& outR){
        float v[4],sum=0.f;
        for(int i=0;i<4;++i){
            const float x=lines[i][positions[i]];
            filtered[i]+=.38f*(x-filtered[i]);
            v[i]=filtered[i];sum+=v[i];
        }
        const float gain=.68f+.18f*density;
        for(int i=0;i<4;++i){
            const float injection=(i&1?right:left)*.28f*(i==2||i==3?-1.f:1.f);
            lines[i][positions[i]]=(v[i]-.5f*sum)*gain+injection;
            positions[i]=(positions[i]+1)%lines[i].size();
        }
        outL=.48f*(v[0]-v[1]+v[2]-v[3]);
        outR=.48f*(v[0]+v[1]-v[2]-v[3]);
    }
private:
    std::vector<float> lines[4];size_t positions[4]{};float filtered[4]{};
};
class StereoDelay {
public:
    void prepare(double sampleRate) {
        sr=static_cast<float>(std::max(22050.0,sampleRate));
        const size_t n=static_cast<size_t>(sr*2.6f)+4;
        for(auto& c:channels) c.assign(n,0.f);
        pos=0; wetSmooth=.3f; timeSmooth=sr*.42f; lpL=lpR=0.f;
        diffusionSmooth=modSmooth=0.f;phaseL=phaseR=0.f;
        diffL[0].prepare(static_cast<size_t>(sr*.0083f));
        diffL[1].prepare(static_cast<size_t>(sr*.0177f));
        diffR[0].prepare(static_cast<size_t>(sr*.0117f));
        diffR[1].prepare(static_cast<size_t>(sr*.0231f));
        tank.prepare(sr);
    }
    void set(float milliseconds,float feedback,float wet,float tone,float pingPong,float diffusion=0.f,float modDepth=0.f,float modRate=.35f) {
        targetTime=milliseconds*.001f*sr;
        targetFeedback=clamp(feedback,0.f,.9f);
        targetWet=clamp(wet,0.f,1.f);
        targetTone=clamp(tone,0.f,1.f);
        targetCross=clamp(pingPong,0.f,1.f);
        targetDiffusion=clamp(diffusion,0.f,1.f);
        targetMod=clamp(modDepth,0.f,1.f);
        targetRate=clamp(modRate,.05f,1.5f);
    }
    void clear() {
        for (auto& channel : channels) std::fill(channel.begin(), channel.end(), 0.f);
        lpL=lpR=0.f;
    }
    void process(float& l,float& r) {
        if(channels[0].empty()) return;
        // Slow tap movement avoids discontinuities during tempo/time changes.
        const float timeStep=1.f-std::exp(-1.f/(.08f*sr));
        timeSmooth+=(std::clamp(targetTime,sr*.02f,sr*2.4f)-timeSmooth)*timeStep;
        wetSmooth+=(targetWet-wetSmooth)*.001f;
        diffusionSmooth+=(targetDiffusion-diffusionSmooth)*.001f;
        modSmooth+=(targetMod-modSmooth)*.001f;
        phaseL+=2.f*3.14159265f*targetRate/sr;
        phaseR+=2.f*3.14159265f*(targetRate*.79f)/sr;
        if(phaseL>=2.f*3.14159265f) phaseL-=2.f*3.14159265f;
        if(phaseR>=2.f*3.14159265f) phaseR-=2.f*3.14159265f;
        const float tap=std::clamp(timeSmooth,2.f,float(channels[0].size()-3));
        const float depthSamples=modSmooth*.003f*sr;
        const float leftTap=std::clamp(tap+depthSamples*std::sin(phaseL),2.f,float(channels[0].size()-3));
        const float rightTap=std::clamp(tap*1.5f+depthSamples*std::sin(phaseR+1.1f),2.f,float(channels[1].size()-3));
        const float a=read(channels[0],leftTap),b=read(channels[1],rightTap);
        float spreadL=a,spreadR=b;
        for(auto& stage:diffL) spreadL=stage.process(spreadL);
        for(auto& stage:diffR) spreadR=stage.process(spreadR);
        const float earlyL=a+(spreadL-a)*diffusionSmooth;
        const float earlyR=b+(spreadR-b)*diffusionSmooth;
        float tailL=0.f,tailR=0.f;
        tank.process(earlyL,earlyR,diffusionSmooth,tailL,tailR);
        // Preserve the familiar short diffusion below 70%; toward 100%,
        // replace distinct repeats with a continuously recycling dense tail.
        const float cloud=clamp((diffusionSmooth-.7f)/.3f,0.f,1.f);
        const float wetL=earlyL*(1.f-cloud)+tailL*cloud*1.6f;
        const float wetR=earlyR*(1.f-cloud)+tailR*cloud*1.6f;
        const float cutoff=1200.f+targetTone*8000.f;
        const float alpha=1.f-std::exp(-2.f*3.14159265f*cutoff/sr);
        lpL+=alpha*(wetL-lpL); lpR+=alpha*(wetR-lpR);
        const float fbL=(1.f-targetCross)*lpL+targetCross*lpR;
        const float fbR=(1.f-targetCross)*lpR+targetCross*lpL;
        channels[0][pos]=l+targetFeedback*fbL;
        channels[1][pos]=r+targetFeedback*fbR;
        pos=(pos+1)%channels[0].size();
        // Wet/dry blend. At 100% this is suitable for a parallel bus.
        l=(1.f-wetSmooth)*l+wetSmooth*wetL;
        r=(1.f-wetSmooth)*r+wetSmooth*wetR;
    }
private:
    static float clamp(float x,float lo,float hi){return std::max(lo,std::min(x,hi));}
    float read(const std::vector<float>& c,float distance) const {
        float p=static_cast<float>(pos)-distance;
        while(p<0.f)p+=static_cast<float>(c.size());
        const auto i=static_cast<size_t>(p);
        const float fraction=p-float(i);
        return c[i]*(1.f-fraction)+c[(i+1)%c.size()]*fraction;
    }
    std::vector<float> channels[2];
    Diffuser diffL[2],diffR[2];DiffusionTank tank;
    size_t pos=0;
    float sr=48000.f,targetTime=20160.f,timeSmooth=20160.f;
    float targetFeedback=.35f,targetWet=.3f,wetSmooth=.3f;
    float targetTone=.65f,targetCross=.5f,lpL=0.f,lpR=0.f;
    float targetDiffusion=0.f,diffusionSmooth=0.f;
    float targetMod=0.f,modSmooth=0.f,targetRate=.35f,phaseL=0.f,phaseR=0.f;
};
}
