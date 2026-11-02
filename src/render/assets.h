// assets.h - pictures packed into the game file.
#ifndef KK_ASSETS_H
#define KK_ASSETS_H
#include "gfx.h"
#ifndef KK_CONTENT_DIR
#define KK_CONTENT_DIR "/vol/content/"
#endif
Texture *asset_texture(const char *name, int repeat, int *out_w, int *out_h);   /* NULL if the picture is missing */
Texture *asset_texture_file(const char *relpath, int mipmaps, int *out_w, int *out_h);   /* any png/jpg under content/, e.g. "video/opener_012.jpg" */
int asset_exists(const char *relpath);
#endif
