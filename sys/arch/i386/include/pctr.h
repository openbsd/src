/*	$OpenBSD: pctr.h,v 1.1 1996/08/08 18:47:04 dm Exp $	*/

/*
 * Pentium performance counter driver for OpenBSD.
 * Author: David Mazieres <dm@lcs.mit.edu>
 */

#ifndef _I386_PERFCNT_H_
#define _I386_PERFCNT_H_

typedef u_quad_t pctrval;

#define PCTR_NUM 2

struct pctrst {
  u_short pctr_fn[PCTR_NUM];
  pctrval pctr_tsc;
  pctrval pctr_hwc[PCTR_NUM];
  pctrval pctr_idl;
};

/* Bit values in fn fields and PIOCS ioctl's */
#define PCTR_K 0x40    /* Monitor kernel-level events */
#define PCTR_U 0x80    /* Monitor user-level events */
#define PCTR_C 0x100   /* count cycles rather than events */

/* ioctl to set which counter a device tracks. */
#define PCIOCRD _IOR('c', 1, struct pctrst)   /* Read counter value */
#define PCIOCS0 _IOW('c', 8, unsigned short)  /* Set counter 0 function */
#define PCIOCS1 _IOW('c', 9, unsigned short)  /* Set counter 1 function */

#define _PATH_PCTR "/dev/pctr"

#endif /* ! _I386_PERFCNT_H_ */
