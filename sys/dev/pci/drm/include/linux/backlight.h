/* Public domain. */

#ifndef _LINUX_BACKLIGHT_H
#define _LINUX_BACKLIGHT_H

#include <sys/task.h>

struct backlight_device;

struct backlight_properties {
	int type;
	int max_brightness;
	int brightness;
	int power;
};

struct backlight_ops {
	int options;
	int (*update_status)(struct backlight_device *);
	int (*get_brightness)(struct backlight_device *);
};

#define BL_CORE_SUSPENDRESUME	1

struct backlight_device {
	const struct backlight_ops *ops;
	struct backlight_properties props;
	struct task task;
	void *data;
};

#define bl_get_data(bd)	(bd)->data

#define BACKLIGHT_RAW		0
#define BACKLIGHT_FIRMWARE	1

#define BACKLIGHT_UPDATE_HOTKEY	0

struct backlight_device *backlight_device_register(const char *, void *,
     void *, const struct backlight_ops *, struct backlight_properties *);
void backlight_device_unregister(struct backlight_device *);

static inline void
backlight_update_status(struct backlight_device *bd)
{
	bd->ops->update_status(bd);
}

static inline void
backlight_force_update(struct backlight_device *bd, int reason)
{
	bd->props.brightness = bd->ops->get_brightness(bd);
}

void backlight_schedule_update_status(struct backlight_device *);

int backlight_enable(struct backlight_device *);
int backlight_disable(struct backlight_device *);

#define devm_of_find_backlight(x)	NULL

#endif
