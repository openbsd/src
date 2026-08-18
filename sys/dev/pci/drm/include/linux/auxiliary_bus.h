/* Public domain. */

#ifndef _LINUX_AUXILIARY_BUS_H
#define _LINUX_AUXILIARY_BUS_H

struct auxiliary_device {
};

static inline void
auxiliary_device_delete(struct auxiliary_device *ad)
{
}

static inline void
auxiliary_device_uninit(struct auxiliary_device *ad)
{
}

static inline int
auxiliary_device_init(struct auxiliary_device *ad)
{
	return 0;
}

static inline int
auxiliary_device_add(struct auxiliary_device *ad)
{
	return 0;
}

#endif
