#include "MemsicAlgo.h"
#include "MemsicCompass.h"

/* Standalone emulator probe. This file is never part of Product. */
float probe_matrix[9] = {1,0,0,0,1,0,0,0,1};
float probe_initial_parameters[4] = {0,0,0,0.5f};
float probe_acceleration[3], probe_magnetic[3];
float probe_parameters[4], probe_calibrated[3], probe_orientation[3];
int probe_enabled, probe_initial, probe_compass_initial, probe_processed;
int probe_accuracy, probe_saturation, probe_updated, probe_oriented;

void probe_init(void)
{
    probe_initial = InitialAlgorithm(probe_matrix, probe_initial_parameters);
    probe_compass_initial = InitialCompassAlgo(1.0f);
}

void probe_step(void)
{
    probe_processed = MainAlgorithmProcess(probe_acceleration, probe_magnetic, probe_enabled);
    probe_accuracy = GetMagAccuracy();
    probe_saturation = GetMagSaturation();
    for (int i = 0; i < 4; ++i) probe_parameters[i] = -12345.0f;
    probe_updated = GetCalPara(probe_parameters);
    GetCalMag(probe_calibrated);
    probe_oriented = GetCalOri(probe_acceleration, probe_calibrated, probe_orientation);
}

extern double __hardfp_sqrt(double);
extern double __hardfp_acos(double);
extern double __hardfp_asin(double);
extern double __hardfp_sin(double);
extern double __hardfp_cos(double);
extern double __hardfp_atan2(double, double);
double probe_math_output[6];
void probe_math(void)
{
    probe_math_output[0] = __hardfp_sqrt(2.25);
    probe_math_output[1] = __hardfp_acos(0.3);
    probe_math_output[2] = __hardfp_asin(-0.4);
    probe_math_output[3] = __hardfp_sin(1.2);
    probe_math_output[4] = __hardfp_cos(-0.7);
    probe_math_output[5] = __hardfp_atan2(-0.3, 0.4);
}
