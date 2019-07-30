/*	$OpenBSD: vmd.h,v 1.67 2018/01/03 05:39:56 ccardenas Exp $	*/

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
#include <sys/socket.h>

#include <machine/vmmvar.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <limits.h>
#include <stdio.h>
#include <pthread.h>

#include "proc.h"

#ifndef VMD_H
#define VMD_H

#define SET(_v, _m)		((_v) |= (_m))
#define CLR(_v, _m)		((_v) &= ~(_m))
#define ISSET(_v, _m)		((_v) & (_m))

#define VMD_USER		"_vmd"
#define VMD_CONF		"/etc/vm.conf"
#define SOCKET_NAME		"/var/run/vmd.sock"
#define VMM_NODE		"/dev/vmm"
#define VM_DEFAULT_BIOS		"/etc/firmware/vmm-bios"
#define VM_DEFAULT_KERNEL	"/bsd"
#define VM_DEFAULT_DEVICE	"hd0a"
#define VM_BOOT_CONF		"/etc/boot.conf"
#define VM_NAME_MAX		64
#define VM_TTYNAME_MAX		16
#define MAX_TAP			256
#define NR_BACKLOG		5
#define VMD_SWITCH_TYPE		"bridge"
#define VM_DEFAULT_MEMORY	512

/* vmd -> vmctl error codes */
#define VMD_BIOS_MISSING	1001
#define VMD_DISK_MISSING	1002
#define VMD_DISK_INVALID	1003
#define VMD_VM_STOP_INVALID	1004
#define VMD_CDROM_MISSING	1005
#define VMD_CDROM_INVALID	1006

/* 100.64.0.0/10 from rfc6598 (IPv4 Prefix for Shared Address Space) */
#define VMD_DHCP_PREFIX		"100.64.0.0/10"

#ifdef VMD_DEBUG
#define dprintf(x...)   do { log_debug(x); } while(0)
#else
#define dprintf(x...)
#endif /* VMD_DEBUG */

enum imsg_type {
	IMSG_VMDOP_START_VM_REQUEST = IMSG_PROC_MAX,
	IMSG_VMDOP_START_VM_CDROM,
	IMSG_VMDOP_START_VM_DISK,
	IMSG_VMDOP_START_VM_IF,
	IMSG_VMDOP_START_VM_END,
	IMSG_VMDOP_START_VM_RESPONSE,
	IMSG_VMDOP_PAUSE_VM,
	IMSG_VMDOP_PAUSE_VM_RESPONSE,
	IMSG_VMDOP_UNPAUSE_VM,
	IMSG_VMDOP_UNPAUSE_VM_RESPONSE,
	IMSG_VMDOP_SEND_VM_REQUEST,
	IMSG_VMDOP_SEND_VM_RESPONSE,
	IMSG_VMDOP_RECEIVE_VM_REQUEST,
	IMSG_VMDOP_RECEIVE_VM_RESPONSE,
	IMSG_VMDOP_RECEIVE_VM_END,
	IMSG_VMDOP_TERMINATE_VM_REQUEST,
	IMSG_VMDOP_TERMINATE_VM_RESPONSE,
	IMSG_VMDOP_TERMINATE_VM_EVENT,
	IMSG_VMDOP_GET_INFO_VM_REQUEST,
	IMSG_VMDOP_GET_INFO_VM_DATA,
	IMSG_VMDOP_GET_INFO_VM_END_DATA,
	IMSG_VMDOP_LOAD,
	IMSG_VMDOP_RELOAD,
	IMSG_VMDOP_PRIV_IFDESCR,
	IMSG_VMDOP_PRIV_IFADD,
	IMSG_VMDOP_PRIV_IFEXISTS,
	IMSG_VMDOP_PRIV_IFUP,
	IMSG_VMDOP_PRIV_IFDOWN,
	IMSG_VMDOP_PRIV_IFGROUP,
	IMSG_VMDOP_PRIV_IFADDR,
	IMSG_VMDOP_PRIV_IFRDOMAIN,
	IMSG_VMDOP_VM_SHUTDOWN,
	IMSG_VMDOP_VM_REBOOT,
	IMSG_VMDOP_CONFIG
};

struct vmop_result {
	int			 vmr_result;
	uint32_t		 vmr_id;
	pid_t			 vmr_pid;
	char			 vmr_ttyname[VM_TTYNAME_MAX];
};

struct vmop_info_result {
	struct vm_info_result	 vir_info;
	char			 vir_ttyname[VM_TTYNAME_MAX];
	uid_t			 vir_uid;
	int64_t			 vir_gid;
};

struct vmop_id {
	uint32_t		 vid_id;
	char			 vid_name[VMM_MAX_NAME_LEN];
	uid_t			 vid_uid;
};

