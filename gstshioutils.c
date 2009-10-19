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

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "gstshioutils.h"

#define FB_DEVICE "/dev/fb0"
#define VEU_NAME "VEU2H"
#define MAXNAMELEN 256

// Registers

#define VPDYR 0x10 /* vpu: decoded y/rgb plane address */
#define VPDCR 0x14 /* vpu: decoded c plane address */
#define VESTR 0x00 /* start register */
#define VESWR 0x10 /* src: line length */
#define VESSR 0x14 /* src: image size */
#define VSAYR 0x18 /* src: y/rgb plane address */
#define VSACR 0x1c /* src: c plane address */
#define VBSSR 0x20 /* bundle mode register */
#define VEDWR 0x30 /* dst: line length */
#define VDAYR 0x34 /* dst: y/rgb plane address */
#define VDACR 0x38 /* dst: c plane address */
#define VTRCR 0x50 /* transform control */
#define VRFCR 0x54 /* resize scale */
#define VRFSR 0x58 /* resize clip */
#define VENHR 0x5c /* enhance */
#define VFMCR 0x70 /* filter mode */
#define VVTCR 0x74 /* lowpass vertical */
#define VHTCR 0x78 /* lowpass horizontal */
#define VAPCR 0x80 /* color match */
#define VECCR 0x84 /* color replace */
#define VAFXR 0x90 /* fixed mode */
#define VSWPR 0x94 /* swap */
#define VEIER 0xa0 /* interrupt mask */
#define VEVTR 0xa4 /* interrupt event */
#define VSTAR 0xb0 /* status */
#define VBSRR 0xb4 /* reset */

#define VMCR00 0x200 /* color conversion matrix coefficient 00 */
#define VMCR01 0x204 /* color conversion matrix coefficient 01 */
#define VMCR02 0x208 /* color conversion matrix coefficient 02 */
#define VMCR10 0x20c /* color conversion matrix coefficient 10 */
#define VMCR11 0x210 /* color conversion matrix coefficient 11 */
#define VMCR12 0x214 /* color conversion matrix coefficient 12 */
#define VMCR20 0x218 /* color conversion matrix coefficient 20 */
#define VMCR21 0x21c /* color conversion matrix coefficient 21 */
#define VMCR22 0x220 /* color conversion matrix coefficient 22 */
#define VCOFFR 0x224 /* color conversion offset */
#define VCBR   0x228 /* color conversion clip */

gulong read_reg(uio_map *ump, gint reg_offs)
{
	volatile gulong *reg = ump->iomem;

	return reg[reg_offs / 4];
}

void write_reg(uio_map *ump, gulong value, gint reg_offs)
{
	volatile gulong *reg = ump->iomem;

	reg[reg_offs / 4] = value;
}

void
clear_framebuffer(framebuffer *fbuf)
{
	memset(fbuf->iomem, 0, fbuf->finfo.line_length * fbuf->vinfo.yres);
}

gboolean 
init_framebuffer(framebuffer *fbuf)
{
	gint fd;

	fd = open(FB_DEVICE, O_RDWR);
	if (fd < 0) 
	{
		return FALSE;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &fbuf->vinfo) == -1) 
	{
		return FALSE;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fbuf->finfo) == -1) 
	{
		return FALSE;
	}

	fbuf->iomem = mmap(0, fbuf->finfo.smem_len, 
			   PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if (fbuf->iomem == MAP_FAILED) 
	{
		return FALSE;
	}
	
	clear_framebuffer(fbuf);

	close(fd);
	return TRUE;
}

gint fgets_with_openclose(gchar *fname, gchar *buf, size_t maxlen)
{
		FILE *fp;

		fp = fopen(fname, "r");
		if (!fp)
			return -1;

		fgets(buf, maxlen, fp);
		fclose(fp);
		return strlen(buf);
}

