#include <math.h>

/* Experimental Arm Compiler/GCC boundary, offline probe only. */
double __hardfp_sqrt(double x) { return sqrt(x); }
double __hardfp_acos(double x) { return acos(x); }
double __hardfp_asin(double x) { return asin(x); }
double __hardfp_sin(double x) { return sin(x); }
double __hardfp_cos(double x) { return cos(x); }
double __hardfp_atan2(double y, double x) { return atan2(y, x); }
