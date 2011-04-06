/*	$OpenBSD: procmap.c,v 1.36 2011/04/06 11:36:26 miod Exp $ */
/*	$NetBSD: pmap.c,v 1.1 2002/09/01 20:32:44 atatat Exp $ */

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/sysctl.h>

#include <uvm/uvm.h>
#include <uvm/uvm_device.h>
#include <uvm/uvm_amap.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#undef doff_t
#undef IN_ACCESS
#undef i_size
#undef i_devvp
#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>

#include <kvm.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

/*
 * stolen (and munged) from #include <uvm/uvm_object.h>
 */
#define UVM_OBJ_IS_VNODE(uobj)	((uobj)->pgops == uvm_vnodeops)
#define UVM_OBJ_IS_AOBJ(uobj)	((uobj)->pgops == aobj_pager)
#define UVM_OBJ_IS_DEVICE(uobj)	((uobj)->pgops == uvm_deviceops)

#define PRINT_VMSPACE		0x00000001
#define PRINT_VM_MAP		0x00000002
#define PRINT_VM_MAP_HEADER	0x00000004
#define PRINT_VM_MAP_ENTRY	0x00000008
#define DUMP_NAMEI_CACHE	0x00000010

struct cache_entry {
	LIST_ENTRY(cache_entry) ce_next;
	struct vnode *ce_vp, *ce_pvp;
	u_long ce_cid, ce_pcid;
	unsigned int ce_nlen;
	char ce_name[256];
};

LIST_HEAD(cache_head, cache_entry) lcache;
void *uvm_vnodeops, *uvm_deviceops, *aobj_pager;
u_long kernel_map_addr;
int debug, verbose;
int print_all, print_map, print_maps, print_solaris, print_ddb, print_amap;
int rwx = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
rlim_t maxssiz;

struct sum {
	unsigned long s_am_nslots;
	unsigned long s_am_maxslots;
	unsigned long s_am_nusedslots;
};

struct kbit {
	/*
	 * size of data chunk
	 */
	size_t k_size;

	/*
	 * something for printf() and something for kvm_read()
	 */
	union {
		void *k_addr_p;
		u_long k_addr_ul;
	} k_addr;

	/*
	 * where we actually put the "stuff"
	 */
	union {
		char data[1];
		struct vmspace vmspace;
		struct vm_map vm_map;
		struct vm_map_entry vm_map_entry;
		struct vnode vnode;
		struct uvm_object uvm_object;
		struct mount mount;
		struct namecache namecache;
		struct inode inode;
		struct iso_node iso_node;
		struct uvm_device uvm_device;
		struct vm_amap vm_amap;
	} k_data;
};

/* the size of the object in the kernel */
#define S(x)	((x)->k_size)
/* the address of the object in kernel, two forms */
#define A(x)	((x)->k_addr.k_addr_ul)
#define P(x)	((x)->k_addr.k_addr_p)
/* the data from the kernel */
#define D(x,d)	(&((x)->k_data.d))

/* suck the data from the kernel */
#define _KDEREF(kd, addr, dst, sz) do { \
	ssize_t len; \
	len = kvm_read((kd), (addr), (dst), (sz)); \
	if (len != (sz)) \
		errx(1, "%s == %ld vs. %lu @ %lx", \
		    kvm_geterr(kd), (long)len, (unsigned long)(sz), (addr)); \
} while (0/*CONSTCOND*/)

/* suck the data using the structure */
#define KDEREF(kd, item) _KDEREF((kd), A(item), D(item, data), S(item))

struct nlist nl[] = {
	{ "_maxsmap" },
#define NL_MAXSSIZ		0
	{ "_uvm_vnodeops" },
#define NL_UVM_VNODEOPS		1
	{ "_uvm_deviceops" },
#define NL_UVM_DEVICEOPS	2
	{ "_aobj_pager" },
#define NL_AOBJ_PAGER		3
	{ "_kernel_map" },
#define NL_KERNEL_MAP		4
	{ NULL }
};

void load_symbols(kvm_t *);
void process_map(kvm_t *, pid_t, struct kinfo_proc2 *, struct sum *);
size_t dump_vm_map_entry(kvm_t *, struct kbit *, struct kbit *, int,
    struct sum *);
char *findname(kvm_t *, struct kbit *, struct kbit *, struct kbit *,
	    struct kbit *, struct kbit *);
