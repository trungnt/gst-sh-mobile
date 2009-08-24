/*
 * SuperH VEU color space conversion and scaling
 * Based on MPlayer Vidix driver by Magnus Damm
 * Modified for GStreamer video display sink usage
 */

#ifndef  GSTSHIOUTILS_H
#define  GSTSHIOUTILS_H

#include <linux/fb.h>
#include <glib.h>

typedef struct _framebuffer 
{
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
  void *iomem;
}framebuffer;

typedef struct _uio_device 
{
  char *name;
  char *path;
  int fd;
}uio_device;

typedef struct _uio_map 
{
  unsigned long address;
  unsigned long size;
  void *iomem;
}uio_map;

typedef struct _uio_module 
{
  uio_device dev;
  uio_map mmio;
  uio_map mem;
}uio_module;

void clear_framebuffer(framebuffer *fbuf);

gboolean init_framebuffer(framebuffer *fbuf);

gboolean init_veu(uio_module *veu);

gboolean setup_veu(uio_module *veu, int src_w, int src_h, int dst_w, int dst_h, 
	       int dst_stride, int pos_x, int pos_y, 
	       int dst_max_w, int dst_max_h, unsigned long dst_addr, int bpp);

gboolean veu_blit(uio_module *veu, unsigned long y_addr, unsigned long c_addr);

#endif // GSTSHIOUTILS_H
