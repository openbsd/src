/*	$Id: sysdep.h,v 1.1.1.1 1995/10/18 08:39:55 deraadt Exp $ */

#ifndef hosts_sparc_H
#define hosts_sparc_H

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <sys/file.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/reg.h>

#ifndef	O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

#define SEEK_SET 0
#define SEEK_CUR 1

#include "fopen-same.h"


#define	HOST_PAGE_SIZE			NBPG
#define	HOST_MACHINE_ARCH		bfd_arch_sparc
#define	HOST_TEXT_START_ADDR		USRTEXT

#define	HOST_STACK_END_ADDR		USRSTACK
/*
#define HOST_DATA_START_ADDR ((bfd_vma)u.u_kproc.kp_eproc.e_vm.vm_daddr)
*/

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
#define TRAD_CORE_REGPOS(core_bfd) \
  ((bfd_vma)(core_bfd)->tdata.trad_core_data->u.u_kproc.kp_proc.p_md.md_tf)

#define CORE_FPU_OFFSET	(sizeof(struct trapframe))

#endif
