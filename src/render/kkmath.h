// kkmath.h - small vector and matrix helpers. Matrices are column-major float[16], the layout GLSL expects.
#ifndef KK_MATH_H
#define KK_MATH_H
#include <math.h>
#include <string.h>

typedef struct { float x, y, z; } Vec3;
typedef struct { float m[16]; } Mat4;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline Vec3 v3(float x, float y, float z) { Vec3 r = {x, y, z}; return r; }
static inline Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 v3_cross(Vec3 a, Vec3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 v3_norm(Vec3 a) { float l = sqrtf(v3_dot(a, a)); if (l < 1e-8f) return v3(0, 0, 1); return v3(a.x / l, a.y / l, a.z / l); }

static inline Mat4 m4_identity(void) { Mat4 r; memset(&r, 0, sizeof r); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f; return r; }
static inline Mat4 m4_mul(Mat4 a, Mat4 b) {              // a * b (apply b first, then a)
    Mat4 r;
    for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) {
        float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
    }
    return r;
}
static inline Mat4 m4_translate(float x, float y, float z) { Mat4 r = m4_identity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
static inline Mat4 m4_scale(float x, float y, float z) { Mat4 r = m4_identity(); r.m[0] = x; r.m[5] = y; r.m[10] = z; return r; }
static inline Mat4 m4_rotate_y(float a) { Mat4 r = m4_identity(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
static inline Mat4 m4_rotate_x(float a) { Mat4 r = m4_identity(); float c = cosf(a), s = sinf(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
static inline Mat4 m4_rotate_z(float a) { Mat4 r = m4_identity(); float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r; }
// right-handed, OpenGL-style clip space (depth -1..1), which is what GX2 uses by default
static inline Mat4 m4_perspective(float fovy_rad, float aspect, float zn, float zf) {
    Mat4 r; memset(&r, 0, sizeof r);
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1.0f; r.m[14] = (2.0f * zf * zn) / (zn - zf);
    return r;
}
static inline Mat4 m4_look_at(Vec3 eye, Vec3 at, Vec3 up) {
    Vec3 f = v3_norm(v3_sub(at, eye)), s = v3_norm(v3_cross(f, up)), u = v3_cross(s, f);
    Mat4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3_dot(s, eye); r.m[13] = -v3_dot(u, eye); r.m[14] = v3_dot(f, eye);
    return r;
}
#endif
