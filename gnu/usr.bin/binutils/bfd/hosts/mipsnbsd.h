/* mips running NetBSD */

#ifndef	hosts_mips
#define hosts_mips

#include "hosts/nbsd.h"

#define	HOST_MACHINE_ARCH	bfd_arch_mips

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
#endif