int locate_uio_device(char *name, uio_device *udp)
{
	char fname[MAXNAMELEN], buf[MAXNAMELEN];
	char *tmp;
	int uio_id;

	uio_id = -1;
	do 
	{
		uio_id++;
		snprintf(fname, MAXNAMELEN, "/sys/class/uio/uio%d/name", 
			uio_id);
		if (fgets_with_openclose(fname, buf, MAXNAMELEN) < 0)
			return -1;
	} while (strncmp(name, buf, strlen(name)));

	tmp = strchr(buf, '\n');
	if (tmp)
		*tmp = '\0';

	udp->name = strdup(buf);
	udp->path = strdup(fname);
	udp->path[strlen(udp->path) - 5] = '\0';

	snprintf(buf, MAXNAMELEN, "/dev/uio%d", uio_id);
	udp->fd = open(buf, O_RDWR | O_SYNC);

	if (udp->fd < 0)
		return -1;

	return 0;
}

gint setup_uio_map(uio_device *udp, gint nr, uio_map *ump)
{
	gchar fname[MAXNAMELEN], buf[MAXNAMELEN];

	snprintf(fname, MAXNAMELEN, "%s/maps/map%d/addr", udp->path, nr);
	if (fgets_with_openclose(fname, buf, MAXNAMELEN) <= 0)
		return -1;

	ump->address = strtoul(buf, NULL, 0);

	snprintf(fname, MAXNAMELEN, "%s/maps/map%d/size", udp->path, nr);
	if (fgets_with_openclose(fname, buf, MAXNAMELEN) <= 0)
		return -1;

	ump->size = strtoul(buf, NULL, 0);
	ump->iomem = mmap(0, ump->size,
				PROT_READ|PROT_WRITE, MAP_SHARED,
				udp->fd, nr * getpagesize());

	if (ump->iomem == MAP_FAILED)
		return -1;

	return 0;
}

gboolean
init_module(uio_module *module, gchar *name)
{
	gint ret;

	ret = locate_uio_device(name, &module->dev);
	if (ret < 0) 
	{
		return FALSE;
	}

	ret = setup_uio_map(&module->dev, 0, &module->mmio);
	if (ret < 0) 
	{
		return FALSE;
	}

	ret = setup_uio_map(&module->dev, 1, &module->mem);
	if (ret < 0) 
	{
		return FALSE;
	}

	return TRUE;      
}

gboolean init_veu(uio_module *veu)
{
	if(!init_module(veu,VEU_NAME))
	{
		return FALSE;
	}
	
	write_reg(&veu->mmio, 0x100, VBSRR); /* reset VEU */

	return TRUE;
}

gulong do_scale(uio_map *ump, gint vertical, gint size_in, gint size_out, 
		gint crop_out)
{
		gulong fixpoint, mant, frac, value, rep;

		/* calculate FRAC and MANT */
		/* The formula is available in the SuperH data sheet. */
		do 
		{
			rep = mant = frac = 0;

			if (size_in == size_out) 
			{
				if (crop_out != size_out)
					mant = 1; /* needed for cropping */
				break;
			}

			/* VEU2H special upscale */
			if (size_out > size_in) 
			{
				fixpoint = (4096 * size_in) / size_out;

				mant = fixpoint / 4096;
				frac = fixpoint - (mant * 4096);
				frac &= ~0x07;

				switch (frac) 
				{
					case 0x800:
						rep = 1;
						break;
					case 0x400:
						rep = 3;
						break;
					case 0x200:
						rep = 7;
						break;
				}

				if (rep)
					break;
			}

			fixpoint = (4096 * (size_in - 1)) / (size_out + 1);
			mant = fixpoint / 4096;
			frac = fixpoint - (mant * 4096);

			if (frac & 0x07) 
			{
				frac &= ~0x07;
				if (size_out > size_in)
					frac -= 8; /* round down if scaling up */
				else
					frac += 8; /* round up if scaling down */
			}

		} while (0);

		/* set scale */
		value = read_reg(ump, VRFCR);
		if (vertical) 
		{
			value &= ~0xffff0000;
			value |= ((mant << 12) | frac) << 16;
		} 
		else 
		{
			value &= ~0xffff;
			value |= (mant << 12) | frac;
		}
		write_reg(ump, value, VRFCR);

		/* set clip */
		value = read_reg(ump, VRFSR);
		if (vertical) 
		{
			value &= ~0xffff0000;
			value |= ((rep << 12) | crop_out) << 16;
		} 
		else 
		{
			value &= ~0xffff;
			value |= (rep << 12) | crop_out;
		}
		write_reg(ump, value, VRFSR);

		return (((size_in * crop_out) / size_out) + 0x03) & ~0x03;
}

