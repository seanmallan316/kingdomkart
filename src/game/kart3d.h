// kart3d.h - a racer's real 3D kart (made from Sean's pictures by tools/model3d): the body with the driver, and four
// wheels that spin and steer. Loaded from content/models/<name>.kkm and <name>.png.
#ifndef KK_KART3D_H
#define KK_KART3D_H
#include "../render/gfx.h"
#include "../render/kkmath.h"
enum { K3_BODY, K3_FL, K3_FR, K3_BL, K3_BR, K3_PARTS };
typedef struct { int ok; Mesh *m[K3_PARTS]; Vec3 piv[K3_PARTS]; float rad[K3_PARTS]; Texture *tex; Vec3 lo, hi; } Kart3D;
int kart3d_load(Kart3D *k, const char *name);   /* 1 if this racer has a 3D kart */
/* draws it: 'base' places the kart (its ground point, facing +z); wheel_ang = how far the wheels have rolled, steer = front-wheel turn */
void kart3d_draw(const Kart3D *k, const Mat4 *base, float wheel_ang, float steer, float r, float g, float b, float a);
#endif