int search_cache(kvm_t *, struct kbit *, char **, char *, size_t);
#if 0
void load_name_cache(kvm_t *);
void cache_enter(struct namecache *);
#endif
static void __dead usage(void);
static pid_t strtopid(const char *);
void print_sum(struct sum *, struct sum *);

int
main(int argc, char *argv[])
{
	char errbuf[_POSIX2_LINE_MAX], *kmem = NULL, *kernel = NULL;
	struct kinfo_proc2 *kproc;
	struct sum total_sum;
	int many, ch, rc;
	kvm_t *kd;
	pid_t pid = -1;
	gid_t gid;

	while ((ch = getopt(argc, argv, "AaD:dlmM:N:p:Prsvx")) != -1) {
		switch (ch) {
		case 'A':
			print_amap = 1;
			break;
		case 'a':
			print_all = 1;
			break;
		case 'd':
			print_ddb = 1;
			break;
		case 'D':
			debug = atoi(optarg);
			break;
		case 'l':
			print_maps = 1;
			break;
		case 'm':
			print_map = 1;
			break;
		case 'M':
			kmem = optarg;
			break;
		case 'N':
			kernel = optarg;
			break;
		case 'p':
			pid = strtopid(optarg);
			break;
		case 'P':
			pid = getpid();
			break;
		case 's':
			print_solaris = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'r':
		case 'x':
			errx(1, "-%c option not implemented, sorry", ch);
			/*NOTREACHED*/
		default:
			usage();
		}
	}

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	gid = getgid();
	if (kernel != NULL || kmem != NULL)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

	argc -= optind;
	argv += optind;

	/* more than one "process" to dump? */
	many = (argc > 1 - (pid == -1 ? 0 : 1)) ? 1 : 0;

	/* apply default */
	if (print_all + print_map + print_maps + print_solaris +
	    print_ddb == 0)
		print_solaris = 1;

	/* start by opening libkvm */
	kd = kvm_openfiles(kernel, kmem, NULL, O_RDONLY, errbuf);

	if (kernel == NULL && kmem == NULL)
		if (setresgid(gid, gid, gid) == -1)
			err(1, "setresgid");

	if (kd == NULL)
		errx(1, "%s", errbuf);

	/* get "bootstrap" addresses from kernel */
	load_symbols(kd);

	memset(&total_sum, 0, sizeof(total_sum));

	do {
		struct sum sum;

		memset(&sum, 0, sizeof(sum));

		if (pid == -1) {
			if (argc == 0)
				pid = getppid();
			else {
				pid = strtopid(argv[0]);
				argv++;
				argc--;
			}
		}

		/* find the process id */
		if (pid == 0)
			kproc = NULL;
		else {
			kproc = kvm_getproc2(kd, KERN_PROC_PID, pid,
			    sizeof(struct kinfo_proc2), &rc);
			if (kproc == NULL || rc == 0) {
				errno = ESRCH;
				warn("%d", pid);
				pid = -1;
				continue;
			}
		}

		/* dump it */
		if (many) {
			if (kproc)
				printf("process %d:\n", pid);
			else
				printf("kernel:\n");
		}

		process_map(kd, pid, kproc, &sum);
		if (print_amap)
			print_sum(&sum, &total_sum);
		pid = -1;
	} while (argc > 0);

	if (print_amap)
		print_sum(&total_sum, NULL);

	/* done.  go away. */
	rc = kvm_close(kd);
	if (rc == -1)
		err(1, "kvm_close");

	return (0);
}

void
print_sum(struct sum *sum, struct sum *total_sum)
{
	const char *t = total_sum == NULL ? "total " : "";
	printf("%samap allocated slots: %lu\n", t, sum->s_am_maxslots);
	printf("%samap mapped slots: %lu\n", t, sum->s_am_nslots);
	printf("%samap used slots: %lu\n", t, sum->s_am_nusedslots);

	if (total_sum) {
		total_sum->s_am_maxslots += sum->s_am_maxslots;
		total_sum->s_am_nslots += sum->s_am_nslots;
		total_sum->s_am_nusedslots += sum->s_am_nusedslots;
	}
}

