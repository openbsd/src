/*	$OpenBSD: pfctl_radix.h,v 1.1 2003/01/03 21:37:44 cedric Exp $ */

/*
 * Copyright (c) 2002 Cedric Berger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _PFCTL_RADIX_H_
#define _PFCTL_RADIX_H_

#include <net/pfvar.h>

void	 pfr_set_fd(int);
int	 pfr_get_fd(void);
int	 pfr_clr_tables(int *ndel, int);
int	 pfr_add_tables(struct pfr_table *, int, int *, int);
int	 pfr_del_tables(struct pfr_table *, int, int *, int);
int	 pfr_get_tables(struct pfr_table *, int *, int);
int	 pfr_get_tstats(struct pfr_tstats *, int *, int);
int	 pfr_clr_tstats(struct pfr_table *, int, int *, int);
int	 pfr_clr_addrs(struct pfr_table *, int *, int);
int	 pfr_add_addrs(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_del_addrs(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_set_addrs(struct pfr_table *, struct pfr_addr *, int, int *,
	     int *, int *, int *, int);
int	 pfr_get_addrs(struct pfr_table *, struct pfr_addr *, int *, int);
int	 pfr_get_astats(struct pfr_table *, struct pfr_astats *, int *, int);
int	 pfr_clr_astats(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_tst_addrs(struct pfr_table *, struct pfr_addr *, int, int *, int);
int	 pfr_wrap_table(struct pfr_table *, struct pf_addr_wrap *, int *, int);
int	 pfr_unwrap_table(struct pfr_table *, struct pf_addr_wrap *, int);

#endif /* _PFCTL_RADIX_H_ */
