// kart3d.h - a racer's real 3D kart (made from Sean's pictures by tools/model3d): the body with the driver, and four
// wheels that spin and steer. Loaded from content/models/<name>.kkm and <name>.png.
// A kart packed by tools/model3d/pack_rig.py also has a driver who moves: torso, head, upper arms, forearms and the
// steering wheel are their own pieces, joined at the hips, neck, shoulders and elbows (see K3Pose).
#ifndef KK_KART3D_H
#define KK_KART3D_H
#include "../render/gfx.h"
#include "../render/kkmath.h"
enum { K3_BODY, K3_FL, K3_FR, K3_BL, K3_BR,
       K3_TORSO, K3_HEAD, K3_UARM_L, K3_UARM_R, K3_FARM_L, K3_FARM_R, K3_SWHEEL,   /* the moving driver; pivots: hips, neck, shoulders, elbows, hub */
       K3_GRIP_L, K3_GRIP_R, K3_SWAXIS,                                           /* points only: where each hand holds the wheel, the steering column's direction */
       K3_PARTS };
/* what an arm is doing (the driver's own left is +x) */
enum { K3A_WHEEL, K3A_WAVE, K3A_UP, K3A_PUMP, K3A_GUARD, K3A_POINT };
typedef struct {
    float lean, bow, twist, bob;          /* upper body: lean sideways (+ = to the driver's right), bow forward (+), turn (+ = left), bounce up (m) */
    float look_yaw, look_pitch, tilt;     /* head: turn (+ = left), look up (+), tilt (+ = toward the right shoulder) */
    int arm[2]; float arm_w[2];           /* [0] left, [1] right: K3A_*, and how far into it (0 = hands on the wheel, 1 = fully) */
    float t;                              /* seconds, drives the waving */
} K3Pose;
typedef struct { int ok, rig; Mesh *m[K3_PARTS]; Vec3 piv[K3_PARTS]; float rad[K3_PARTS];   /* rad: a wheel's radius; for the head, how far it may turn (0 = no limit of its own) */
                Texture *tex; Vec3 lo, hi; } Kart3D;
extern Texture *g_kart3d_tex_override;
extern float g_kart3d_wheel_blur, g_kart3d_wheel_step;   /* set before drawing a kart: how blurred its wheels are (0..1) and how far they turn in a frame */   /* set while loading a low-detail copy: it shares the full kart's picture */
int kart3d_load(Kart3D *k, const char *name);   /* 1 if this racer has a 3D kart */
/* draws it: 'base' places the kart (its ground point, facing +z); wheel_ang = how far the wheels have rolled, steer = front-wheel turn */
void kart3d_draw(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, float r, float g, float b, float a);
/* the same with the driver posed (pose may be NULL: sitting still, hands on the wheel) */
void kart3d_draw_pose(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, const K3Pose *pose, float r, float g, float b, float a);
/* every piece flat in one colour, unlit (the shadow squashed on the road) */
void kart3d_draw_flat(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, const K3Pose *pose, float r, float g, float b, float a);
#endif
