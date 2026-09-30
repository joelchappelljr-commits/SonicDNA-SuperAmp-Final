#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace sdna {
constexpr int N = 16;
constexpr float pi = 3.14159265358979323846f;
inline float clamp(float x, float a, float b) { return std::max(a, std::min(x, b)); }
struct Params {
    float mix = .30f, decay = 5.0f, size = 1.0f, preDelayMs = 24.0f;
    float dampingHz = 6500.0f, lowCutHz = 150.0f, modulation = .30f, modulationRate = 1.0f, width = 1.0f;
};
class Delay {
public:
    void prepare(size_t n) { data.assign(n + 4, 0.0f); pos = 0; }
    void clear() { std::fill(data.begin(), data.end(), 0.0f); pos = 0; }
    void write(float x) { data[pos] = x; pos = (pos + 1) % data.size(); }
    float read(float samples) const {
        // Cubic Hermite interpolation keeps the modulated read smooth.
        const float rp = static_cast<float>(pos) - samples;
        const int i = static_cast<int>(std::floor(rp));
        const float t = rp - static_cast<float>(i);
        const auto at = [&](int offset) { int k = (i + offset) % static_cast<int>(data.size()); if (k < 0) k += static_cast<int>(data.size()); return data[static_cast<size_t>(k)]; };
        const float a = at(-1), b = at(0), c = at(1), d = at(2);
        return b + .5f * t * (c - a + t * (2*a - 5*b + 4*c - d + t * (3*(b-c) + d-a)));
    }
private:
    std::vector<float> data;
    size_t pos = 0;
};
class Allpass {
public:
    void prepare(size_t samples) { buffer.assign(std::max<size_t>(samples, 1), 0.f); pos = 0; }
    void clear() { std::fill(buffer.begin(), buffer.end(), 0.f); pos = 0; }
    float process(float x) { const float v = buffer[pos]; const float y = v - .67f*x; buffer[pos] = x + .67f*y; pos = (pos+1)%buffer.size(); return y; }
private:
    std::vector<float> buffer;
    size_t pos = 0;
};
class Hall {
public:
    void prepare(double sampleRate) {
        sr = static_cast<float>(std::max(22050.0, sampleRate));
        for (auto& line : lines) line.prepare(static_cast<size_t>(sr * .45f));
        for (int j=0;j<4;++j) { inputAP[j].prepare(static_cast<size_t>(sr * diffusionMs[j] * .001f)); }
        for (auto& p : predelay) p.prepare(static_cast<size_t>(sr * .26f));
        reset();
    }
    void reset() { for(auto& d:lines)d.clear(); for(auto& d:predelay)d.clear(); for(auto& a:inputAP)a.clear(); lp.fill(0); hpState.fill(0); phase.fill(0); mixSmoothed=.30f; sampleIndex=0; }
    void setParams(Params p) { target = p; }
    void process(float& l, float& r) {
        const float dryL=l, dryR=r;
        mixSmoothed += .001f * (clamp(target.mix,0,1)-mixSmoothed);
        const float sz=clamp(target.size,.5f,1.7f), rt=clamp(target.decay,.5f,30.f);
        const float pre=clamp(target.preDelayMs,0,200)*.001f*sr+1;
        const float aLP=1.f-std::exp(-2.f*pi*clamp(target.dampingHz,600,18000)/sr);
        const float aHP=1.f-std::exp(-2.f*pi*clamp(target.lowCutHz,20,1000)/sr);
        predelay[0].write(dryL); predelay[1].write(dryR);
        float inL=predelay[0].read(pre), inR=predelay[1].read(pre);
        float mid=.5f*(inL+inR), side=.5f*(inL-inR);
        float inj[4] = {mid+side, mid-side, mid+.35f*side, mid-.35f*side};
        for(int j=0;j<4;++j) inj[j]=inputAP[j].process(inj[j]);
        std::array<float,N> v{};
        float sum=0.f;
        for(int i=0;i<N;++i) {
            const float base=delayMs[i]*.001f*sr*sz;
            const float depth=clamp(target.modulation,0,1)*.00019f*sr;
            phase[i] += (2.f*pi*rateHz[i]*clamp(target.modulationRate,.25f,3.f)/sr);
            if(phase[i]>=2.f*pi) phase[i]-=2.f*pi;
            const float offset=depth*(std::sin(phase[i])+.22f*std::sin(phase[i]*.371f+float(i)));
            const float x=lines[i].read(base+offset);
            lp[i] += aLP*(x-lp[i]);
            v[i]=lp[i];
            sum+=v[i];
        }
        // Householder reflection: energy-preserving, dense global mixing.
        const float mean=sum/8.f;
        float wetL=0,wetR=0;
        for(int i=0;i<N;++i) {
            const float feedback=std::pow(10.f, -3.f*delayMs[i]*.001f*sz/rt);
            const float drive=inj[i%4]*.115f*((i&4)?-1.f:1.f);
            lines[i].write((v[i]-mean)*feedback+drive);
            const float s=((i*7)%16 < 8)?1.f:-1.f;
            wetL+=v[i]*outputL[i]; wetR+=v[i]*outputL[i]*s;
        }
        wetL*=.31f; wetR*=.31f;
        hpState[0] += aHP*(wetL-hpState[0]); hpState[1] += aHP*(wetR-hpState[1]);
        wetL-=hpState[0]; wetR-=hpState[1];
        const float w=clamp(target.width,0,1.5f), m=.5f*(wetL+wetR), s=.5f*(wetL-wetR)*w;
        wetL=m+s; wetR=m-s;
        // Constant-power dry/wet mixing.
        const float dryGain=std::cos(.5f*pi*mixSmoothed), wetGain=std::sin(.5f*pi*mixSmoothed);
        l=dryGain*dryL+wetGain*wetL; r=dryGain*dryR+wetGain*wetR;
        ++sampleIndex;
    }
private:
    float sr=48000.f, mixSmoothed=.30f;
    unsigned long long sampleIndex=0;
    Params target;
    std::array<Delay,N> lines;
    std::array<Delay,2> predelay;
    std::array<Allpass,4> inputAP;
    std::array<float,N> lp{}, hpState{}, phase{};
    static constexpr std::array<float,4> diffusionMs{4.9f,7.7f,11.3f,16.1f};
    static constexpr std::array<float,N> delayMs{39.1f,43.7f,48.1f,53.9f,59.3f,65.9f,72.7f,80.9f,89.3f,98.9f,109.1f,120.7f,133.1f,147.1f,162.7f,179.9f};
    static constexpr std::array<float,N> rateHz{.13f,.17f,.19f,.23f,.29f,.31f,.37f,.41f,.43f,.47f,.53f,.59f,.61f,.67f,.71f,.73f};
    static constexpr std::array<float,N> outputL{1,-1,1,1,-1,1,-1,-1,1,1,-1,1,1,-1,-1,1};
};
}
