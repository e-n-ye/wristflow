#include "memsic_fake.h"
#include <math.h>
#include <string.h>
unsigned memsic_calls, memsic_initials;
int memsic_enabled, memsic_fail, memsic_bad, memsic_saturated, memsic_update;
void (*memsic_process_hook)(void);
static float offset[4], field[3];
void memsic_reset(void)
{
    memsic_calls=memsic_initials=0;
    memsic_enabled=memsic_fail=memsic_bad=memsic_saturated=0; memsic_update=1;
    memset(offset,0,sizeof offset);
    memsic_process_hook=0;
}
int InitialAlgorithm(float *matrix, float *parameters)
{ (void)matrix; ++memsic_initials; memcpy(offset,parameters,sizeof offset); return memsic_fail ? -1 : 1; }
int InitialCompassAlgo(float factor) { (void)factor; return memsic_fail ? -1 : 1; }
int MainAlgorithmProcess(float *a, float *m, int enabled)
{
    (void)a; ++memsic_calls; memsic_enabled=enabled;
    for (unsigned i=0;i<3;++i) field[i]=m[i]-offset[i];
    if (memsic_process_hook) memsic_process_hook();
    return memsic_fail ? -1 : 1;
}
int GetMagAccuracy(void) { return 3; }
int GetMagSaturation(void) { return memsic_saturated; }
int GetCalPara(float *parameters)
{
    const float values[4]={.1f,-.08f,.05f,.5f};
    memcpy(parameters,values,sizeof values);
    if (memsic_bad==2) parameters[2]=NAN;
    return memsic_update;
}
int GetCalMag(float *out)
{ memcpy(out,field,sizeof field); if (memsic_bad==1) out[1]=NAN; return 0; }
int GetCalOri(float *a, float *m, float *out)
{
    (void)a; out[0]=atan2f(-m[0],m[1])*57.29577951308232f;
    if (out[0]<0) out[0]+=360;
    out[1]=out[2]=0;
    if (memsic_bad==3) out[2]=INFINITY;
    return memsic_fail ? -1 : 1;
}
