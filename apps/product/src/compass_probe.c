#include "compass_probe.h"
#if defined(_WIN32) && defined(_stdcall)
#undef _stdcall /* The vendor EVB header defines this empty for this C boundary. */
#endif
#include "MemsicAlgo.h"
#include "MemsicCompass.h"
#include <math.h>
#include <string.h>

static float identity[9] = {1,0,0,0,1,0,0,0,1};
static bool finite_values(const float *v, unsigned count)
{ for (unsigned i=0;i<count;++i) if (!isfinite(v[i])) return false; return true; }
static float dot(const float a[3], const float b[3])
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
bool wf_compass_probe_axes_valid(const int8_t axes[3])
{
    if (!axes) return false;
    int indices[3], sign=1;
    for (unsigned i=0;i<3;++i) {
        int a=axes[i];
        if (!a || a < -3 || a > 3) return false;
        sign *= a < 0 ? -1 : 1; indices[i]=(a < 0 ? -a : a)-1;
        for (unsigned j=0;j<i;++j) if (indices[j]==indices[i]) return false;
    }
    for (unsigned i=0;i<3;++i) for (unsigned j=i+1;j<3;++j)
        if (indices[i]>indices[j]) sign=-sign;
    return sign==1; /* Proper signed-permutation rotation only. */
}
static float mapped(const float v[3], int8_t axis)
{ return axis < 0 ? -v[-axis-1] : v[axis-1]; }
bool wf_compass_probe_project(const float a[3], const float m[3], float *degrees)
{
    if (!degrees || !finite_values(a,3) || !finite_values(m,3)) return false;
    float an=sqrtf(dot(a,a)), mn=sqrtf(dot(m,m));
    if (!(an>0) || !(mn>0) || !isfinite(an) || !isfinite(mn)) return false;
    float u[3], n[3];
    for (unsigned i=0;i<3;++i) { u[i]=a[i]/an; n[i]=m[i]/mn; }
    float vertical=dot(n,u);
    for (unsigned i=0;i<3;++i) n[i]-=vertical*u[i];
    float hn=sqrtf(dot(n,n));
    if (!(hn>1e-6f) || !(1-u[1]*u[1]>1e-12f)) return false;
    float east_y=(n[2]*u[0]-n[0]*u[2])/hn;
    float angle=atan2f(east_y,n[1]/hn)*57.29577951308232f;
    if (!isfinite(angle)) return false;
    if (angle<0) angle+=360;
    if (angle>=360) angle=0;
    *degrees=angle; return true;
}
void wf_compass_probe_stop(wf_compass_probe_t *p, wf_compass_probe_end_t reason)
{
    if (!p->snapshot.active) return;
    p->snapshot.active=false; p->snapshot.output_valid=p->snapshot.projected_valid=false;
    p->snapshot.ended=reason;
}
void wf_compass_probe_tick(wf_compass_probe_t *p, uint32_t now)
{
    if (p->snapshot.active && now-p->snapshot.started_ms>=p->snapshot.duration_ms)
        wf_compass_probe_stop(p,WF_PROBE_DEADLINE);
}
bool wf_compass_probe_begin(wf_compass_probe_t *p, uint32_t generation, uint32_t now,
    unsigned seconds, bool algorithm, const int8_t acc_axes[3], const int8_t mag_axes[3])
{
    if (p->snapshot.active || seconds<1 || seconds>120 ||
        !wf_compass_probe_axes_valid(acc_axes) || !wf_compass_probe_axes_valid(mag_axes)) return false;
    uint32_t session=p->snapshot.session+1;
    p->snapshot=(wf_compass_probe_snapshot_t){.active=true,.algorithm=algorithm,
        .session=session,.generation=generation,.started_ms=now,.duration_ms=seconds*1000U};
    memcpy(p->snapshot.acc_axes,acc_axes,3); memcpy(p->snapshot.mag_axes,mag_axes,3);
    if (algorithm) {
        float initial[4]={0,0,0,.5f};
        if (InitialAlgorithm(identity,initial)!=1 || InitialCompassAlgo(1.0f)!=1) {
            wf_compass_probe_stop(p,WF_PROBE_LIBRARY); return false;
        }
    }
    return true;
}
bool wf_compass_probe_freeze(wf_compass_probe_t *p)
{
    wf_compass_probe_snapshot_t *s=&p->snapshot;
    if (!s->active || !s->algorithm || s->frozen || !s->parameters_valid) return false;
    if (InitialAlgorithm(identity,s->parameters)!=1 || InitialCompassAlgo(1.0f)!=1) {
        wf_compass_probe_stop(p,WF_PROBE_LIBRARY); return false;
    }
    s->frozen=true; s->output_valid=s->projected_valid=false;
    return true;
}
bool wf_compass_probe_feed(wf_compass_probe_t *p, const wf_compass_probe_frame_t *frame, uint32_t now)
{
    wf_compass_probe_tick(p,now);
    wf_compass_probe_snapshot_t *s=&p->snapshot;
    if (!s->active) return false;
    s->frame=*frame; ++s->frames; s->output_valid=s->projected_valid=false;
    if ((frame->transfers&1U) && frame->bank!=0) { wf_compass_probe_stop(p,WF_PROBE_CONFIG); return false; }
    if (frame->transfers!=31U) { wf_compass_probe_stop(p,WF_PROBE_IO); return false; }
    if (frame->xl!=0x20 || frame->ctrl3!=0x44) { wf_compass_probe_stop(p,WF_PROBE_CONFIG); return false; }
    int32_t pair_gap=(int32_t)(frame->acc_end_ms-frame->mag_ms);
    if (!(frame->status&1U) ||
        frame->acc_end_ms-frame->acc_begin_ms>50U || pair_gap < -50 || pair_gap > 50 ||
        now-frame->acc_end_ms>50U) { ++s->rejected; return false; }
    float acc[3],mag[3];
    for (unsigned i=0;i<3;++i) {
        acc[i]=frame->acceleration[i]*.000061f; /* +/-2g, 0.061mg/LSB. */
        mag[i]=frame->magnetic[i]/16384.0f; /* Rev.B sensitivity, 20bit counts/G. */
    }
    for (unsigned i=0;i<3;++i) {
        s->acceleration_g[i]=mapped(acc,s->acc_axes[i]);
        s->magnetic_gauss[i]=mapped(mag,s->mag_axes[i]);
    }
    if (!s->algorithm) return true;
    float an=sqrtf(dot(s->acceleration_g,s->acceleration_g));
    if (!(an>=.7f && an<=1.3f)) { ++s->rejected; return false; }
    if (MainAlgorithmProcess(s->acceleration_g,s->magnetic_gauss,s->frozen ? 0 : 1)!=1) {
        wf_compass_probe_stop(p,WF_PROBE_LIBRARY); return false;
    }
    s->accuracy=GetMagAccuracy(); s->saturation=GetMagSaturation();
    if (s->saturation) { wf_compass_probe_stop(p,WF_PROBE_SATURATED); return false; }
    float parameters[4]={0};
    int updated=GetCalPara(parameters);
    GetCalMag(s->corrected_gauss);
    if ((updated!=0 && updated!=1) || s->accuracy<0 || s->accuracy>3 || !finite_values(s->corrected_gauss,3) ||
        GetCalOri(s->acceleration_g,s->corrected_gauss,s->orientation)!=1 ||
        !finite_values(s->orientation,3) || s->orientation[0]<0 || s->orientation[0]>360 ||
        fabsf(s->orientation[1])>360 || fabsf(s->orientation[2])>360 ||
        (updated==1 && (!finite_values(parameters,4) || !(parameters[3]>0)))) {
        wf_compass_probe_stop(p,WF_PROBE_LIBRARY); return false;
    }
    if (updated==1 && !s->frozen) {
        memcpy(s->parameters,parameters,sizeof(parameters)); s->parameters_valid=true; ++s->updates;
    }
    s->output_valid=true;
    s->projected_valid=wf_compass_probe_project(s->acceleration_g,s->corrected_gauss,&s->projected_degrees);
    return true;
}