void
process_map(kvm_t *kd, pid_t pid, struct kinfo_proc2 *proc, struct sum *sum)
{
	struct kbit kbit[4], *vmspace, *vm_map, *header, *vm_map_entry;
	struct vm_map_entry *last;
	u_long addr, next;
	size_t total = 0;
	char *thing;
	uid_t uid;

	if ((uid = getuid())) {
		if (pid == 0) {
			warnx("kernel map is restricted");
			return;
		}
		if (uid != proc->p_uid) {
			warnx("other users' process maps are restricted");
			return;
		}
	}

	vmspace = &kbit[0];
	vm_map = &kbit[1];
	header = &kbit[2];
	vm_map_entry = &kbit[3];

	A(vmspace) = 0;
	A(vm_map) = 0;
	A(header) = 0;
	A(vm_map_entry) = 0;

	if (pid > 0) {
		A(vmspace) = (u_long)proc->p_vmspace;
		S(vmspace) = sizeof(struct vmspace);
		KDEREF(kd, vmspace);
		thing = "proc->p_vmspace.vm_map";
	} else {
		A(vmspace) = 0;
		S(vmspace) = 0;
		thing = "kernel_map";
	}

	if (pid > 0 && (debug & PRINT_VMSPACE)) {
		printf("proc->p_vmspace %p = {", P(vmspace));
		printf(" vm_refcnt = %d,", D(vmspace, vmspace)->vm_refcnt);
		printf(" vm_shm = %p,\n", D(vmspace, vmspace)->vm_shm);
		printf("    vm_rssize = %d,", D(vmspace, vmspace)->vm_rssize);
		printf(" vm_swrss = %d,", D(vmspace, vmspace)->vm_swrss);
		printf(" vm_tsize = %d,", D(vmspace, vmspace)->vm_tsize);
		printf(" vm_dsize = %d,\n", D(vmspace, vmspace)->vm_dsize);
		printf("    vm_ssize = %d,", D(vmspace, vmspace)->vm_ssize);
		printf(" vm_taddr = %p,", D(vmspace, vmspace)->vm_taddr);
		printf(" vm_daddr = %p,\n", D(vmspace, vmspace)->vm_daddr);
		printf("    vm_maxsaddr = %p,",
		    D(vmspace, vmspace)->vm_maxsaddr);
		printf(" vm_minsaddr = %p }\n",
		    D(vmspace, vmspace)->vm_minsaddr);
	}

	S(vm_map) = sizeof(struct vm_map);
	if (pid > 0) {
		A(vm_map) = A(vmspace);
		memcpy(D(vm_map, vm_map), &D(vmspace, vmspace)->vm_map,
		    S(vm_map));
	} else {
		A(vm_map) = kernel_map_addr;
		KDEREF(kd, vm_map);
	}
	if (debug & PRINT_VM_MAP) {
		printf("%s %p = {", thing, P(vm_map));

		printf(" pmap = %p,\n", D(vm_map, vm_map)->pmap);
		printf("    lock = <struct lock>,");
		printf(" header = <struct vm_map_entry>,");
		printf(" nentries = %d,\n", D(vm_map, vm_map)->nentries);
		printf("    size = %lx,", D(vm_map, vm_map)->size);
		printf(" ref_count = %d,", D(vm_map, vm_map)->ref_count);
		printf(" ref_lock = <struct simplelock>,\n");
		printf("    hint = %p,", D(vm_map, vm_map)->hint);
		printf(" hint_lock = <struct simplelock>,\n");
		printf("    first_free = %p,", D(vm_map, vm_map)->first_free);
		printf(" flags = %x <%s%s%s%s%s%s >,\n", D(vm_map, vm_map)->flags,
		    D(vm_map, vm_map)->flags & VM_MAP_PAGEABLE ? " PAGEABLE" : "",
		    D(vm_map, vm_map)->flags & VM_MAP_INTRSAFE ? " INTRSAFE" : "",
		    D(vm_map, vm_map)->flags & VM_MAP_WIREFUTURE ? " WIREFUTURE" : "",
		    D(vm_map, vm_map)->flags & VM_MAP_BUSY ? " BUSY" : "",
		    D(vm_map, vm_map)->flags & VM_MAP_WANTLOCK ? " WANTLOCK" : "",
#if VM_MAP_TOPDOWN > 0
		    D(vm_map, vm_map)->flags & VM_MAP_TOPDOWN ? " TOPDOWN" :
#endif
		    "");
		printf("    flags_lock = <struct simplelock>,");
		printf(" timestamp = %u }\n", D(vm_map, vm_map)->timestamp);
	}
	if (print_ddb) {
		printf("MAP %p: [0x%lx->0x%lx]\n", P(vm_map),
		    D(vm_map, vm_map)->min_offset,
		    D(vm_map, vm_map)->max_offset);
		printf("\t#ent=%d, sz=%ld, ref=%d, version=%d, flags=0x%x\n",
		    D(vm_map, vm_map)->nentries,
		    D(vm_map, vm_map)->size,
		    D(vm_map, vm_map)->ref_count,
		    D(vm_map, vm_map)->timestamp,
		    D(vm_map, vm_map)->flags);
		printf("\tpmap=%p(resident=<unknown>)\n",
		    D(vm_map, vm_map)->pmap);
	}

	A(header) = A(vm_map) + offsetof(struct vm_map, header);
	S(header) = sizeof(struct vm_map_entry);
	memcpy(D(header, vm_map_entry), &D(vm_map, vm_map)->header, S(header));
	dump_vm_map_entry(kd, vmspace, header, 1, sum);

	/* headers */
#ifdef DISABLED_HEADERS
	if (print_map)
		printf("%-*s %-*s rwx RWX CPY NCP I W A\n",
		    (int)sizeof(long) * 2 + 2, "Start",
		    (int)sizeof(long) * 2 + 2, "End");
	if (print_maps)
		printf("%-*s %-*s rwxp %-*s Dev   Inode      File\n",
		    (int)sizeof(long) * 2 + 0, "Start",
		    (int)sizeof(long) * 2 + 0, "End",
		    (int)sizeof(long) * 2 + 0, "Offset");
	if (print_solaris)
		printf("%-*s %*s Protection        File\n",
		    (int)sizeof(long) * 2 + 0, "Start",
		    (int)sizeof(int) * 2 - 1,  "Size ");
#endif
	if (print_all)
		printf("%-*s %-*s %*s %-*s rwxpc  RWX  I/W/A Dev  %*s - File\n",
		    (int)sizeof(long) * 2, "Start",
		    (int)sizeof(long) * 2, "End",
		    (int)sizeof(int)  * 2, "Size ",
		    (int)sizeof(long) * 2, "Offset",
		    (int)sizeof(int)  * 2, "Inode");

	/* these are the "sub entries" */
	next = (u_long)D(header, vm_map_entry)->next;
	D(vm_map_entry, vm_map_entry)->next =
	    D(header, vm_map_entry)->next + 1;
	last = P(header);

	while (next != 0 && D(vm_map_entry, vm_map_entry)->next != last) {
		addr = next;
		A(vm_map_entry) = addr;
		S(vm_map_entry) = sizeof(struct vm_map_entry);
		KDEREF(kd, vm_map_entry);
		total += dump_vm_map_entry(kd, vmspace, vm_map_entry, 0, sum);
		next = (u_long)D(vm_map_entry, vm_map_entry)->next;
	}
	if (print_solaris)
		printf("%-*s %8luK\n",
		    (int)sizeof(void *) * 2 - 2, " total",
		    (unsigned long)total);
	if (print_all)
		printf("%-*s %9luk\n",
		    (int)sizeof(void *) * 4 - 1, " total",
		    (unsigned long)total);
}

