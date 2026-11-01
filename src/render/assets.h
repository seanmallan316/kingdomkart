// assets.h - pictures packed into the game file.
#ifndef KK_ASSETS_H
#define KK_ASSETS_H
#include "gfx.h"
Texture *asset_texture(const char *name, int repeat, int *out_w, int *out_h);   /* NULL if the picture is missing */
#endif
