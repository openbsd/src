/* $OpenBSD: osf1_cvt.h,v 1.1 2000/08/04 15:47:54 ericj Exp $ */
/* $NetBSD: osf1_cvt.h,v 1.5 1999/05/10 05:58:44 cgd Exp $ */

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _COMPAT_OSF1_OSF1_CVT_H_
#define _COMPAT_OSF1_OSF1_CVT_H_

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include <compat/common/compat_util.h>

#define	osf1_cvt_dev_from_native(dev)					\
    osf1_makedev(major(dev), minor(dev))
#define	osf1_cvt_dev_to_native(dev)					\
    makedev(osf1_major(dev), osf1_minor(dev))

void	osf1_cvt_flock_from_native(const struct flock *nf,
				   struct osf1_flock *of);
int	osf1_cvt_flock_to_native(const struct osf1_flock *of,
				 struct flock *nf);
int	osf1_cvt_msghdr_xopen_to_native(const struct osf1_msghdr_xopen *omh,
					struct msghdr *nmh);
int	osf1_cvt_pathconf_name_to_native(int oname, int *bnamep);
void	osf1_cvt_rusage_from_native(const struct rusage *nru,
				    struct osf1_rusage *oru);
void	osf1_cvt_sigaction_from_native(const struct sigaction *nsa,
				       struct osf1_sigaction *osa);
int	osf1_cvt_sigaction_to_native(const struct osf1_sigaction *osa,
				     struct sigaction *nsa);
void	osf1_cvt_sigaltstack_from_native(const struct sigaltstack *nss,
					 struct osf1_sigaltstack *oss);
int	osf1_cvt_sigaltstack_to_native(const struct osf1_sigaltstack *oss,
				       struct sigaltstack *nss);
void	osf1_cvt_sigset_from_native(const sigset_t *nss, osf1_sigset_t *oss);
int	osf1_cvt_sigset_to_native(const osf1_sigset_t *oss, sigset_t *nss);
void	osf1_cvt_stat_from_native(const struct stat *nst,
				  struct osf1_stat *ost);
void	osf1_cvt_statfs_from_native(const struct statfs *nsfs,
				    struct osf1_statfs *osfs);

extern const int osf1_errno_rxlist[];
extern const int osf1_signal_xlist[];
extern const int osf1_signal_rxlist[];

extern const struct emul_flags_xtab osf1_access_flags_xtab[];
extern const struct emul_flags_xtab osf1_fcntl_getsetfd_flags_rxtab[];
extern const struct emul_flags_xtab osf1_fcntl_getsetfd_flags_xtab[];
extern const struct emul_flags_xtab osf1_fcntl_getsetfl_flags_rxtab[];
extern const struct emul_flags_xtab osf1_fcntl_getsetfl_flags_xtab[];
extern const struct emul_flags_xtab osf1_mmap_flags_xtab[];
extern const struct emul_flags_xtab osf1_mmap_prot_xtab[];
extern const struct emul_flags_xtab osf1_nfs_mount_flags_xtab[];
extern const struct emul_flags_xtab osf1_open_flags_rxtab[];
extern const struct emul_flags_xtab osf1_open_flags_xtab[];
extern const struct emul_flags_xtab osf1_reboot_opt_xtab[];
extern const struct emul_flags_xtab osf1_sendrecv_msg_flags_xtab[];
extern const struct emul_flags_xtab osf1_sigaction_flags_rxtab[];
extern const struct emul_flags_xtab osf1_sigaction_flags_xtab[];
extern const struct emul_flags_xtab osf1_sigaltstack_flags_rxtab[];
extern const struct emul_flags_xtab osf1_sigaltstack_flags_xtab[];
extern const struct emul_flags_xtab osf1_wait_options_xtab[];

#endif /* _COMPAT_OSF1_OSF1_CVT_H_ */