void
load_symbols(kvm_t *kd)
{
	int rc, i;

	rc = kvm_nlist(kd, &nl[0]);
	if (rc == -1)
		errx(1, "%s == %d", kvm_geterr(kd), rc);
	for (i = 0; i < sizeof(nl)/sizeof(nl[0]); i++)
		if (nl[i].n_value == 0 && nl[i].n_name)
			printf("%s not found\n", nl[i].n_name);

	uvm_vnodeops =	(void*)nl[NL_UVM_VNODEOPS].n_value;
	uvm_deviceops =	(void*)nl[NL_UVM_DEVICEOPS].n_value;
	aobj_pager =	(void*)nl[NL_AOBJ_PAGER].n_value;

	_KDEREF(kd, nl[NL_MAXSSIZ].n_value, &maxssiz,
	    sizeof(maxssiz));
	_KDEREF(kd, nl[NL_KERNEL_MAP].n_value, &kernel_map_addr,
	    sizeof(kernel_map_addr));
}

size_t
dump_vm_map_entry(kvm_t *kd, struct kbit *vmspace,
    struct kbit *vm_map_entry, int ishead, struct sum *sum)
{
	struct kbit kbit[4], *uvm_obj, *vp, *vfs, *amap;
	struct vm_map_entry *vme;
	ino_t inode = 0;
	dev_t dev = 0;
	size_t sz = 0;
	char *name;

