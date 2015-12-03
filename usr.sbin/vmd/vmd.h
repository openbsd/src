/*	$OpenBSD: vmd.h,v 1.9 2015/12/03 16:11:32 reyk Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <machine/vmmvar.h>

#include <limits.h>

#include "proc.h"

#ifndef VMD_H
#define VMD_H

#define VMD_USER		"_vmd"
#define VMD_CONF		"/etc/vm.conf"
#define SOCKET_NAME		"/var/run/vmd.sock"
#define VMM_NODE		"/dev/vmm"
#define VM_NAME_MAX		64
#define VM_TTYNAME_MAX		32
#define MAX_TAP			256
#define NR_BACKLOG		5

/* #define VMD_DEBUG */

#ifdef VMD_DEBUG
#define dprintf(x...)   do { log_debug(x); } while(0)
#else
#define dprintf(x...)
#endif /* VMM_DEBUG */

enum imsg_type {
	IMSG_VMDOP_DISABLE_VMM_REQUEST = IMSG_PROC_MAX,
	IMSG_VMDOP_DISABLE_VMM_RESPONSE,
	IMSG_VMDOP_ENABLE_VMM_REQUEST,
	IMSG_VMDOP_ENABLE_VMM_RESPONSE,
	IMSG_VMDOP_START_VM_REQUEST,
	IMSG_VMDOP_START_VM_DISK,
	IMSG_VMDOP_START_VM_IF,
	IMSG_VMDOP_START_VM_END,
	IMSG_VMDOP_START_VM_RESPONSE,
	IMSG_VMDOP_TERMINATE_VM_REQUEST,
	IMSG_VMDOP_TERMINATE_VM_RESPONSE,
	IMSG_VMDOP_GET_INFO_VM_REQUEST,
	IMSG_VMDOP_GET_INFO_VM_DATA,
	IMSG_VMDOP_GET_INFO_VM_END_DATA
};

struct vmop_start_result {
	int	 vmr_result;
	char	 vmr_ttyname[VM_TTYNAME_MAX];
};

struct vmd_vm {
	struct vm_create_params	vm_params;
	uint32_t		vm_vmid;
	int			vm_kernel;
	int			vm_disks[VMM_MAX_DISKS_PER_VM];
	int			vm_ifs[VMM_MAX_NICS_PER_VM];
	char			vm_ttyname[VM_TTYNAME_MAX];
	int			vm_tty;
	uint32_t		vm_peerid;
	TAILQ_ENTRY(vmd_vm)	vm_entry;
};
TAILQ_HEAD(vmlist, vmd_vm);

struct vmd {
	struct privsep		 vmd_ps;
	const char		*vmd_conffile;

	int			 vmd_debug;
	int			 vmd_verbose;
	int			 vmd_noaction;
	int			 vmd_vmcount;

	uint32_t		 vmd_nvm;
	struct vmlist		*vmd_vms;

	int			 vmd_fd;
};

/* vmd.c */
struct vmd_vm *vm_getbyvmid(uint32_t);
void	 vm_remove(struct vmd_vm *);

/* vmm.c */
pid_t	 vmm(struct privsep *, struct privsep_proc *);
int	 write_page(uint32_t dst, void *buf, uint32_t, int);
int	 read_page(uint32_t dst, void *buf, uint32_t, int);
int	 opentap(void);

/* control.c */
int	 config_init(struct vmd *);
void	 config_purge(struct vmd *, unsigned int);
int	 config_setreset(struct vmd *, unsigned int);
int	 config_getreset(struct vmd *, struct imsg *);
int	 config_getvm(struct privsep *, struct vm_create_params *,
	    int, uint32_t);
int	 config_getdisk(struct privsep *, struct imsg *);
int	 config_getif(struct privsep *, struct imsg *);

/* parse.y */
int	 parse_config(const char *);
int	 cmdline_symset(char *);

#endif /* VMD_H */
