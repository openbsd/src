/*	$OpenBSD: pfctl_radix.c,v 1.4 2003/01/03 22:31:15 cedric Exp $ */

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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "pfctl_radix.h"

static int pfr_dev = -1;

static int
pfr_ioctl(unsigned long op, void *buf)
{
	if (pfr_dev < 0)
		return (pfr_dev);
	return (ioctl(pfr_dev, op, buf));
}

void
pfr_set_fd(int fd)
{
	pfr_dev = fd;
}

int
pfr_get_fd(void)
{
	return pfr_dev;
}

int
pfr_clr_tables(int *ndel, int flags)
{
	struct pfioc_table io;

	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	if (pfr_ioctl(DIOCRCLRTABLES, &io))
		return (-1);
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	return (0);
}

int
pfr_add_tables(struct pfr_table *tbl, int size, int *nadd, int flags)
{
	struct pfioc_table io;

	if (size < 0 || (size && tbl == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_buffer = tbl;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRADDTABLES, &io))
		return (-1);
	if (nadd != NULL)
		*nadd = io.pfrio_nadd;
	return (0);
}

int
pfr_del_tables(struct pfr_table *tbl, int size, int *ndel, int flags)
{
	struct pfioc_table io;

	if (size < 0 || (size && tbl == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_buffer = tbl;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRDELTABLES, &io))
		return (-1);
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	return (0);
}

int
pfr_get_tables(struct pfr_table *tbl, int *size, int flags)
{
	struct pfioc_table io;

	if (size == NULL || *size < 0 || (*size && tbl == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_buffer = tbl;
	io.pfrio_size = *size;
	if (pfr_ioctl(DIOCRGETTABLES, &io))
		return (-1);
	*size = io.pfrio_size;
	return (0);
}

int
pfr_get_tstats(struct pfr_tstats *tbl, int *size, int flags)
{
	struct pfioc_table io;

	if (size == NULL || *size < 0 || (*size && tbl == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_buffer = tbl;
	io.pfrio_size = *size;
	if (pfr_ioctl(DIOCRGETTSTATS, &io))
		return (-1);
	*size = io.pfrio_size;
	return (0);
}

int
pfr_clr_addrs(struct pfr_table *tbl, int *ndel, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	if (pfr_ioctl(DIOCRSETADDRS, &io))
		return (-1);
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	return (0);
}

int
pfr_add_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *nadd, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRADDADDRS, &io))
		return (-1);
	if (nadd != NULL)
		*nadd = io.pfrio_nadd;
	return (0);
}

int
pfr_del_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *ndel, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRDELADDRS, &io))
		return (-1);
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	return (0);
}

int
pfr_set_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *size2, int *nadd, int *ndel, int *nchange, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = size;
	io.pfrio_size2 = (size2 != NULL) ? *size2 : 0;
	if (pfr_ioctl(DIOCRSETADDRS, &io))
		return (-1);
	if (nadd != NULL)
		*nadd = io.pfrio_nadd;
	if (ndel != NULL)
		*ndel = io.pfrio_ndel;
	if (nchange != NULL)
		*nchange = io.pfrio_nchange;
	if (size2 != NULL)
		*size2 = io.pfrio_size2;
	return (0);
}

int
pfr_get_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int *size,
	int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size == NULL || *size < 0 || (*size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = *size;
	if (pfr_ioctl(DIOCRGETADDRS, &io))
		return (-1);
	*size = io.pfrio_size;
	return (0);
}

int
pfr_get_astats(struct pfr_table *tbl, struct pfr_astats *addr, int *size,
	int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size == NULL || *size < 0 || (*size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = *size;
	if (pfr_ioctl(DIOCRGETASTATS, &io))
		return (-1);
	*size = io.pfrio_size;
	return (0);
}

int
pfr_clr_astats(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *nzero, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRCLRTSTATS, &io))
		return (-1);
	if (nzero != NULL)
		*nzero = io.pfrio_nzero;
	return (0);
}

int
pfr_clr_tstats(struct pfr_table *tbl, int size, int *nzero, int flags)
{
	struct pfioc_table io;

	if (size < 0 || (size && !tbl)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_buffer = tbl;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRCLRTSTATS, &io))
		return (-1);
	if (nzero)
		*nzero = io.pfrio_nzero;
	return (0);
}


int
pfr_tst_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
	int *nmatch, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL || size < 0 || (size && addr == NULL)) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = addr;
	io.pfrio_size = size;
	if (pfr_ioctl(DIOCRTSTADDRS, &io))
		return (-1);
	if (nmatch)
		*nmatch = io.pfrio_nmatch;
	return (0);
}

int
pfr_wrap_table(struct pfr_table *tbl, struct pf_addr_wrap *wrap,
    int *exists, int flags)
{
	struct pfioc_table io;

	if (tbl == NULL) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_table = *tbl;
	io.pfrio_buffer = wrap;
	io.pfrio_size = wrap ? 1 : 0;
	io.pfrio_exists = exists ? 1 : 0;
	if (pfr_ioctl(DIOCRWRAPTABLE, &io))
		return (-1);
	if (exists)
		*exists = io.pfrio_exists;
	return (0);
}

int
pfr_unwrap_table(struct pfr_table *tbl, struct pf_addr_wrap *wrap, int flags)
{
	struct pfioc_table io;

	if (wrap == NULL) {
		errno = EINVAL;
		return -1;
	}
	bzero(&io, sizeof io);
	io.pfrio_flags = flags;
	io.pfrio_buffer = wrap;
	io.pfrio_size = 1;
	if (pfr_ioctl(DIOCRUNWRTABLE, &io))
		return (-1);
	if (tbl != NULL)
		*tbl = io.pfrio_table;
	return (0);
}
