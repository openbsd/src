/* Public domain. */

#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <sys/types.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/backlight.h>
#include <linux/kgdb.h>
#include <linux/fs.h>
#include <linux/i2c.h> /* via uapi/linux/fb.h */

struct fb_cmap;
struct fb_fillrect;
struct fb_copyarea;
struct fb_image;
struct fb_info;

struct apertures_struct;

struct fb_var_screeninfo {
	int pixclock;
	uint32_t xres;
	uint32_t yres;
	uint32_t width;
	uint32_t height;
};

struct fb_ops {
	int (*fb_set_par)(struct fb_info *);
};

struct fb_info {
	struct fb_var_screeninfo var;
	const struct fb_ops *fbops;
	char *screen_buffer;
	char *screen_base;
	bus_size_t screen_size;
	void *par;
	int fbcon_rotate_hint;
	bool skip_vt_switch;
	int flags;
};

#define KHZ2PICOS(a)	(1000000000UL/(a))

#define FB_BLANK_UNBLANK	0
#define FB_BLANK_NORMAL		1
#define FB_BLANK_HSYNC_SUSPEND	2
#define FB_BLANK_VSYNC_SUSPEND	3
#define FB_BLANK_POWERDOWN	4

#define FBINFO_STATE_RUNNING	0
#define FBINFO_STATE_SUSPENDED	1

#define FBINFO_DEFAULT		0
#define FBINFO_VIRTFB		1
#define FBINFO_READS_FAST	2

#define FBINFO_HIDE_SMEM_START	0

#define FB_ROTATE_UR		0
#define FB_ROTATE_CW		1
#define FB_ROTATE_UD		2
#define FB_ROTATE_CCW		3

static inline struct fb_info *
framebuffer_alloc(size_t size, void *dev)
{
	return kzalloc(sizeof(struct fb_info) + size, GFP_KERNEL);
}

static inline void
fb_set_suspend(struct fb_info *fbi, int s)
{
}

static inline void
framebuffer_release(struct fb_info *fbi)
{
	kfree(fbi);
}

static inline int
fb_get_options(const char *name, char **opt)
{
	return 0;
}

static inline int
register_framebuffer(struct fb_info *fbi)
{
	if (fbi->fbops && fbi->fbops->fb_set_par)
		fbi->fbops->fb_set_par(fbi);
	return 0;
}

static inline void
unregister_framebuffer(struct fb_info *fbi)
{
}

#endif
