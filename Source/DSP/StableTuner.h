#pragma once
#include <algorithm>
#include <array>
#include <cmath>

class StableTuner
{
public:
    void prepare(double sampleRate)
    {
        decimation=std::max(1,(int)std::lround(sampleRate/11025.0));
        analysisRate=sampleRate/decimation;
        lowpassCoefficient=1.0-std::exp(-2.0*3.141592653589793*1200.0/sampleRate);
        ring.fill(0.0f); write=0; filled=0; decimationCounter=0;
        analysisCounter=0; lowpass=0.0; stableMidi=0.0; pendingMidi=0.0;
        pendingCount=0; missingCount=0; result=0.0f;
    }

    void push(float input) noexcept
    {
        lowpass+=lowpassCoefficient*((double)input-lowpass);
        if(++decimationCounter<decimation) return;
        decimationCounter=0;
        ring[write]=(float)lowpass;
        write=(write+1)&(ringSize-1);
        filled=std::min(filled+1,ringSize);
        if(filled>=window+maxLag && ++analysisCounter>=hop)
        {
            analysisCounter=0;
            analyse();
        }
    }

    float frequency() const noexcept { return result; }

private:
    static constexpr int ringSize=2048, window=1024, maxLag=192, hop=512;
    std::array<float,ringSize> ring{};
    std::array<double,maxLag+2> difference{};
    int write=0,filled=0,decimation=4,decimationCounter=0,analysisCounter=0;
    int pendingCount=0,missingCount=0;
    double analysisRate=11025.0,lowpassCoefficient=0.15,lowpass=0.0;
    double stableMidi=0.0,pendingMidi=0.0;
    float result=0.0f;

    void missing() noexcept
    {
        if(++missingCount>=8) { result=0.0f; stableMidi=0.0; pendingCount=0; }
    }

    void analyse() noexcept
    {
        double energy=0.0;
        for(int i=0;i<window;++i)
        {
            const double x=ring[(write-1-i)&(ringSize-1)];
            energy+=x*x;
        }
        if(energy/window<0.0015*0.0015) { missing(); return; }

        const int lo=std::max(2,(int)(analysisRate/1200.0));
        const int hi=std::min(maxLag,(int)std::ceil(analysisRate/65.0));
        if(hi<=lo+2) { missing(); return; }
        difference[0]=0.0;
        double running=0.0;
        int chosen=0;
        for(int lag=1;lag<=hi+1;++lag)
        {
            double sum=0.0;
            for(int i=0;i<window;++i)
            {
                const double delta=(double)ring[(write-1-i)&(ringSize-1)]
                                   -(double)ring[(write-1-i-lag)&(ringSize-1)];
                sum+=delta*delta;
            }
            running+=sum;
            difference[lag]=running>1.0e-15?sum*lag/running:1.0;
        }
        for(int lag=lo;lag<=hi;++lag)
            if(difference[lag]<0.15) { chosen=lag; break; }
        if(chosen==0) { missing(); return; }
        while(chosen<hi && difference[chosen+1]<difference[chosen]) ++chosen;
        const double left=difference[chosen-1], mid=difference[chosen], right=difference[chosen+1];
        const double denominator=left-2.0*mid+right;
        const double offset=std::abs(denominator)>1.0e-12
            ?std::clamp(0.5*(left-right)/denominator,-0.5,0.5):0.0;
        const double hz=analysisRate/(chosen+offset);
        if(!std::isfinite(hz) || hz<65.0 || hz>1200.0) { missing(); return; }
        const double midi=69.0+12.0*std::log2(hz/440.0);
        missingCount=0;
        if(stableMidi==0.0) stableMidi=midi;
        else if(std::abs(midi-stableMidi)<0.45)
        { stableMidi+=0.28*(midi-stableMidi); pendingCount=0; }
        else
        {
            if(pendingCount==0 || std::abs(midi-pendingMidi)>0.35)
            { pendingMidi=midi; pendingCount=1; }
            else if(++pendingCount>=3)
            { stableMidi=pendingMidi; pendingCount=0; }
        }
        result=(float)(440.0*std::exp2((stableMidi-69.0)/12.0));
    }
};
