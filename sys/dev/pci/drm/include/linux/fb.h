/* Public domain. */

#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <sys/types.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/backlight.h>
#include <linux/kgdb.h>

struct fb_var_screeninfo {
	int pixclock;
	uint32_t width;
	uint32_t height;
};

struct fb_info {
	struct fb_var_screeninfo var;
	char *screen_buffer;
	void *par;
	int fbcon_rotate_hint;
	bool skip_vt_switch;
};

#define FB_BLANK_UNBLANK	0
#define FB_BLANK_NORMAL		1
#define FB_BLANK_HSYNC_SUSPEND	2
#define FB_BLANK_VSYNC_SUSPEND	3
#define FB_BLANK_POWERDOWN	4

#define FBINFO_STATE_RUNNING	0
#define FBINFO_STATE_SUSPENDED	1

#define FB_ROTATE_UR		0
#define FB_ROTATE_CW		1
#define FB_ROTATE_UD		2
#define FB_ROTATE_CCW		3

#define framebuffer_alloc(flags, device) \
	kzalloc(sizeof(struct fb_info), GFP_KERNEL)

#define fb_set_suspend(x, y)

#endif
