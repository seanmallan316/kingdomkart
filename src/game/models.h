// models.h - 3D models for scenery and road obstacles, built from simple shapes.
#ifndef KK_MODELS_H
#define KK_MODELS_H
#include "../render/batch.h"
int model_known(const char *img);                                  /* is there a 3D model for this scenery name ("#olive", ...)? */
int model_build(Batch *b, const char *img, float h, unsigned seed); /* adds it to b, standing at the origin facing +z, about h tall; returns corners added */
int model_tire(Batch *b);                                          /* a kart tire: radius 1, axle along x, 1 wide */
#endif