struct vmop_ifreq {
	uint32_t		 vfr_id;
	char			 vfr_name[IF_NAMESIZE];
	char			 vfr_value[VM_NAME_MAX];
	struct ifaliasreq	 vfr_ifra;
};

struct vmop_create_params {
	struct vm_create_params	 vmc_params;
	unsigned int		 vmc_flags;
#define VMOP_CREATE_KERNEL	0x01
#define VMOP_CREATE_MEMORY	0x02
#define VMOP_CREATE_NETWORK	0x04
#define VMOP_CREATE_DISK	0x08
#define VMOP_CREATE_CDROM	0x10

	/* userland-only part of the create params */
	unsigned int		 vmc_ifflags[VMM_MAX_NICS_PER_VM];
#define VMIFF_UP		0x01
#define VMIFF_LOCKED		0x02
#define VMIFF_LOCAL		0x04
#define VMIFF_RDOMAIN		0x08
#define VMIFF_OPTMASK		(VMIFF_LOCKED|VMIFF_LOCAL|VMIFF_RDOMAIN)
	char			 vmc_ifnames[VMM_MAX_NICS_PER_VM][IF_NAMESIZE];
	char			 vmc_ifswitch[VMM_MAX_NICS_PER_VM][VM_NAME_MAX];
	char			 vmc_ifgroup[VMM_MAX_NICS_PER_VM][IF_NAMESIZE];
	unsigned int		 vmc_ifrdomain[VMM_MAX_NICS_PER_VM];
	uid_t			 vmc_uid;
	int64_t			 vmc_gid;
};

struct vm_dump_header_cpuid {
	unsigned long code, leaf;
	unsigned int a, b, c, d;
};

#define VM_DUMP_HEADER_CPUID_COUNT	5

struct vm_dump_header {
	uint8_t			 vmh_signature[12];
#define VM_DUMP_SIGNATURE	 VMM_HV_SIGNATURE
	uint8_t			 vmh_pad[3];
	uint8_t			 vmh_version;
#define VM_DUMP_VERSION		 2
	struct			 vm_dump_header_cpuid
	    vmh_cpuids[VM_DUMP_HEADER_CPUID_COUNT];
} __packed;

struct vmboot_params {
	int			 vbp_fd;
	off_t			 vbp_partoff;
	char			 vbp_device[PATH_MAX];
	char			 vbp_image[PATH_MAX];
	uint32_t		 vbp_bootdev;
	uint32_t		 vbp_howto;
	char			*vbp_arg;
};

struct vmd_if {
	char			*vif_name;
	char			*vif_switch;
	char			*vif_group;
	int			 vif_fd;
	unsigned int		 vif_rdomain;
	unsigned int		 vif_flags;
	TAILQ_ENTRY(vmd_if)	 vif_entry;
};

struct vmd_switch {
	uint32_t		 sw_id;
	char			*sw_name;
	char			 sw_ifname[IF_NAMESIZE];
	char			*sw_group;
	unsigned int		 sw_rdomain;
	unsigned int		 sw_flags;
	int			 sw_running;
	TAILQ_ENTRY(vmd_switch)	 sw_entry;
};
TAILQ_HEAD(switchlist, vmd_switch);

struct vmd_vm {
	struct vmop_create_params vm_params;
	pid_t			 vm_pid;
	uint32_t		 vm_vmid;
	int			 vm_kernel;
	int			 vm_cdrom;
	int			 vm_disks[VMM_MAX_DISKS_PER_VM];
	struct vmd_if		 vm_ifs[VMM_MAX_NICS_PER_VM];
	char			*vm_ttyname;
	int			 vm_tty;
	uint32_t		 vm_peerid;
	/* When set, VM is running now (PROC_PARENT only) */
	int			 vm_running;
	/* When set, VM is not started by default (PROC_PARENT only) */
	int			 vm_disabled;
	/* When set, VM was defined in a config file */
	int			 vm_from_config;
	struct imsgev		 vm_iev;
	int			 vm_shutdown;
	uid_t			 vm_uid;
	int			 vm_received;
	int			 vm_paused;
	int			 vm_receive_fd;

	TAILQ_ENTRY(vmd_vm)	 vm_entry;
};
TAILQ_HEAD(vmlist, vmd_vm);

struct address {
	struct sockaddr_storage	 ss;
	int			 prefixlen;
	TAILQ_ENTRY(address)	 entry;
};
TAILQ_HEAD(addresslist, address);

struct vmd_config {
	struct address		 cfg_localprefix;
};

struct vmd {
	struct privsep		 vmd_ps;
	const char		*vmd_conffile;

	/* global configuration that is sent to the children */
	struct vmd_config	 vmd_cfg;

	int			 vmd_debug;
	int			 vmd_verbose;
	int			 vmd_noaction;

