/*	$OpenBSD: aperture.c,v 1.5 2000/03/02 01:49:09 todd Exp $	*/

/*
 * Copyright 1994 the XFree86 Project Inc. 
 */

/* 
 * linear framebuffer aperture driver for OpenBSD
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>

#define VGA_START 0xA0000
#ifdef PC98 
#define VGA_END   0xFFFFF
#define HOLE16M_START	0xF00000
#define HOLE16M_END	0xFFFFFF
#else 
#define VGA_END   0xBFFFF
#endif 

/* open counter */
static int ap_open_count = 0;

/*
 * Open the device
 */
int
apopen(dev_t dev, int oflags, int devtype, struct proc *p)
{

    if (suser(p->p_ucred, &p->p_acflag) != 0) {
	return(EPERM);
    }
    /* authorize only one simultaneous open() */
    if (ap_open_count > 0) {
	return(EPERM);
    }
    ap_open_count++;

    return(0);
}

/*
 * Close the device
 */
int
apclose(dev_t dev, int cflags, int devtype, struct proc *p)
{

    ap_open_count--;
    return(0);
}

/*
 *  mmap() physical memory sections
 * 
 * allow only section in the vga framebuffer and above main memory 
 * to be mapped
 */
int
apmmap(dev_t dev, int offset, int length)
{

#ifdef AP_DEBUG
    printf("apmmap: addr 0x%x\n", offset);
#endif
    if  ((minor(dev) == 0) 
	 && ((offset >= VGA_START && offset <= VGA_END )
	     || (unsigned)offset > (unsigned)ctob(physmem)
#ifdef PC98
	     || ((unsigned)offset >=HOLE16M_START
                   && (unsigned)offset <= HOLE16M_END)
#endif
	     )) {
	return i386_btop(offset);
    } else {
	return(-1);
    }
}
       
