#include "wrist_pose.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void expect(int16_t x, int16_t y, int16_t z, int accepted)
{
    int16_t axes[] = {x,y,z};
    assert((wf_wrist_pose_classify(axes) == WF_WRIST_POSE_ACCEPT) == accepted);
}

int main(void)
{
    /* 2026-10-09: user's tilted-view target, old predicate rejected all 20.
     * Sensor sample mode is diagnostic; these are classification fixtures. */
    const int16_t target[][3] = {
        {-10952,-528,-12206}, {-11250,-358,-12083}, {-11053,-378,-12077},
        {-11053,-293,-12020}, {-11072,-380,-11928}, {-11233,-272,-12321},
        {-11113,-339,-12288}, {-11256,-316,-12220}, {-11200,-364,-12216},
        {-11464,-300,-12271}, {-11000,-473,-12230}, {-11255,-102,-12405},
        {-11217,-162,-12274}, {-11235,-168,-12257}, {-10875,-473,-12225},
        {-10709,-520,-12058}, {-11222,-118,-12101}, {-10867,-319,-12060},
        {-10780,-455,-11885}, {-10739,-386,-11676}
    };
    for (unsigned i=0; i<sizeof target/sizeof target[0]; ++i) {
        expect(target[i][0],target[i][1],target[i][2],1);
        expect(-target[i][0],target[i][1],target[i][2],0); /* opposite lift */
        expect(target[i][1],target[i][0],target[i][2],0); /* side-on lift */
    }
    expect(-16384,0,0,1); /* previously accepted upright view */
    expect(-15000,3000,-5000,1);
    expect(0,0,-16384,0); /* flat */
    expect(-7504,1688,-14637,0); /* old failed scan, returned to flat gate */
    expect(-9361,953,-12301,0); /* old failed scan at518ms, leaving view range */
    expect(-14426,382,-6422,1); /* old scan's last accepted sample */
    expect(-13492,135,-7537,1); /* old Z failure is now inside view range */
    expect(-8276,1439,-14206,0);
    /* Boundaries: direction/side/backward limits, joint XZ angle and gravity. */
    expect(-9000,0,-10000,0);
    expect(-9001,0,-10000,1);
    expect(-14000,7999,-4000,1);
    expect(-14000,8000,-4000,0);
    expect(-14000,-8000,-4000,0);
    expect(-14000,0,6999,1);
    expect(-14000,0,7000,0);
    expect(-12000,0,6000,0);
    expect(-12001,0,6000,1);
    expect(-10000,0,-12247,1);
    expect(-10000,0,-12248,0);
    expect(-12999,0,0,0);
    expect(-13000,0,0,1);
    expect(-20000,0,0,1);
    expect(-20001,0,0,0);
    expect(0,0,0,0); /* free fall */
    expect(INT16_MIN,0,0,0); /* high acceleration */
    expect(INT16_MIN,INT16_MIN,INT16_MIN,0); /* arithmetic must not overflow */
    expect(INT16_MAX,INT16_MAX,INT16_MAX,0);
    puts("Tilted-view fixtures, opposite/side/flat rejection and boundaries passed");
    return 0;
}