gboolean 
setup_veu(uio_module *veu, gint src_w, gint src_h, gint dst_w, gint dst_h, 
		gint dst_stride, gint pos_x, gint pos_y, 
		gint dst_max_w, gint dst_max_h, gulong dst_addr, gint bpp)
{
	gint src_stride, cropped_w, cropped_h;

	if(strcmp(veu->dev.name,VEU_NAME))
	{
		/* Not VEU */
		return FALSE;
	}

	/* Aligning */
	src_stride = (src_w+15) & ~15;
	pos_x = pos_x & ~0x03;
	
	/* Cropped sizes */
	cropped_w = dst_w;
	cropped_h = dst_h;
	if ((dst_w + pos_x) > dst_max_w)
		cropped_w = dst_max_w - pos_x;

	if ((dst_h + pos_y) > dst_max_h)
		cropped_h = dst_max_h - pos_y;

	dst_addr += pos_x * (bpp / 8);
	dst_addr += pos_y * dst_stride;

	src_w = do_scale(&veu->mmio, 0, src_w, dst_w, cropped_w);
	src_h = do_scale(&veu->mmio, 1, src_h, dst_h, cropped_h);

	write_reg(&veu->mmio, src_stride, VESWR);
	write_reg(&veu->mmio, src_w | (src_h << 16), VESSR);
	write_reg(&veu->mmio, 0, VBSSR); /* not using bundle mode */

	write_reg(&veu->mmio, dst_stride, VEDWR);
	write_reg(&veu->mmio, dst_addr, VDAYR);
	write_reg(&veu->mmio, 0, VDACR); /* unused for RGB */

	write_reg(&veu->mmio, 0x67, VSWPR);
	write_reg(&veu->mmio, (6 << 16) | 2 | 4, VTRCR); // NV12

	/* YUV->RGB Conversion coeffs */
	write_reg(&veu->mmio, 0x0cc5, VMCR00);
	write_reg(&veu->mmio, 0x0950, VMCR01);
	write_reg(&veu->mmio, 0x0000, VMCR02);

	write_reg(&veu->mmio, 0x397f, VMCR10);
	write_reg(&veu->mmio, 0x0950, VMCR11);
	write_reg(&veu->mmio, 0x3ccd, VMCR12);

	write_reg(&veu->mmio, 0x0000, VMCR20);
	write_reg(&veu->mmio, 0x0950, VMCR21);
	write_reg(&veu->mmio, 0x1023, VMCR22);

	write_reg(&veu->mmio, 0x00800010, VCOFFR);

	write_reg(&veu->mmio, 1, VEIER); /* enable interrupt in VEU */

	return TRUE;
}

gboolean
veu_blit(uio_module *veu, gulong y_addr, gulong c_addr)
{
	gulong enable = 1;

	if(strcmp(veu->dev.name,VEU_NAME))
	{
		/* Not VEU */
		return FALSE;
	}
		
	write_reg(&veu->mmio, y_addr, VSAYR);
	write_reg(&veu->mmio, c_addr, VSACR);

	/* Enable interrupt in UIO driver */
	write(veu->dev.fd, &enable, sizeof(gulong));

	write_reg(&veu->mmio, 1, VESTR); /* start operation */

	return TRUE;
}


void veu_wait_irq(uio_module *veu)
{
		unsigned long n_pending;

		read(veu->dev.fd, &n_pending, sizeof(unsigned long));

		write_reg(&veu->mmio, 0x100, VEVTR);
}
