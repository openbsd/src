
/*
 * RAM-disk ioctl functions:
 */

#include <sys/ioccom.h>

struct rd_conf {
	caddr_t rd_addr;
	size_t  rd_size;
	int     rd_type;
};

#define RD_GETCONF	_IOR('r', 0, struct rd_conf)	/* get unit config */
#define RD_SETCONF	_IOW('r', 1, struct rd_conf)	/* set unit config */

/*
 * There are three configurations supported for each unit,
 * reflected in the value of the rd_type field:
 */
#define RD_UNCONFIGURED 0
/*
 *     Not yet configured.  Open returns ENXIO.
 */
#define RD_KMEM_FIXED	1
/*
 *     Disk image resident in kernel (patched in or loaded).
 *     Requires that the function: rd_set_kmem() is called to
 *     attach the (initialized) kernel memory to be used by the
 *     device.  This can be called by an "open hook" if this
 *     driver is compiled with the RD_OPEN_HOOK option.
 *     No attempt will be made to free this memory.
 */
#define RD_KMEM_ALLOCATED 2
/*
 *     Small, wired-down chunk of kernel memory obtained from
 *     kmem_alloc().  The allocation is performed by an ioctl
 *     call on the "control" unit (regular unit + 16)
 */
#define RD_UMEM_SERVER 3
/*
 *     Indirect access to user-space of a user-level server.
 *     (Like the MFS hack, but better! 8^)  Device operates
 *     only while the server has the control device open and
 *     continues to service I/O requests.  The process that
 *     does this setconf will become the I/O server. 
 *     Support for this configuration type is optional:
 *         option  RAMDISK_SERVER
 */

#ifdef	_KERNEL
/*
 * If the option RAMDISK_HOOKS is on, then these functions are
 * called by the ramdisk driver to allow machine-dependent to
 * configure and/or load each ramdisk unit.
 */
extern void rd_attach_hook __P((int unit, struct rd_conf *));
extern void rd_open_hook   __P((int unit, struct rd_conf *));
#endif
