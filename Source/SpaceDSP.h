#pragma once
#include "HallDSP.h"
#include <array>
#include <cmath>

namespace sdna {
class Plate {
public:
    void prepare(double sampleRate) {
        sr=static_cast<float>(std::max(22050.0,sampleRate));
        for(auto& line:lines)line.prepare(static_cast<size_t>(sr*.30f));
        for(auto& line:pre)line.prepare(static_cast<size_t>(sr*.26f));
        for(int i=0;i<4;++i)inputAP[i].prepare(static_cast<size_t>(sr*inputMs[i]*.001f));
        for(int i=0;i<8;++i)loopAP[i].prepare(static_cast<size_t>(sr*loopMs[i]*.001f));
        lp.fill(0.f);phase.fill(0.f);high.fill(0.f);mixSmooth=.3f;
    }
    void setParams(Params p){params=p;}
    void process(float& l,float& r){
        const float dryL=l,dryR=r;
        mixSmooth+=(clamp(params.mix,0.f,1.f)-mixSmooth)*.001f;
        const float sz=clamp(params.size,.5f,1.7f), decay=clamp(params.decay*.9f,.45f,20.f);
        const float low=1.f-std::exp(-2.f*pi*clamp(params.lowCutHz,20.f,1000.f)/sr);
        const float damp=1.f-std::exp(-2.f*pi*clamp(params.dampingHz*1.18f,900.f,18000.f)/sr);
        const float preSamples=clamp(params.preDelayMs,0.f,200.f)*.001f*sr+1.f;
        pre[0].write(dryL);pre[1].write(dryR);
        const float a=pre[0].read(preSamples),b=pre[1].read(preSamples);
        float inj[4]={a,b,.6f*a+.4f*b,.6f*b+.4f*a};
        for(int j=0;j<4;++j)inj[j]=inputAP[j].process(inj[j]);
        std::array<float,8> v{};float sum=0.f;
        for(int i=0;i<8;++i){
            phase[i]+=2.f*pi*plateRates[i]*clamp(params.modulationRate,.25f,3.f)/sr;
            if(phase[i]>2.f*pi)phase[i]-=2.f*pi;
            const float offset=clamp(params.modulation,0.f,1.f)*.00018f*sr*std::sin(phase[i]);
            const float tap=plateMs[i]*.001f*sr*sz+offset;
            const float x=lines[i].read(tap);
            lp[i]+=damp*(x-lp[i]);v[i]=lp[i];sum+=v[i];
        }
        const float mean=sum*.25f;float wetL=0,wetR=0;
        for(int i=0;i<8;++i){
            const float gain=std::pow(10.f,-3.f*plateMs[i]*.001f*sz/decay);
            const float mixed=(v[i]-mean)*gain+.16f*inj[i%4]*((i&4)?-1.f:1.f);
            lines[i].write(loopAP[i].process(mixed));
            wetL+=v[i]*outL[i];wetR+=v[i]*outR[i];
        }
        wetL*=.34f;wetR*=.34f;
        high[0]+=low*(wetL-high[0]);high[1]+=low*(wetR-high[1]);
        wetL-=high[0];wetR-=high[1];
        const float mid=.5f*(wetL+wetR),side=.5f*(wetL-wetR)*clamp(params.width,0.f,1.5f);
        const float d=std::cos(.5f*pi*mixSmooth),w=std::sin(.5f*pi*mixSmooth);
        l=d*dryL+w*(mid+side);r=d*dryR+w*(mid-side);
    }
private:
    float sr=48000.f,mixSmooth=.3f;Params params;
    std::array<Delay,8> lines;std::array<Delay,2> pre;
    std::array<Allpass,4> inputAP;std::array<Allpass,8> loopAP;
    std::array<float,8> lp{},phase{};std::array<float,2> high{};
    static constexpr float inputMs[4]={3.1f,4.9f,7.7f,11.3f};
    static constexpr float loopMs[8]={4.3f,5.7f,7.1f,8.9f,10.7f,13.3f,15.1f,18.1f};
    static constexpr float plateMs[8]={43.1f,50.9f,60.7f,71.3f,83.9f,97.1f,113.9f,131.7f};
    static constexpr float plateRates[8]={.13f,.17f,.23f,.29f,.31f,.37f,.43f,.47f};
    static constexpr float outL[8]={1.f,-1.f,1.f,1.f,-1.f,1.f,-1.f,-1.f};
    static constexpr float outR[8]={1.f,1.f,-1.f,1.f,1.f,-1.f,-1.f,-1.f};
};
class Room {
public:
    void prepare(double sampleRate){
        sr=static_cast<float>(std::max(22050.0,sampleRate));
        for(auto& d:pre)d.prepare(static_cast<size_t>(sr*.26f));
        for(auto& d:early)d.prepare(static_cast<size_t>(sr*.12f));
        for(auto& d:late)d.prepare(static_cast<size_t>(sr*.14f));
        lp.fill(0.f);high.fill(0.f);mixSmooth=.3f;
    }
    void setParams(Params p){params=p;}
    void process(float& l,float& r){
        const float dryL=l,dryR=r;
        mixSmooth+=(clamp(params.mix,0.f,1.f)-mixSmooth)*.001f;
        const float size=clamp(params.size,.5f,1.7f),rt=clamp(params.decay*.22f,.25f,1.8f);
        const float low=1.f-std::exp(-2.f*pi*clamp(params.lowCutHz,20.f,1000.f)/sr);
        const float damp=1.f-std::exp(-2.f*pi*clamp(params.dampingHz*.85f,600.f,11000.f)/sr);
        const float preSamples=clamp(params.preDelayMs,0.f,200.f)*.001f*sr+1.f;
        pre[0].write(dryL);pre[1].write(dryR);
        const float a=pre[0].read(preSamples),b=pre[1].read(preSamples);
        early[0].write(a);early[1].write(b);
        float reflL=0,reflR=0;
        for(int j=0;j<4;++j){
            reflL+=early[0].read(earlyLms[j]*.001f*sr*size)*earlyGain[j];
            reflR+=early[1].read(earlyRms[j]*.001f*sr*size)*earlyGain[j];
        }
        const float feedL=a*.42f+reflL*.42f,feedR=b*.42f+reflR*.42f;
        std::array<float,4> v{};float sum=0.f;
        for(int i=0;i<4;++i){
            const float x=late[i].read(lateMs[i]*.001f*sr*size);
            lp[i]+=damp*(x-lp[i]);v[i]=lp[i];sum+=v[i];
        }
        const float mean=sum*.5f;
        for(int i=0;i<4;++i){
            const float gain=std::pow(10.f,-3.f*lateMs[i]*.001f*size/rt);
            late[i].write((v[i]-mean)*gain+.18f*((i%2)?feedR:feedL));
        }
        float wetL=reflL*.85f+(v[0]-v[1]+v[2]+v[3])*.55f;
        float wetR=reflR*.85f+(v[0]+v[1]-v[2]+v[3])*.55f;
        high[0]+=low*(wetL-high[0]);high[1]+=low*(wetR-high[1]);
        wetL-=high[0];wetR-=high[1];
        const float mid=.5f*(wetL+wetR),side=.5f*(wetL-wetR)*clamp(params.width,0.f,1.5f);
        const float d=std::cos(.5f*pi*mixSmooth),w=std::sin(.5f*pi*mixSmooth);
        l=d*dryL+w*(mid+side);r=d*dryR+w*(mid-side);
    }
private:
    float sr=48000.f,mixSmooth=.3f;Params params;
    std::array<Delay,2> pre,early;std::array<Delay,4> late;
    std::array<float,4> lp{};std::array<float,2> high{};
    static constexpr float earlyLms[4]={7.3f,13.7f,23.9f,37.3f};
    static constexpr float earlyRms[4]={9.1f,17.3f,27.1f,42.7f};
    static constexpr float earlyGain[4]={.37f,.27f,.18f,.11f};
    static constexpr float lateMs[4]={23.1f,29.9f,37.7f,47.3f};
};
// A diffuse, modulated ambient tail with a slowly blooming stereo layer.
class Cloud {
public:
    void prepare(double sampleRate) {
        sr=static_cast<float>(std::max(22050.0,sampleRate));
        plate.prepare(sampleRate);
        for(auto& d:chorus)d.prepare(static_cast<size_t>(sr*.26f));
        diffusionL[0].prepare(static_cast<size_t>(sr*.027f));
        diffusionL[1].prepare(static_cast<size_t>(sr*.043f));
        diffusionR[0].prepare(static_cast<size_t>(sr*.031f));
        diffusionR[1].prepare(static_cast<size_t>(sr*.049f));
        smooth={0.f,0.f};phase={0.f,1.7f};mixSmooth=.3f;
    }
    void setParams(Params p) {
        params=p;
        p.mix=1.f;p.decay=clamp(p.decay*1.85f,.7f,30.f);
        p.size=clamp(p.size*1.15f,.5f,1.7f);
        p.preDelayMs=clamp(p.preDelayMs+25.f,0.f,200.f);
        p.modulation=clamp(.3f+p.modulation*.7f,0.f,1.f);
        p.dampingHz=clamp(p.dampingHz*.62f,600.f,12000.f);
        plate.setParams(p);
    }
    void process(float& l,float& r) {
        const float dryL=l,dryR=r;
        float a=l,b=r;plate.process(a,b);
        const float rate=clamp(params.modulationRate,.25f,3.f);
        const float depth=clamp(params.modulation,0.f,1.f);
        for(int i=0;i<2;++i) {
            phase[i]+=2.f*pi*(i==0?.19f:.23f)*rate/sr;
            if(phase[i]>2.f*pi)phase[i]-=2.f*pi;
            chorus[i].write(i==0?a:b);
        }
        const float tapL=chorus[0].read(sr*(.115f+.012f*depth*std::sin(phase[0])));
        const float tapR=chorus[1].read(sr*(.155f+.014f*depth*std::sin(phase[1])));
        // The filtered layer swells behind the direct diffuse tail.
        const float smoothing=1.f-std::exp(-1.f/(sr*.18f));
        smooth[0]+=(tapL-smooth[0])*smoothing;
        smooth[1]+=(tapR-smooth[1])*smoothing;
        float hazeL=tapL,hazeR=tapR;
        for(auto& stage:diffusionL)hazeL=stage.process(hazeL);
        for(auto& stage:diffusionR)hazeR=stage.process(hazeR);
        // A quieter early reflection gives way to a wide, audible wash.
        // This is an original diffuse algorithm; no pitch or sampled choir.
        float wetL=3.0f*(.14f*a+.35f*tapL+.72f*smooth[0]+.46f*hazeL-.10f*hazeR);
        float wetR=3.0f*(.14f*b+.35f*tapR+.72f*smooth[1]+.46f*hazeR-.10f*hazeL);
        const float mid=.5f*(wetL+wetR),side=.5f*(wetL-wetR)*clamp(params.width,0.f,1.5f);
        mixSmooth+=(clamp(params.mix,0.f,1.f)-mixSmooth)*.001f;
        const float d=std::cos(.5f*pi*mixSmooth),w=std::sin(.5f*pi*mixSmooth);
        l=d*dryL+w*(mid+side);r=d*dryR+w*(mid-side);
    }
private:
    Plate plate;Params params;float sr=48000.f,mixSmooth=.3f;
    std::array<Delay,2> chorus;std::array<Allpass,2> diffusionL,diffusionR;
    std::array<float,2> smooth{},phase{};
};
// A high-density modulated hall. The design is original, but follows the same
// broad acoustic goals as classic high-end digital halls: fast input diffusion,
// an orthogonal feedback network, decorrelated stereo taps, slow time variation,
// and a delayed diffuse layer that arrives behind the main tail instead of as an
// obvious repeat.
class LushHall {
public:
    void prepare(double sampleRate) {
        sr=static_cast<float>(std::max(22050.0,sampleRate));
        for(auto& d:pre)d.prepare(static_cast<size_t>(sr*.26f));
        for(auto& d:early)d.prepare(static_cast<size_t>(sr*.12f));
        for(auto& d:lines)d.prepare(static_cast<size_t>(sr*.72f));
        for(auto& d:bloomDelay)d.prepare(static_cast<size_t>(sr*.55f));
        for(size_t i=0;i<inputDiffL.size();++i){
            inputDiffL[i].prepare(static_cast<size_t>(sr*inputDiffLms[i]*.001f));
            inputDiffR[i].prepare(static_cast<size_t>(sr*inputDiffRms[i]*.001f));
        }
        for(size_t i=0;i<bloomDiffL.size();++i){
            bloomDiffL[i].prepare(static_cast<size_t>(sr*bloomDiffLms[i]*.001f));
            bloomDiffR[i].prepare(static_cast<size_t>(sr*bloomDiffRms[i]*.001f));
        }
        reset();
    }
    void reset(){
        for(auto& d:pre)d.clear();
        for(auto& d:early)d.clear();
        for(auto& d:lines)d.clear();
        for(auto& d:bloomDelay)d.clear();
        for(auto& a:inputDiffL)a.clear();
        for(auto& a:inputDiffR)a.clear();
        for(auto& a:bloomDiffL)a.clear();
        for(auto& a:bloomDiffR)a.clear();
        lp.fill(0.f);lowState.fill(0.f);outHp={0.f,0.f};
        // Spread the oscillators at startup so the first reflections do not
        // sweep together. Keep the two modulation frequencies independent:
        // multiplying a wrapped phase by a fractional ratio creates a jump.
        for(size_t i=0;i<phase.size();++i){
            phase[i]=2.f*pi*float((i*7)%16)/16.f;
            secondaryPhase[i]=2.f*pi*float((i*11+3)%16)/16.f;
        }
        bloomPhase={.37f,2.13f};mixSmooth=.3f;
    }
    void setParams(Params p){params=p;}
    void process(float& l,float& r){
        const float dryL=l,dryR=r;
        mixSmooth+=(clamp(params.mix,0.f,1.f)-mixSmooth)*.001f;
        const float size=clamp(params.size,.5f,1.7f);
        const float rt=clamp(params.decay*.98f,.6f,30.f);
        const float mod=clamp(params.modulation,0.f,1.f);
        const float modRate=clamp(params.modulationRate,.25f,3.f);
        const float preSamples=clamp(params.preDelayMs,0.f,200.f)*.001f*sr+1.f;
        const float outHpCoeff=1.f-std::exp(-2.f*pi*clamp(params.lowCutHz,20.f,1000.f)/sr);
        const float feedbackHpCoeff=1.f-std::exp(-2.f*pi*clamp(params.lowCutHz*.36f,15.f,240.f)/sr);

        pre[0].write(dryL);pre[1].write(dryR);
        float inL=pre[0].read(preSamples),inR=pre[1].read(preSamples);

        // Quiet, asymmetrical early reflections establish size without sounding
        // like discrete taps. They feed the late field as well as the output.
        early[0].write(inL);early[1].write(inR);
        float erL=0.f,erR=0.f;
        for(size_t i=0;i<earlyGain.size();++i){
            erL+=early[0].read(earlyLms[i]*.001f*sr*size)*earlyGain[i];
            erR+=early[1].read(earlyRms[i]*.001f*sr*size)*earlyGain[i];
        }

        float diffL=inL+.30f*erR,diffR=inR+.30f*erL;
        for(auto& a:inputDiffL)diffL=a.process(diffL);
        for(auto& a:inputDiffR)diffR=a.process(diffR);
        const float mid=.5f*(diffL+diffR),side=.5f*(diffL-diffR);

        std::array<float,16> v{};
        for(size_t i=0;i<v.size();++i){
            phase[i]+=2.f*pi*rateHz[i]*modRate/sr;
            if(phase[i]>=2.f*pi)phase[i]-=2.f*pi;
            secondaryPhase[i]+=2.f*pi*rateHz[i]*.413f*modRate/sr;
            if(secondaryPhase[i]>=2.f*pi)secondaryPhase[i]-=2.f*pi;
            const float base=lateMs[i]*.001f*sr*size;
            // A tiny amount of motion remains even at zero to prevent the static
            // comb signature that makes lesser halls sound metallic.
            const float depth=(.00010f+.00105f*mod)*sr;
            const float offset=depth*(std::sin(phase[i])+.23f*std::sin(secondaryPhase[i]));
            const float x=lines[i].read(base+offset);
            const float dampHz=clamp(params.dampingHz*dampingScale[i],650.f,18000.f);
            const float damp=1.f-std::exp(-2.f*pi*dampHz/sr);
            lp[i]+=damp*(x-lp[i]);
            lowState[i]+=feedbackHpCoeff*(lp[i]-lowState[i]);
            // Keep the very bottom from circulating forever; retain most of it
            // so the hall stays full rather than thin.
            v[i]=lp[i]-.28f*lowState[i];
        }

        // In-place 16-point Hadamard transform. Dividing by four makes it
        // orthonormal, so energy is redistributed rather than concentrated into
        // a few obvious feedback paths.
        std::array<float,16> mixed=v;
        for(size_t step=1;step<16;step*=2)
            for(size_t base=0;base<16;base+=step*2)
                for(size_t j=0;j<step;++j){
                    const float a=mixed[base+j],b=mixed[base+j+step];
                    mixed[base+j]=a+b;mixed[base+j+step]=a-b;
                }
        for(auto& x:mixed)x*=.25f;

        for(size_t i=0;i<16;++i){
            const float delaySeconds=lateMs[i]*.001f*size;
            const float feedback=std::pow(10.f,-3.f*delaySeconds/rt);
            const float stereo=(i&1)?side:-side;
            const float inject=(mid+stereo*injectSide[i])*.082f*injectSign[i];
            lines[i].write(mixed[i]*feedback+inject);
        }

        float coreL=0.f,coreR=0.f;
        for(size_t i=0;i<16;++i){
            coreL+=v[i]*outL[i];coreR+=v[i]*outR[i];
        }
        coreL*=.235f;coreR*=.235f;

        // This second layer is delayed and re-diffused, not sample-smoothed.
        // It therefore grows behind the main tail while preserving full-band
        // detail instead of turning into the weak low-frequency smear of the
        // earlier Bloom experiment.
        bloomDelay[0].write(coreL);bloomDelay[1].write(coreR);
        bloomPhase[0]+=2.f*pi*.071f*modRate/sr;
        bloomPhase[1]+=2.f*pi*.089f*modRate/sr;
        if(bloomPhase[0]>=2.f*pi)bloomPhase[0]-=2.f*pi;
        if(bloomPhase[1]>=2.f*pi)bloomPhase[1]-=2.f*pi;
        const float bloomDepth=(.0015f+.0045f*mod)*sr;
        float hazeL=bloomDelay[0].read(sr*(.118f+.025f*(size-1.f))+bloomDepth*std::sin(bloomPhase[0]));
        float hazeR=bloomDelay[1].read(sr*(.157f+.031f*(size-1.f))+bloomDepth*std::sin(bloomPhase[1]));
        hazeL+=.16f*bloomDelay[1].read(sr*.211f);
        hazeR+=.16f*bloomDelay[0].read(sr*.193f);
        for(auto& a:bloomDiffL)hazeL=a.process(hazeL);
        for(auto& a:bloomDiffR)hazeR=a.process(hazeR);
        const float bloomAmount=.30f+.28f*clamp((rt-2.f)/10.f,0.f,1.f)+.12f*mod;

        float wetL=.84f*coreL+bloomAmount*(.72f*hazeL-.14f*hazeR)+.12f*erL;
        float wetR=.84f*coreR+bloomAmount*(.72f*hazeR-.14f*hazeL)+.12f*erR;
        outHp[0]+=outHpCoeff*(wetL-outHp[0]);outHp[1]+=outHpCoeff*(wetR-outHp[1]);
        wetL-=outHp[0];wetR-=outHp[1];

        const float wetMid=.5f*(wetL+wetR),wetSide=.5f*(wetL-wetR)*clamp(params.width,0.f,1.5f);
        const float dryGain=std::cos(.5f*pi*mixSmooth),wetGain=std::sin(.5f*pi*mixSmooth);
        l=dryGain*dryL+wetGain*(wetMid+wetSide);
        r=dryGain*dryR+wetGain*(wetMid-wetSide);
    }
private:
    Params params;float sr=48000.f,mixSmooth=.3f;
    std::array<Delay,2> pre,early,bloomDelay;
    std::array<Delay,16> lines;
    std::array<Allpass,6> inputDiffL,inputDiffR;
    std::array<Allpass,3> bloomDiffL,bloomDiffR;
    std::array<float,16> lp{},lowState{},phase{},secondaryPhase{};
    std::array<float,2> outHp{},bloomPhase{};
    static constexpr std::array<float,6> inputDiffLms{3.1f,5.3f,8.1f,12.7f,17.9f,24.1f};
    static constexpr std::array<float,6> inputDiffRms{4.1f,6.7f,9.7f,14.3f,20.3f,27.7f};
    static constexpr std::array<float,3> bloomDiffLms{17.3f,31.7f,47.9f};
    static constexpr std::array<float,3> bloomDiffRms{19.9f,36.1f,53.3f};
    static constexpr std::array<float,6> earlyLms{5.3f,9.7f,16.9f,27.7f,41.3f,58.7f};
    static constexpr std::array<float,6> earlyRms{6.7f,12.1f,20.3f,32.9f,46.9f,64.1f};
    static constexpr std::array<float,6> earlyGain{.30f,.23f,.18f,.13f,.09f,.06f};
    static constexpr std::array<float,16> lateMs{47.9f,54.7f,62.9f,72.1f,82.7f,94.9f,108.7f,124.1f,141.7f,161.3f,183.1f,207.7f,235.1f,265.7f,299.9f,337.1f};
    static constexpr std::array<float,16> rateHz{.071f,.083f,.097f,.109f,.127f,.149f,.167f,.181f,.211f,.233f,.257f,.283f,.307f,.337f,.367f,.401f};
    static constexpr std::array<float,16> dampingScale{.88f,1.04f,.93f,1.10f,.97f,.84f,1.07f,.91f,1.02f,.86f,1.12f,.95f,.89f,1.05f,.92f,1.09f};
    static constexpr std::array<float,16> injectSide{.82f,-.77f,.63f,-.58f,.91f,-.69f,.51f,-.86f,.72f,-.61f,.94f,-.54f,.66f,-.88f,.57f,-.74f};
    static constexpr std::array<float,16> injectSign{1.f,-1.f,1.f,1.f,-1.f,1.f,-1.f,-1.f,1.f,-1.f,-1.f,1.f,1.f,1.f,-1.f,1.f};
    static constexpr std::array<float,16> outL{1.f,-1.f,1.f,1.f,-1.f,1.f,-1.f,-1.f,1.f,1.f,-1.f,1.f,1.f,-1.f,-1.f,1.f};
    static constexpr std::array<float,16> outR{1.f,1.f,-1.f,1.f,1.f,-1.f,-1.f,-1.f,-1.f,1.f,1.f,-1.f,1.f,-1.f,1.f,1.f};
};

// A dedicated modulated space: Lush Hall supplies dense, diffuse reflections;
// two independent short moving taps add slow stereo motion to the wet field.
// The dry path never enters the moving taps, preserving pick attack and pitch.
class ModulatedHall {
public:
    void prepare(double sampleRate){
        sr=static_cast<float>(std::max(22050.0,sampleRate));
        base.prepare(sampleRate);
        for(auto& d:motion)d.prepare(static_cast<size_t>(sr*.04f));
        reset();
    }
    void reset(){
        base.reset();for(auto& d:motion)d.clear();
        phase={.47f,2.71f};mixSmooth=.3f;
    }
    void setParams(Params p){
        params=p;
        auto core=p;core.mix=1.f;core.width=1.f;
        // Keep the hall's motion restrained; the moving stereo layer supplies
        // the more apparent modulation on this voice.
        core.modulation=.12f+.38f*clamp(p.modulation,0.f,1.f);
        base.setParams(core);
    }
    void process(float& l,float& r){
        const float dryL=l,dryR=r;
        float wetL=l,wetR=r;
        base.process(wetL,wetR);
        const float depth=clamp(params.modulation,0.f,1.f);
        const float speed=clamp(params.modulationRate,.25f,3.f);
        float wet[2]={wetL,wetR};
        for(int ch=0;ch<2;++ch){
            const float hz=(ch==0?.137f:.173f)*speed;
            phase[ch]+=2.f*pi*hz/sr;
            if(phase[ch]>=2.f*pi)phase[ch]-=2.f*pi;
            motion[ch].write(wet[ch]);
            const float samples=sr*(.013f+(.00035f+.0025f*depth)*std::sin(phase[ch]));
            const float moved=motion[ch].read(samples);
            // Blend with the direct wet field so the motion remains diffuse.
            wet[ch]=(.85f-.22f*depth)*wet[ch]+(.15f+.22f*depth)*moved;
        }
        const float mid=.5f*(wet[0]+wet[1]);
        const float side=.5f*(wet[0]-wet[1])*clamp(params.width,0.f,1.5f);
        mixSmooth+=(clamp(params.mix,0.f,1.f)-mixSmooth)*.001f;
        const float dryGain=std::cos(.5f*pi*mixSmooth);
        const float wetGain=std::sin(.5f*pi*mixSmooth);
        l=dryGain*dryL+wetGain*(mid+side);
        r=dryGain*dryR+wetGain*(mid-side);
    }
private:
    LushHall base;Params params;
    std::array<Delay,2> motion;
    std::array<float,2> phase{};
    float sr=48000.f,mixSmooth=.3f;
};

class Space {
public:
    void prepare(double sampleRate){
        hall.prepare(sampleRate);plate.prepare(sampleRate);room.prepare(sampleRate);bloom.prepare(sampleRate);lush.prepare(sampleRate);modHall.prepare(sampleRate);
        sr=static_cast<float>(sampleRate);blend={1.f,0.f,0.f,0.f,0.f,0.f};mixSmooth=.3f;
        for(auto& delay:modLines)delay.prepare(static_cast<size_t>(sr*.025f));
        phase={0.f,1.8f};enabledMix=1.f;
    }
    void setParams(Params p){params=p;hall.setParams(p);plate.setParams(p);room.setParams(p);bloom.setParams(p);lush.setParams(p);modHall.setParams(p);}
    void setMode(int newMode){mode=std::clamp(newMode,0,5);}
    void setEnabled(bool on){enabled=on;}
    void process(float& l,float& r){
        const float dryL=l,dryR=r;
        float hL=l,hR=r,pL=l,pR=r,rL=l,rR=r,bL=l,bR=r,xL=l,xR=r,mL=l,mR=r;
        // Only run a voice while it is selected or fading out. Six parallel
        // reverbs can starve the audio callback alongside two NAM captures.
        if(mode==0||blend[0]>.0001f)hall.process(hL,hR);
        if(mode==1||blend[1]>.0001f)plate.process(pL,pR);
        if(mode==2||blend[2]>.0001f)room.process(rL,rR);
        if(mode==3||blend[3]>.0001f)bloom.process(bL,bR);
        // Lush Hall is deliberately heavier than the legacy voices. Keep it idle
        // unless selected or still crossfading out so NAM users do not pay for it.
        if(mode==4||blend[4]>.0001f)lush.process(xL,xR);
        if(mode==5||blend[5]>.0001f)modHall.process(mL,mR);
        const float step=1.f-std::exp(-1.f/(.06f*sr));
        for(int i=0;i<6;++i)blend[i]+=((mode==i?1.f:0.f)-blend[i])*step;
        float wetL=hL*blend[0]+pL*blend[1]+rL*blend[2]+bL*blend[3]+xL*blend[4]+mL*blend[5];
        float wetR=hR*blend[0]+pR*blend[1]+rR*blend[2]+bR*blend[3]+xR*blend[4]+mR*blend[5];
        // Every algorithm uses the same smoothed, equal-power mix. Modulate only
        // its reverb contribution so the dry guitar and dry delay remain clear.
        mixSmooth+=(clamp(params.mix,0.f,1.f)-mixSmooth)*.001f;
        const float dryGain=std::cos(.5f*pi*mixSmooth);
        const float modDepth=clamp(params.modulation,0.f,1.f);
        const float rate=clamp(params.modulationRate,.25f,3.f);
        // Keep the existing 30% default and older presets sonically unchanged.
        // Above that, the control adds progressively audible stereo movement.
        const float amount=clamp((modDepth-.30f)/.70f,0.f,1.f)*.8f*(1.f-blend[5]);
        float wet[2]={wetL-dryGain*dryL,wetR-dryGain*dryR};
        for(int ch=0;ch<2;++ch){
            phase[ch]+=2.f*pi*rate/sr;
            if(phase[ch]>=2.f*pi)phase[ch]-=2.f*pi;
            modLines[ch].write(wet[ch]);
            const float tapMs=6.f+3.5f*modDepth*std::sin(phase[ch]);
            const float moved=modLines[ch].read(sr*tapMs*.001f);
            wet[ch]+=(moved-wet[ch])*amount;
        }
        const float processedL=dryGain*dryL+wet[0],processedR=dryGain*dryR+wet[1];
        enabledMix+=((enabled?1.f:0.f)-enabledMix)*(1.f-std::exp(-1.f/(.025f*sr)));
        l=dryL+(processedL-dryL)*enabledMix;
        r=dryR+(processedR-dryR)*enabledMix;
    }
private:
    Hall hall;Plate plate;Room room;Cloud bloom;LushHall lush;ModulatedHall modHall;Params params;
    std::array<Delay,2> modLines;std::array<float,2> phase{};
    float sr=48000.f,mixSmooth=.3f,enabledMix=1.f;bool enabled=true;
    int mode=0;std::array<float,6> blend{1.f,0.f,0.f,0.f,0.f,0.f};
};
}
