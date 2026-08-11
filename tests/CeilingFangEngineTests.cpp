#include "ceilingfang/CeilingFangEngine.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using ceilingfang::CeilingFangEngine; using ceilingfang::CeilingFangParameters;
namespace {
float renderPeak(CeilingFangParameters p,float in){ CeilingFangEngine e; e.prepare(48000); e.setParameters(p); e.reset(); float peak=0; for(int i=0;i<8192;++i){ auto f=e.processSample(in*(i&1?1:-1),in); if(i>1024) peak=std::max(peak,std::max(std::fabs(f.left),std::fabs(f.right))); } return peak; }
void testSilence(){ CeilingFangEngine e; e.prepare(48000); for(int i=0;i<4096;++i){ auto f=e.processSample(0,0); assert(std::fabs(f.left)<=1e-7f); assert(std::fabs(f.right)<=1e-7f);} }
void testCeiling(){ CeilingFangParameters p; p.ceiling=-6; p.lookahead=5; p.release=20; p.clip=.2f; auto peak=renderPeak(p,1.6f); assert(peak <= .53f); }
void testClip(){ CeilingFangParameters dry; dry.ceiling=0; dry.clip=0; CeilingFangParameters clipped=dry; clipped.ceiling=-3; clipped.clip=1; assert(renderPeak(clipped,2.0f) < renderPeak(dry,2.0f)); }
void testDetectModes(){ CeilingFangParameters a; a.detect=0; CeilingFangParameters b=a; b.detect=1; assert(std::fabs(renderPeak(a,1.2f)-renderPeak(b,1.2f)) < .2f); }
void testFinite(){ CeilingFangEngine e; CeilingFangParameters p; p.ceiling=1000; p.lookahead=1000; p.release=0; p.detect=1000; p.zeroBias=1000; p.adapt=1000; p.clip=1000; e.prepare(0); e.setParameters(p); for(int i=0;i<4096;++i){ auto f=e.processSample(1000,-1000); assert(std::isfinite(f.left)); assert(f.left>=-1.001f&&f.left<=1.001f); assert(std::isfinite(f.right)); } }
}
int main(){ testSilence(); testCeiling(); testClip(); testDetectModes(); testFinite(); std::cout<<"CeilingFangEngineTests passed\n"; }
