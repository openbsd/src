/*	$NetBSD: hpux_machdep.h,v 1.1 1996/01/06 12:44:08 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Jason R. Thorpe.
 * All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_HPUX_MACHDEP_H_
#define _MACHINE_HPUX_MACHDEP_H_

int	hpux_cpu_makecmds __P((struct proc *, struct exec_package *));
int	hpux_cpu_vmcmd __P((struct proc *, struct exec_vmcmd *));
void	hpux_cpu_bsd_to_hpux_stat __P((struct stat *, struct hpux_stat *));
void	hpux_cpu_uname __P((struct hpux_utsname *));
int	hpux_cpu_sysconf_arch __P((void));
int	hpux_to_bsd_uoff __P((int *, int *, struct proc *));
int	hpux_dumpu __P((struct vnode *, struct ucred *));

#endif /* ! _MACHINE_HPUX_MACHDEP_H_ */
