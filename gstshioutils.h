/**
 * SuperH VEU color space conversion and scaling
 * Based on MPlayer Vidix driver by Magnus Damm
 * Modified for GStreamer video display sink usage
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
 *
 * \author Pablo Virolainen <pablo.virolainen@nomovok.com>
 * \author Johannes Lahti <johannes.lahti@nomovok.com>
 * \author Aki Honkasuo <aki.honkasuo@nomovok.com>
 *
 */

#ifndef  GSTSHIOUTILS_H
#define  GSTSHIOUTILS_H

#include <linux/fb.h>
#include <glib.h>

/**
 * \struct _framebuffer gstshioutils.h
 * \var vinfo Variable framebuffer info
 * \var finfo Fixed framebuffer info
 * \var iomem Pointer to the framebuffer memory space
 */
typedef struct _framebuffer 
{
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	void *iomem;
}framebuffer;

/**
 * \struct _uio_device gstshioutils.h
 * \var name Name of the device
 * \var path Path of the device
 * \var fd File handle to the device
 */
typedef struct _uio_device 
{
	gchar *name;
	gchar *path;
	gint fd;
}uio_device;

/**
 * \struct _uio_map gstshioutils.h
 * \var address Start address of the UIO device memory
 * \var size Size of the UIO device memory
 * \var iomem Pointer to the UIO device memory
 */
typedef struct _uio_map 
{
	gulong address;
	gulong size;
	void *iomem;
}uio_map;

/**
 * \struct _uio_module gstshioutils.h
 * \var dev The device of the module
 * \var mmio The mapped register are of the module
 * \var mem The mapped memory of the module
 */
typedef struct _uio_module 
{
	uio_device dev;
	uio_map mmio;
	uio_map mem;
}uio_module;

/**
 * Clear the framebuffer
 * \param pointer to the framebuffer
 */
void clear_framebuffer(framebuffer *fbuf);

/**
 * Initialize the framebuffer
 * \param pointer to the framebuffer
 */
gboolean init_framebuffer(framebuffer *fbuf);

/**
 * Initialize the Video Engine Unit
 * \param pointer to the VEU
 */
gboolean init_veu(uio_module *veu);

/**
 * Setup the Video Engine Unit
 * \param pointer to the VEU
 * \param src_w Width of the source stream
 * \param src_h Height of the source stream
 * \param dst_w Width of the destination stream
 * \param dst_h Height of the destination stream
 * \param dst_stride Line length of the destination buffer
 * \param pos_x X -coordinate of the destination stream
 * \param pos_y Y -coordinate of the destination stream
 * \param dst_max_w Maximum width of the destination stream
 * \param dst_max_h Maximum height of the destination stream
 * \param dst_addr Address of the destination buffer
 * \param bpp Bits per pixel
 */
gboolean setup_veu(uio_module *veu, gint src_w, gint src_h, gint dst_w, gint dst_h, 
		   gint dst_stride, gint pos_x, gint pos_y, gint dst_max_w, 
		   gint dst_max_h, gulong dst_addr, gint bpp);

/**
 * Blit the image using VEU
 * \param pointer to the VEU
 * \param y_addr Address of the Y -data
 * \param c_addr Address of the C -data
 */
gboolean veu_blit(uio_module *veu, gulong y_addr, gulong c_addr);

void veu_wait_irq(uio_module *veu);


#endif // GSTSHIOUTILS_H