	uvm_obj = &kbit[0];
	vp = &kbit[1];
	vfs = &kbit[2];
	amap = &kbit[3];

	A(uvm_obj) = 0;
	A(vp) = 0;
	A(vfs) = 0;

	vme = D(vm_map_entry, vm_map_entry);

	if ((ishead && (debug & PRINT_VM_MAP_HEADER)) ||
	    (!ishead && (debug & PRINT_VM_MAP_ENTRY))) {
		printf("%s %p = {", ishead ? "vm_map.header" : "vm_map_entry",
		    P(vm_map_entry));
		printf(" prev = %p,", vme->prev);
		printf(" next = %p,\n", vme->next);
		printf("    start = %lx,", vme->start);
		printf(" end = %lx,", vme->end);
		printf(" object.uvm_obj/sub_map = %p,\n", vme->object.uvm_obj);
		printf("    offset = %lx,", (unsigned long)vme->offset);
		printf(" etype = %x <%s%s%s%s%s >,", vme->etype,
		    vme->etype & UVM_ET_OBJ ? " OBJ" : "",
		    vme->etype & UVM_ET_SUBMAP ? " SUBMAP" : "",
		    vme->etype & UVM_ET_COPYONWRITE ? " COW" : "",
		    vme->etype & UVM_ET_NEEDSCOPY ? " NEEDSCOPY" : "",
		    vme->etype & UVM_ET_HOLE ? " HOLE" : "");
		printf(" protection = %x,\n", vme->protection);
		printf("    max_protection = %x,", vme->max_protection);
		printf(" inheritance = %d,", vme->inheritance);
		printf(" wired_count = %d,\n", vme->wired_count);
		printf("    aref = <struct vm_aref>,");
		printf(" advice = %d,", vme->advice);
		printf(" flags = %x <%s%s > }\n", vme->flags,
		    vme->flags & UVM_MAP_STATIC ? " STATIC" : "",
		    vme->flags & UVM_MAP_KMEM ? " KMEM" : "");
	}

	if (ishead)
		return (0);

	A(vp) = 0;
	A(uvm_obj) = 0;

	if (vme->object.uvm_obj != NULL) {
		P(uvm_obj) = vme->object.uvm_obj;
		S(uvm_obj) = sizeof(struct uvm_object);
		KDEREF(kd, uvm_obj);
		if (UVM_ET_ISOBJ(vme) &&
		    UVM_OBJ_IS_VNODE(D(uvm_obj, uvm_object))) {
			P(vp) = P(uvm_obj);
			S(vp) = sizeof(struct vnode);
			KDEREF(kd, vp);
		}
	}

	if (vme->aref.ar_amap != NULL) {
		P(amap) = vme->aref.ar_amap;
		S(amap) = sizeof(struct vm_amap);
		KDEREF(kd, amap);
	}

	A(vfs) = 0;

	if (P(vp) != NULL && D(vp, vnode)->v_mount != NULL) {
		P(vfs) = D(vp, vnode)->v_mount;
		S(vfs) = sizeof(struct mount);
		KDEREF(kd, vfs);
		D(vp, vnode)->v_mount = D(vfs, mount);
	}

