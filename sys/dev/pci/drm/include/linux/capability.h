/* Public domain. */

#ifndef _LINUX_CAPABILITY_H
#define _LINUX_CAPABILITY_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <machine/cpu.h>

#define CAP_SYS_ADMIN	0x1
static inline int 
capable(int cap) 
{ 
	KASSERT(cap == CAP_SYS_ADMIN);
	return suser(curproc) == 0;
} 

#endif
