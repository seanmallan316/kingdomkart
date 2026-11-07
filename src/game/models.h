// models.h - 3D models for scenery and road obstacles: smooth shapes with real surfaces, built in code.
#ifndef KK_MODELS_H
#define KK_MODELS_H
#include "../render/batch.h"
#include <stdint.h>
/* the surfaces: each model's corners go into one bucket per surface, drawn with that surface's picture */
enum { MAT_PLAIN, MAT_PLASTER, MAT_STONE, MAT_WOOD, MAT_BARK, MAT_LEAF, MAT_LEAFCARD, MAT_FROND, MAT_CLOTH, MAT_ROCK, MAT_WOOL, MAT_THATCH, MAT_N };
typedef struct { Batch *b[MAT_N]; } ModelOut;
int model_known(const char *img);                  /* is there a 3D model for this scenery name ("#olive", "#house1", ...)? */
/* adds it standing at 'at' facing 'yaw', about h tall and w wide; tr,tg,tb = the road's building stone color. Returns corners added, -1 if a bucket is too full */
int model_build(ModelOut *o, const char *img, float h, float w, unsigned seed, Vec3 at, float yaw, float tr, float tg, float tb);
void model_texture(int mat, uint32_t *rgba, int size);   /* draws the surface's picture (repeats seamlessly), size x size */
int model_mat_cutout(int mat);                     /* has see-through parts (leaves, fronds) */
int model_tire(Batch *b);
int model_jwall(Batch *b, float L, float H, float T, int rahab, unsigned seed);   /* a stretch of the walls of Jericho, for the stone picture */                          /* a kart tire: radius 1, axle along x, 1 wide */
#endif
