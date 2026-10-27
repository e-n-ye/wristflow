#ifdef NDEBUG
#undef NDEBUG
#endif
#include "compass_probe.h"
#include "support/memsic_fake.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static const int8_t identity[3]={1,2,3};
static wf_compass_probe_frame_t frame(uint32_t time)
{
    return (wf_compass_probe_frame_t){.mag_ms=time,.acc_begin_ms=time,.acc_end_ms=time+1,
        .acceleration={0,0,16384},.magnetic={0,5734,-4915},.xl=0x20,.ctrl3=0x44,
        .status=1,.transfers=31,.source=1};
}
static void begin(wf_compass_probe_t *p, uint32_t now, bool algorithm)
{ assert(wf_compass_probe_begin(p,7,now,2,algorithm,identity,identity)); }
int main(void)
{
    memsic_reset(); wf_compass_probe_t p={0};
    const int8_t reflection[3]={1,2,-3}, duplicate[3]={1,1,3}, bad[3]={0,2,3}, rotation[3]={2,-1,3};
    assert(wf_compass_probe_axes_valid(identity) && wf_compass_probe_axes_valid(rotation));
    assert(!wf_compass_probe_axes_valid(reflection) && !wf_compass_probe_axes_valid(duplicate));
    assert(!wf_compass_probe_axes_valid(bad));
    assert(!wf_compass_probe_begin(&p,7,0,0,true,identity,identity));
    assert(!wf_compass_probe_begin(&p,7,0,121,true,identity,identity));
    assert(!wf_compass_probe_begin(&p,7,0,2,true,reflection,identity));
    assert(!memsic_initials);
    begin(&p,0,false); assert(!memsic_initials);
    wf_compass_probe_frame_t f=frame(20);
    assert(wf_compass_probe_feed(&p,&f,22));
    assert(!p.snapshot.output_valid && p.snapshot.frames==1 && !memsic_calls);
    assert(fabsf(p.snapshot.acceleration_g[2]-.999424f)<1e-6f);
    assert(fabsf(p.snapshot.magnetic_gauss[1]-5734.0f/16384)<1e-7f);
    assert(!wf_compass_probe_begin(&p,8,25,1,true,identity,identity));
    wf_compass_probe_stop(&p,WF_PROBE_CANCELLED); assert(!wf_compass_probe_feed(&p,&f,23));
    begin(&p,100,true); f=frame(120); assert(wf_compass_probe_feed(&p,&f,122));
    assert(p.snapshot.output_valid && p.snapshot.projected_valid && p.snapshot.parameters_valid);
    assert(p.snapshot.accuracy==3 && p.snapshot.updates==1 && memsic_enabled==1);
    assert(wf_compass_probe_freeze(&p) && !wf_compass_probe_freeze(&p));
    f=frame(150); assert(wf_compass_probe_feed(&p,&f,152) && memsic_enabled==0);
    const float frozen_offset[3]={.1f,-.08f,.05f};
    for (unsigned i=0;i<3;++i)
        assert(fabsf(p.snapshot.corrected_gauss[i]-(p.snapshot.magnetic_gauss[i]-frozen_offset[i]))<1e-7f);
    assert(p.snapshot.updates==1);
    wf_compass_probe_tick(&p,2100); assert(!p.snapshot.active && p.snapshot.ended==WF_PROBE_DEADLINE);
    assert(p.snapshot.parameters_valid && !p.snapshot.output_valid);
    unsigned calls=memsic_calls; assert(!wf_compass_probe_feed(&p,&f,2150) && memsic_calls==calls);
    begin(&p,UINT32_MAX-100,true); f=frame(UINT32_MAX-80);
    assert(wf_compass_probe_feed(&p,&f,UINT32_MAX-78));
    wf_compass_probe_tick(&p,1900); assert(p.snapshot.ended==WF_PROBE_DEADLINE && !p.snapshot.active);
    begin(&p,0,true); f=frame(20); f.status=0;
    calls=memsic_calls; assert(!wf_compass_probe_feed(&p,&f,22) && memsic_calls==calls && p.snapshot.active);
    f=frame(20); f.acc_end_ms=71;
    assert(!wf_compass_probe_feed(&p,&f,72) && p.snapshot.active);
    f=frame(20); f.acc_end_ms=30; f.acc_begin_ms=20;
    assert(!wf_compass_probe_feed(&p,&f,81));
    f=frame(100); f.acc_begin_ms=40; f.acc_end_ms=45; f.source=2;
    assert(!wf_compass_probe_feed(&p,&f,101));
    f.acc_begin_ms=60; f.acc_end_ms=65;
    assert(wf_compass_probe_feed(&p,&f,101)); /* A fresh pre-MAG wrist read is valid. */
    f=frame(120); memset(f.acceleration,0,sizeof f.acceleration);
    calls=memsic_calls; assert(!wf_compass_probe_feed(&p,&f,122) && memsic_calls==calls);
    wf_compass_probe_stop(&p,WF_PROBE_CANCELLED);
    begin(&p,0,true); f=frame(20); f.transfers=15;
    assert(!wf_compass_probe_feed(&p,&f,22) && p.snapshot.ended==WF_PROBE_IO);
    begin(&p,0,true); f=frame(20); f.bank=0x80; f.transfers=1;
    assert(!wf_compass_probe_feed(&p,&f,22) && p.snapshot.ended==WF_PROBE_CONFIG);
    begin(&p,0,true); f=frame(20); f.xl=0x28;
    assert(!wf_compass_probe_feed(&p,&f,22) && p.snapshot.ended==WF_PROBE_CONFIG);
    for (int bad_output=1;bad_output<=3;++bad_output) {
        begin(&p,0,true); f=frame(20); memsic_bad=bad_output;
        assert(!wf_compass_probe_feed(&p,&f,22) && p.snapshot.ended==WF_PROBE_LIBRARY);
        memsic_bad=0;
    }
    begin(&p,0,true); memsic_update=-1; f=frame(20);
    assert(!wf_compass_probe_feed(&p,&f,22) && p.snapshot.ended==WF_PROBE_LIBRARY); memsic_update=1;
    begin(&p,0,true); memsic_saturated=1;
    assert(!wf_compass_probe_feed(&p,&f,22) && p.snapshot.ended==WF_PROBE_SATURATED); memsic_saturated=0;
    memsic_fail=1; assert(!wf_compass_probe_begin(&p,7,0,2,true,identity,identity)); memsic_fail=0;
    float angle=0, up[3]={0,0,1};
    const float fields[4][3]={{0,1,-.3f},{-1,0,-.3f},{0,-1,-.3f},{1,0,-.3f}};
    for (unsigned i=0;i<4;++i) {
        assert(wf_compass_probe_project(up,fields[i],&angle) && fabsf(angle-i*90.0f)<.001f);
    }
    /* Independent body-to-world Euler matrix; expected bearing comes from
     * its forward column, not from the production cross-product expression. */
    for (int yaw=0;yaw<360;yaw+=30) for (int pitch=-40;pitch<=40;pitch+=40)
        for (int roll=-30;roll<=30;roll+=30) {
            float z=yaw*.01745329252f,y=pitch*.01745329252f,x=roll*.01745329252f;
            float cz=cosf(z),sz=sinf(z),cy=cosf(y),sy=sinf(y),cx=cosf(x),sx=sinf(x);
            float r[3][3]={{cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx},
                           {sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx},
                           {-sy,cy*sx,cy*cx}};
            float expected=atan2f(r[0][1],r[1][1])*57.295779513f;
            if (expected<0) expected+=360;
            for (int dip=-1;dip<=1;++dip) {
                float a[3],m[3];
                for (unsigned j=0;j<3;++j) { a[j]=r[2][j]; m[j]=r[1][j]+dip*.6f*r[2][j]; }
                assert(wf_compass_probe_project(a,m,&angle));
                float error=remainderf(angle-expected,360);
                assert(fabsf(error)<.002f);
            }
        }
    const float vertical[3]={0,0,1}, forward_vertical[3]={0,1,0}, invalid[3]={0,NAN,1};
    assert(!wf_compass_probe_project(up,vertical,&angle));
    assert(!wf_compass_probe_project(forward_vertical,fields[1],&angle));
    assert(!wf_compass_probe_project(up,invalid,&angle));
    return 0;
}
