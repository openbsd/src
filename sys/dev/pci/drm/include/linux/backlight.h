/* Public domain. */

#ifndef _LINUX_BACKLIGHT_H
#define _LINUX_BACKLIGHT_H

#include <sys/task.h>
#include <linux/fb.h>

struct backlight_device;
struct device;

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

static inline void *
bl_get_data(struct backlight_device *bd)
{
	return bd->data;
}

#define BACKLIGHT_RAW		0
#define BACKLIGHT_FIRMWARE	1

#define BACKLIGHT_UPDATE_HOTKEY	0

struct backlight_device *backlight_device_register(const char *, void *,
    void *, const struct backlight_ops *, struct backlight_properties *);
void backlight_device_unregister(struct backlight_device *);

struct backlight_device *devm_backlight_device_register(void *, const char *,
    void *, void *, const struct backlight_ops *,
    const struct backlight_properties *);

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

static inline void
backlight_device_set_brightness(struct backlight_device *bd, int level)
{
	if (level > bd->props.max_brightness)
		return;
	bd->props.brightness = level;
	bd->ops->update_status(bd);
}

void backlight_schedule_update_status(struct backlight_device *);

int backlight_enable(struct backlight_device *);
int backlight_disable(struct backlight_device *);

static inline struct backlight_device *
devm_of_find_backlight(struct device *dev)
{
	return NULL;
}

static inline struct backlight_device *
backlight_device_get_by_name(const char *name)
{
	return NULL;
}

#endif
