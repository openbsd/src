/* m68k hosts running NetBSD */

#ifndef hosts_m68knbsd_h
#define hosts_m68knbsd_h

#include "hosts/nbsd.h"

#define	HOST_MACHINE_ARCH	bfd_arch_m68k

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm
#endif
