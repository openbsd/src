/* ns32k running NetBSD */

#ifndef	hosts_ns32knbsd_h
#define hosts_ns32knbsd_h

#include "hosts/nbsd.h"

#define	HOST_MACHINE_ARCH	bfd_arch_ns32k

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
#endif
