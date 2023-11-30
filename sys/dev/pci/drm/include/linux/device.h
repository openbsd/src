/* Public domain. */

#ifndef _LINUX_DEVICE_H
#define _LINUX_DEVICE_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <linux/ioport.h>
#include <linux/lockdep.h>
#include <linux/pm.h>
#include <linux/kobject.h>
#include <linux/ratelimit.h> /* dev_printk.h -> ratelimit.h */

struct device_node;

struct device_driver {
	struct device *dev;
};

struct device_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *, struct device_attribute *, char *);
};

#define DEVICE_ATTR(_name, _mode, _show, _store) \
	struct device_attribute dev_attr_##_name
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name

#define device_create_file(a, b)	0
#define device_remove_file(a, b)

#define dev_get_drvdata(x)	NULL
#define dev_set_drvdata(x, y)

#define dev_pm_set_driver_flags(x, y)

#define devm_kzalloc(x, y, z)	kzalloc(y, z)

#define dev_warn(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_WARN(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_notice(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *NOTICE* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_crit(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_err(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_emerg(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *EMERGENCY* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_printk(level, dev, fmt, arg...)				\
	printf("drm:pid%d:%s *PRINTK* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)

#define dev_warn_ratelimited(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_notice_ratelimited(dev, fmt, arg...)			\
	printf("drm:pid%d:%s *NOTICE* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_err_ratelimited(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)

#define dev_warn_once(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_err_once(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)

#ifdef DRMDEBUG
#define dev_info(dev, fmt, arg...)				\
	printf("drm: " fmt, ## arg)
#define dev_info_once(dev, fmt, arg...)				\
	printf("drm: " fmt, ## arg)
#define dev_dbg(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *DEBUG* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_dbg_once(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *DEBUG* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_dbg_ratelimited(dev, fmt, arg...)			\
	printf("drm:pid%d:%s *DEBUG* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#else
#define dev_info(dev, fmt, arg...) 				\
	    do { } while(0)
#define dev_info_once(dev, fmt, arg...) 			\
	    do { } while(0)
#define dev_dbg(dev, fmt, arg...) 				\
	    do { } while(0)
#define dev_dbg_once(dev, fmt, arg...) 				\
	    do { } while(0)
#define dev_dbg_ratelimited(dev, fmt, arg...) 			\
	    do { } while(0)
#endif

static inline const char *
dev_driver_string(struct device *dev)
{
	return dev->dv_cfdata->cf_driver->cd_name;
}

/* XXX return true for thunderbolt/USB4 */
#define dev_is_removable(x)	false

/* should be bus id as string, ie 0000:00:02.0 */
#define dev_name(dev)		""

#endif
