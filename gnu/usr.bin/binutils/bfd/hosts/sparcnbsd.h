/* Sparc running NetBSD */

#ifndef hosts_sparcnbsd_h
#define hosts_sparcnbsd_h

#include "hosts/nbsd.h"

#define	HOST_MACHINE_ARCH	bfd_arch_sparc

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
#define TRAD_CORE_REGPOS(core_bfd) \
  ((bfd_vma)(core_bfd)->tdata.trad_core_data->u.u_kproc.kp_proc.p_md.md_tf)

#define CORE_FPU_OFFSET	(sizeof(struct trapframe))

#endif