	/*
	 * dig out the device number and inode number from certain
	 * file system types.
	 */
#define V_DATA_IS(vp, type, d, i) do { \
	struct kbit data; \
	P(&data) = D(vp, vnode)->v_data; \
	S(&data) = sizeof(*D(&data, type)); \
	KDEREF(kd, &data); \
	dev = D(&data, type)->d; \
	inode = D(&data, type)->i; \
} while (0/*CONSTCOND*/)

	if (A(vp) &&
	    D(vp, vnode)->v_type == VREG &&
	    D(vp, vnode)->v_data != NULL) {
		switch (D(vp, vnode)->v_tag) {
		case VT_UFS:
		case VT_EXT2FS:
			V_DATA_IS(vp, inode, i_dev, i_number);
			break;
		case VT_ISOFS:
			V_DATA_IS(vp, iso_node, i_dev, i_number);
			break;
		case VT_NON:
		case VT_NFS:
		case VT_MFS:
		case VT_MSDOSFS:
		case VT_PROCFS:
		default:
			break;
		}
	}

	name = findname(kd, vmspace, vm_map_entry, vp, vfs, uvm_obj);

	if (print_map) {
		printf("0x%lx 0x%lx %c%c%c %c%c%c %s %s %d %d %d",
		    vme->start, vme->end,
		    (vme->protection & VM_PROT_READ) ? 'r' : '-',
		    (vme->protection & VM_PROT_WRITE) ? 'w' : '-',
		    (vme->protection & VM_PROT_EXECUTE) ? 'x' : '-',
		    (vme->max_protection & VM_PROT_READ) ? 'r' : '-',
		    (vme->max_protection & VM_PROT_WRITE) ? 'w' : '-',
		    (vme->max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
		    (vme->etype & UVM_ET_COPYONWRITE) ? "COW" : "NCOW",
		    (vme->etype & UVM_ET_NEEDSCOPY) ? "NC" : "NNC",
		    vme->inheritance, vme->wired_count,
		    vme->advice);
		if (verbose) {
			if (inode)
				printf(" %d,%d %u",
				    major(dev), minor(dev), inode);
			if (name[0])
				printf(" %s", name);
		}
		printf("\n");
	}

	if (print_maps)
		printf("%0*lx-%0*lx %c%c%c%c %0*lx %02x:%02x %u     %s\n",
		    (int)sizeof(void *) * 2, vme->start,
		    (int)sizeof(void *) * 2, vme->end,
		    (vme->protection & VM_PROT_READ) ? 'r' : '-',
		    (vme->protection & VM_PROT_WRITE) ? 'w' : '-',
		    (vme->protection & VM_PROT_EXECUTE) ? 'x' : '-',
		    (vme->etype & UVM_ET_COPYONWRITE) ? 'p' : 's',
		    (int)sizeof(void *) * 2,
		    (unsigned long)vme->offset,
		    major(dev), minor(dev), inode, inode ? name : "");

	if (print_ddb) {
		printf(" - %p: 0x%lx->0x%lx: obj=%p/0x%lx, amap=%p/%d\n",
		    P(vm_map_entry), vme->start, vme->end,
		    vme->object.uvm_obj, (unsigned long)vme->offset,
		    vme->aref.ar_amap, vme->aref.ar_pageoff);
		printf("\tsubmap=%c, cow=%c, nc=%c, prot(max)=%d/%d, inh=%d, "
		    "wc=%d, adv=%d\n",
		    (vme->etype & UVM_ET_SUBMAP) ? 'T' : 'F',
		    (vme->etype & UVM_ET_COPYONWRITE) ? 'T' : 'F',
		    (vme->etype & UVM_ET_NEEDSCOPY) ? 'T' : 'F',
		    vme->protection, vme->max_protection,
		    vme->inheritance, vme->wired_count, vme->advice);
		if (inode && verbose)
			printf("\t(dev=%d,%d ino=%u [%s] [%p])\n",
			    major(dev), minor(dev), inode, inode ? name : "", P(vp));
		else if (name[0] == ' ' && verbose)
			printf("\t(%s)\n", &name[2]);
	}

	if (print_solaris) {
		char prot[30];

		prot[0] = '\0';
		prot[1] = '\0';
		if (vme->protection & VM_PROT_READ)
			strlcat(prot, "/read", sizeof(prot));
		if (vme->protection & VM_PROT_WRITE)
			strlcat(prot, "/write", sizeof(prot));
		if (vme->protection & VM_PROT_EXECUTE)
			strlcat(prot, "/exec", sizeof(prot));

		sz = (size_t)((vme->end - vme->start) / 1024);
		printf("%0*lX %6luK %-15s   %s\n",
		    (int)sizeof(void *) * 2, (unsigned long)vme->start,
		    (unsigned long)sz, &prot[1], name);
	}

	if (print_all) {
		sz = (size_t)((vme->end - vme->start) / 1024);
		printf("%0*lx-%0*lx %7luk %0*lx %c%c%c%c%c (%c%c%c) %d/%d/%d %02d:%02d %7u - %s",
		    (int)sizeof(void *) * 2, vme->start, (int)sizeof(void *) * 2,
		    vme->end - (vme->start != vme->end ? 1 : 0), (unsigned long)sz,
		    (int)sizeof(void *) * 2, (unsigned long)vme->offset,
		    (vme->protection & VM_PROT_READ) ? 'r' : '-',
		    (vme->protection & VM_PROT_WRITE) ? 'w' : '-',
		    (vme->protection & VM_PROT_EXECUTE) ? 'x' : '-',
		    (vme->etype & UVM_ET_COPYONWRITE) ? 'p' : 's',
		    (vme->etype & UVM_ET_NEEDSCOPY) ? '+' : '-',
		    (vme->max_protection & VM_PROT_READ) ? 'r' : '-',
		    (vme->max_protection & VM_PROT_WRITE) ? 'w' : '-',
		    (vme->max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
		    vme->inheritance, vme->wired_count, vme->advice,
		    major(dev), minor(dev), inode, name);
		if (A(vp))
			printf(" [%p]", P(vp));
		printf("\n");
	}

	if (print_amap && vme->aref.ar_amap) {
		printf(" amap - ref: %d fl: 0x%x maxsl: %d nsl: %d nuse: %d\n",
		    D(amap, vm_amap)->am_ref,
		    D(amap, vm_amap)->am_flags,
		    D(amap, vm_amap)->am_maxslot,
		    D(amap, vm_amap)->am_nslot,
		    D(amap, vm_amap)->am_nused);
		if (sum) {
			sum->s_am_nslots += D(amap, vm_amap)->am_nslot;
			sum->s_am_maxslots += D(amap, vm_amap)->am_maxslot;
			sum->s_am_nusedslots += D(amap, vm_amap)->am_nused;
		}
	}

	/* no access allowed, don't count space */
	if ((vme->protection & rwx) == 0)
		sz = 0;

	return (sz);
}

char *
findname(kvm_t *kd, struct kbit *vmspace,
    struct kbit *vm_map_entry, struct kbit *vp,
    struct kbit *vfs, struct kbit *uvm_obj)
{
	static char buf[1024], *name;
	struct vm_map_entry *vme;
	size_t l;

	vme = D(vm_map_entry, vm_map_entry);

	if (UVM_ET_ISOBJ(vme)) {
		if (A(vfs)) {
			l = strlen(D(vfs, mount)->mnt_stat.f_mntonname);
			switch (search_cache(kd, vp, &name, buf, sizeof(buf))) {
			case 0: /* found something */
				if (name - (1 + 11 + l) < buf)
					break;
				name--;
				*name = '/';
				/*FALLTHROUGH*/
			case 2: /* found nothing */
				name -= 11;
				memcpy(name, " -unknown- ", (size_t)11);
				name -= l;
				memcpy(name,
				    D(vfs, mount)->mnt_stat.f_mntonname, l);
				break;
			case 1: /* all is well */
				if (name - (1 + l) < buf)
					break;
				name--;
				*name = '/';
				if (l != 1) {
					name -= l;
					memcpy(name,
					    D(vfs, mount)->mnt_stat.f_mntonname, l);
				}
				break;
			}
		} else if (UVM_OBJ_IS_DEVICE(D(uvm_obj, uvm_object))) {
			struct kbit kdev;
			dev_t dev;

			P(&kdev) = P(uvm_obj);
			S(&kdev) = sizeof(struct uvm_device);
			KDEREF(kd, &kdev);
			dev = D(&kdev, uvm_device)->u_device;
			name = devname(dev, S_IFCHR);
			if (name != NULL)
				snprintf(buf, sizeof(buf), "/dev/%s", name);
			else
				snprintf(buf, sizeof(buf), "  [ device %d,%d ]",
				    major(dev), minor(dev));
			name = buf;
		} else if (UVM_OBJ_IS_AOBJ(D(uvm_obj, uvm_object)))
			name = "  [ uvm_aobj ]";
		else if (UVM_OBJ_IS_VNODE(D(uvm_obj, uvm_object)))
			name = "  [ ?VNODE? ]";
		else {
			snprintf(buf, sizeof(buf), "  [ unknown (%p) ]",
			    D(uvm_obj, uvm_object)->pgops);
			name = buf;
		}
	} else if (D(vmspace, vmspace)->vm_maxsaddr <= (caddr_t)vme->start &&
	    (D(vmspace, vmspace)->vm_maxsaddr + (size_t)maxssiz) >=
	    (caddr_t)vme->end) {
		name = "  [ stack ]";
	} else if (D(vmspace, vmspace)->vm_daddr <= (caddr_t)vme->start &&
	    D(vmspace, vmspace)->vm_daddr + MAXDSIZ >= (caddr_t)vme->end &&
	    D(vmspace, vmspace)->vm_dsize * getpagesize() / 2 <
	    (vme->end - vme->start)) {
		name = "  [ heap ]";
	} else if (UVM_ET_ISHOLE(vme))
		name = "  [ hole ]";
	else
		name = "  [ anon ]";

	return (name);
}

int
search_cache(kvm_t *kd, struct kbit *vp, char **name, char *buf, size_t blen)
{
	struct cache_entry *ce;
	struct kbit svp;
	char *o, *e;
	u_long cid;

#if 0
	if (nchashtbl == NULL)
		load_name_cache(kd);
#endif

	P(&svp) = P(vp);
	S(&svp) = sizeof(struct vnode);
	cid = D(vp, vnode)->v_id;

	e = &buf[blen - 1];
	o = e;
	do {
		LIST_FOREACH(ce, &lcache, ce_next)
			if (ce->ce_vp == P(&svp) && ce->ce_cid == cid)
				break;
		if (ce && ce->ce_vp == P(&svp) && ce->ce_cid == cid) {
			if (o != e)
				*(--o) = '/';
			if (o - ce->ce_nlen <= buf)
				break;
			o -= ce->ce_nlen;
			memcpy(o, ce->ce_name, ce->ce_nlen);
			P(&svp) = ce->ce_pvp;
			cid = ce->ce_pcid;
		} else
			break;
	} while (1/*CONSTCOND*/);
	*e = '\0';
	*name = o;

	if (e == o)
		return (2);

	KDEREF(kd, &svp);
	return (D(&svp, vnode)->v_flag & VROOT);
}

#if 0
void
load_name_cache(kvm_t *kd)
{
	struct namecache _ncp, *ncp, *oncp;
	struct nchashhead _ncpp, *ncpp;
	u_long nchash;
	int i;

	LIST_INIT(&lcache);

	_KDEREF(kd, nchash_addr, &nchash, sizeof(nchash));
	nchashtbl = calloc(sizeof(nchashtbl), (int)nchash);
	if (nchashtbl == NULL)
		err(1, "load_name_cache");
	_KDEREF(kd, nchashtbl_addr, nchashtbl,
	    sizeof(nchashtbl) * (int)nchash);

	ncpp = &_ncpp;

	for (i = 0; i < nchash; i++) {
		ncpp = &nchashtbl[i];
		oncp = NULL;
		LIST_FOREACH(ncp, ncpp, nc_hash) {
			if (ncp == oncp ||
			    ncp == (void*)0xdeadbeef)
				break;
			oncp = ncp;
			_KDEREF(kd, (u_long)ncp, &_ncp, sizeof(*ncp));
			ncp = &_ncp;
			if (ncp->nc_nlen > 0) {
				if (ncp->nc_nlen > 2 ||
				    ncp->nc_name[0] != '.' ||
				    (ncp->nc_name[1] != '.' &&
				    ncp->nc_nlen != 1))
					cache_enter(ncp);
			}
		}
	}
}

void
cache_enter(struct namecache *ncp)
{
	struct cache_entry *ce;

	if (debug & DUMP_NAMEI_CACHE)
		printf("ncp->nc_vp %10p, ncp->nc_dvp %10p, ncp->nc_nlen "
		    "%3d [%.*s] (nc_dvpid=%lu, nc_vpid=%lu)\n",
		    ncp->nc_vp, ncp->nc_dvp,
		    ncp->nc_nlen, ncp->nc_nlen, ncp->nc_name,
		    ncp->nc_dvpid, ncp->nc_vpid);

	ce = malloc(sizeof(struct cache_entry));
	if (ce == NULL)
		err(1, "cache_enter");

	ce->ce_vp = ncp->nc_vp;
	ce->ce_pvp = ncp->nc_dvp;
	ce->ce_cid = ncp->nc_vpid;
	ce->ce_pcid = ncp->nc_dvpid;
	ce->ce_nlen = (unsigned)ncp->nc_nlen;
	strlcpy(ce->ce_name, ncp->nc_name, sizeof(ce->ce_name));

	LIST_INSERT_HEAD(&lcache, ce, ce_next);
}
#endif

static void __dead
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-AadlmPsv] [-D number] "
	    "[-M core] [-N system] [-p pid] [pid ...]\n",
	    __progname);
	exit(1);
}

static pid_t
strtopid(const char *str)
{
	pid_t pid;

	errno = 0;
	pid = (pid_t)strtonum(str, 0, INT_MAX, NULL);
	if (errno != 0)
		usage();
	return (pid);
}
