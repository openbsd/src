/* Intel 386 running NetBSD */

#ifndef	hosts_i386bsd_H
#define hosts_i386bsd_H

#include "hosts/nbsd.h"

#define	HOST_MACHINE_ARCH	bfd_arch_i386

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
#endif