	uint32_t		 vmd_nvm;
	struct vmlist		*vmd_vms;
	uint32_t		 vmd_nswitches;
	struct switchlist	*vmd_switches;

	int			 vmd_fd;
	int			 vmd_ptmfd;
};

static inline struct sockaddr_in *
ss2sin(struct sockaddr_storage *ss)
{
	return ((struct sockaddr_in *)ss);
}

static inline struct sockaddr_in6 *
ss2sin6(struct sockaddr_storage *ss)
{
	return ((struct sockaddr_in6 *)ss);
}

struct packet_ctx {
	uint8_t			 pc_htype;
	uint8_t			 pc_hlen;
	uint8_t			 pc_smac[ETHER_ADDR_LEN];
	uint8_t			 pc_dmac[ETHER_ADDR_LEN];

	struct sockaddr_storage	 pc_src;
	struct sockaddr_storage	 pc_dst;
};

/* packet.c */
ssize_t	 assemble_hw_header(unsigned char *, size_t, size_t,
	    struct packet_ctx *, unsigned int);
ssize_t	 assemble_udp_ip_header(unsigned char *, size_t, size_t,
	    struct packet_ctx *pc, unsigned char *, size_t);
ssize_t	 decode_hw_header(unsigned char *, size_t, size_t, struct packet_ctx *,
	    unsigned int);
ssize_t	 decode_udp_ip_header(unsigned char *, size_t, size_t,
	    struct packet_ctx *);

/* vmd.c */
int	 vmd_reload(unsigned int, const char *);
struct vmd_vm *vm_getbyid(uint32_t);
struct vmd_vm *vm_getbyvmid(uint32_t);
uint32_t vm_id2vmid(uint32_t, struct vmd_vm *);
uint32_t vm_vmid2id(uint32_t, struct vmd_vm *);
struct vmd_vm *vm_getbyname(const char *);
struct vmd_vm *vm_getbypid(pid_t);
void	 vm_stop(struct vmd_vm *, int);
void	 vm_remove(struct vmd_vm *);
int	 vm_register(struct privsep *, struct vmop_create_params *,
	    struct vmd_vm **, uint32_t, uid_t);
int	 vm_checkperm(struct vmd_vm *, uid_t);
int	 vm_opentty(struct vmd_vm *);
void	 vm_closetty(struct vmd_vm *);
void	 switch_remove(struct vmd_switch *);
struct vmd_switch *switch_getbyname(const char *);
char	*get_string(uint8_t *, size_t);
uint32_t prefixlen2mask(uint8_t);

/* priv.c */
void	 priv(struct privsep *, struct privsep_proc *);
int	 priv_getiftype(char *, char *, unsigned int *);
int	 priv_findname(const char *, const char **);
int	 priv_validgroup(const char *);
int	 vm_priv_ifconfig(struct privsep *, struct vmd_vm *);
int	 vm_priv_brconfig(struct privsep *, struct vmd_switch *);
uint32_t vm_priv_addr(struct address *, uint32_t, int, int);

/* vmm.c */
struct iovec;

void	 vmm(struct privsep *, struct privsep_proc *);
void	 vmm_shutdown(void);
void	*vaddr_mem(paddr_t, size_t);
int	 write_mem(paddr_t, const void *buf, size_t);
int	 read_mem(paddr_t, void *buf, size_t);
int	 iovec_mem(paddr_t, size_t, struct iovec *, int);
int	 opentap(char *);
int	 fd_hasdata(int);
void	 mutex_lock(pthread_mutex_t *);
void	 mutex_unlock(pthread_mutex_t *);
int	 vmm_pipe(struct vmd_vm *, int, void (*)(int, short, void *));

/* vm.c */
int	 start_vm(struct vmd_vm *, int);
int receive_vm(struct vmd_vm *, int, int);
__dead void vm_shutdown(unsigned int);

/* control.c */
int	 config_init(struct vmd *);
void	 config_purge(struct vmd *, unsigned int);
int	 config_setconfig(struct vmd *);
int	 config_getconfig(struct vmd *, struct imsg *);
int	 config_setreset(struct vmd *, unsigned int);
int	 config_getreset(struct vmd *, struct imsg *);
int	 config_setvm(struct privsep *, struct vmd_vm *, uint32_t, uid_t);
int	 config_getvm(struct privsep *, struct imsg *);
int	 config_getdisk(struct privsep *, struct imsg *);
int	 config_getif(struct privsep *, struct imsg *);
int	 config_getcdrom(struct privsep *, struct imsg *);

/* vmboot.c */
FILE	*vmboot_open(int, int, struct vmboot_params *);
void	 vmboot_close(FILE *, struct vmboot_params *);

/* parse.y */
int	 parse_config(const char *);
int	 cmdline_symset(char *);
int	 host(const char *, struct address *);

#endif /* VMD_H */
