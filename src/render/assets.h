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
/* the opener video: frames content/<prefix>NNN.jpg decoded ahead of time on the other CPU cores */
int vid_open(const char *prefix, int nframes, int w, int h);   /* 0 if there is no memory for it */
int vid_update(int want);                                      /* call every frame with the frame that should be showing; returns frames waiting */
Texture *vid_texture(void);                                    /* NULL until the first frame is ready */
void vid_close(void);
#endif
