/*	$OpenBSD: kvm_file2.c,v 1.15 2010/06/29 16:39:23 guenther Exp $	*/

/*
 * Copyright (c) 2009 Todd C. Miller <Todd.Miller@courtesan.com>
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

/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Extended file list interface for kvm.  pstat, fstat and netstat are
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#define __need_process

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#define _KERNEL
#include <sys/file.h>
#include <sys/mount.h>
#include <dev/systrace.h>
#undef _KERNEL
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/unpcb.h>
#include <sys/filedesc.h>
#include <sys/pipe.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#define _KERNEL
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#undef _KERNEL

#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>

#include <nnpfs/nnpfs_config.h>
#include <nnpfs/nnpfs_node.h>

#include <msdosfs/bpb.h>
#define _KERNEL
#include <msdosfs/denode.h>
#undef _KERNEL
#include <msdosfs/msdosfsmount.h>

#include <miscfs/specfs/specdev.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#include <nlist.h>
#include <kvm.h>
#include <db.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kvm_private.h"

static struct kinfo_file2 *kvm_deadfile2_byfile(kvm_t *, int, int,
    size_t, int *);
static struct kinfo_file2 *kvm_deadfile2_byid(kvm_t *, int, int,
    size_t, int *);
static int fill_file2(kvm_t *, struct kinfo_file2 *, struct file *,
    struct vnode *, struct proc *, int);
static int filestat(kvm_t *, struct kinfo_file2 *, struct vnode *);

LIST_HEAD(proclist, proc);

struct kinfo_file2 *
kvm_getfile2(kvm_t *kd, int op, int arg, size_t esize, int *cnt)
{
	int mib[6], rv;
	size_t size;

	if (kd->filebase != NULL) {
		free(kd->filebase);
		/*
		 * Clear this pointer in case this call fails.  Otherwise,
		 * kvm_close() will free it again.
		 */
		kd->filebase = 0;
	}

	if (ISALIVE(kd)) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_FILE2;
		mib[2] = op;
		mib[3] = arg;
		mib[4] = esize;
		mib[5] = 0;

		/* find size and alloc buffer */
		rv = sysctl(mib, 6, NULL, &size, NULL, 0);
		if (rv == -1) {
			if (kd->vmfd != -1)
				goto deadway;
			_kvm_syserr(kd, kd->program, "kvm_getfile2");
			return (NULL);
		}
		kd->filebase = _kvm_malloc(kd, size);
		if (kd->filebase == NULL)
			return (NULL);

		/* get actual data */
		mib[5] = size / esize;
		rv = sysctl(mib, 6, kd->filebase, &size, NULL, 0);
		if (rv == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getfile2");
			return (NULL);
		}
		*cnt = size / esize;
		return ((struct kinfo_file2 *)kd->filebase);
	} else {
		if (esize > sizeof(struct kinfo_file2)) {
			_kvm_syserr(kd, kd->program,
			    "kvm_getfile2: unknown fields requested: libkvm out of date?");
			return (NULL);
		}
	    deadway:
		switch (op) {
		case KERN_FILE_BYFILE:
			if (arg != 0) {
				_kvm_err(kd, kd->program,
				    "%s: invalid argument");
				return (NULL);
			}
			return (kvm_deadfile2_byfile(kd, op, arg, esize, cnt));
			break;
		case KERN_FILE_BYPID:
		case KERN_FILE_BYUID:
			return (kvm_deadfile2_byid(kd, op, arg, esize, cnt));
			break;
		default:
			return (NULL);
		}
	}
}

static struct kinfo_file2 *
kvm_deadfile2_byfile(kvm_t *kd, int op, int arg, size_t esize, int *cnt)
{
	size_t size;
	struct nlist nl[3], *p;
	int buflen = kd->arglen, n = 0;
	char *where = kd->argspc;
	struct kinfo_file2 *kf = NULL;
	struct file *fp, file;
	struct filelist filehead;
	int nfiles;

	nl[0].n_name = "_filehead";
	nl[1].n_name = "_nfiles";
	nl[2].n_name = 0;

	if (kvm_nlist(kd, nl) != 0) {
		for (p = nl; p->n_type != 0; ++p)
			;
		_kvm_err(kd, kd->program,
			 "%s: no such symbol", p->n_name);
		return (NULL);
	}
	if (KREAD(kd, nl[0].n_value, &filehead)) {
		_kvm_err(kd, kd->program, "can't read filehead");
		return (NULL);
	}
	if (KREAD(kd, nl[1].n_value, &nfiles)) {
		_kvm_err(kd, kd->program, "can't read nfiles");
		return (NULL);
	}
	size = (nfiles + 10) * sizeof(struct kinfo_file2);
	kd->filebase = _kvm_malloc(kd, size);
	if (kd->filebase == NULL)
		return (NULL);

	LIST_FOREACH(fp, &filehead, f_list) {
		if (buflen < sizeof(struct kinfo_file2))
			break;

		if (KREAD(kd, (long)fp, &file)) {
			_kvm_err(kd, kd->program, "can't read kfp");
			return (NULL);
		}
		kf = (struct kinfo_file2 *)where;
		where += sizeof(struct kinfo_file2);
		buflen -= sizeof(struct kinfo_file2);
		n++;
		if (fill_file2(kd, kf, fp, NULL, NULL, 0) == -1)
			return (NULL);
	}
	if (n != nfiles) {
		_kvm_err(kd, kd->program, "inconsistent nfiles");
		return (NULL);
	}
	*cnt = n;
	return (kf);
}

static struct kinfo_file2 *
kvm_deadfile2_byid(kvm_t *kd, int op, int arg, size_t esize, int *cnt)
{
	size_t size;
	struct nlist nl[5], *np;
	int buflen = kd->arglen, n = 0;
	char *where = kd->argspc;
	struct kinfo_file2 *kf = NULL;
	struct file *fp, file;
	struct filelist filehead;
	struct filedesc0 filed0;
#define filed	filed0.fd_fd
	struct proclist allproc;
	struct proc *p, proc;
	struct process process;
	struct pcred pcred;
	struct ucred ucred;
	int i, nfiles, nprocs;

	nl[0].n_name = "_filehead";
	nl[1].n_name = "_nfiles";
	nl[2].n_name = "_nprocs";
	nl[3].n_name = "_allproc";
	nl[4].n_name = 0;

	if (kvm_nlist(kd, nl) != 0) {
		for (np = nl; np->n_type != 0; ++np)
			;
		_kvm_err(kd, kd->program,
			 "%s: no such symbol", np->n_name);
		return (NULL);
	}
	if (KREAD(kd, nl[0].n_value, &filehead)) {
		_kvm_err(kd, kd->program, "can't read filehead");
		return (NULL);
	}
	if (KREAD(kd, nl[1].n_value, &nfiles)) {
		_kvm_err(kd, kd->program, "can't read nfiles");
		return (NULL);
	}
	if (KREAD(kd, nl[2].n_value, &nprocs)) {
		_kvm_err(kd, kd->program, "can't read nprocs");
		return (NULL);
	}
	if (KREAD(kd, nl[3].n_value, &allproc)) {
		_kvm_err(kd, kd->program, "can't read allproc");
		return (NULL);
	}
	/* this may be more room than we need but counting is expensive */
	size = (nfiles + 10) * sizeof(struct kinfo_file2);
	kd->filebase = _kvm_malloc(kd, size);
	if (kd->filebase == NULL)
		return (NULL);

	LIST_FOREACH(p, &allproc, p_list) {
		if (buflen < sizeof(struct kinfo_file2))
			break;

		if (KREAD(kd, (u_long)p, &proc)) {
			_kvm_err(kd, kd->program, "can't read proc at %x", p);
			return (NULL);
		}

		/* skip system, embryonic and undead processes */
		if ((proc.p_flag & P_SYSTEM) ||
		    proc.p_stat == SIDL || proc.p_stat == SZOMB)
			continue;
		if (op == KERN_FILE_BYPID) {
			if (arg > 0 && proc.p_pid != (pid_t)arg) {
				/* not the pid we are looking for */
				continue;
			}
		} else /* if (op == KERN_FILE_BYUID) */ {
			if (arg > 0 && proc.p_ucred->cr_uid != (uid_t)arg) {
				/* not the uid we are looking for */
				continue;
			}
		}

		if (proc.p_fd == NULL || proc.p_p == NULL)
			continue;

		if (KREAD(kd, (u_long)proc.p_p, &process)) {
			_kvm_err(kd, kd->program, "can't read process at %x",
			    proc.p_p);
			return (NULL);
		}
		proc.p_p = &process;

		if (KREAD(kd, (u_long)process.ps_cred, &pcred) == 0)
			KREAD(kd, (u_long)pcred.pc_ucred, &ucred);
		process.ps_cred = &pcred;
		pcred.pc_ucred = &ucred;

		if (KREAD(kd, (u_long)proc.p_fd, &filed0)) {
			_kvm_err(kd, kd->program, "can't read filedesc at %x",
			    proc.p_fd);
			return (NULL);
		}
		proc.p_fd = &filed;

		if (proc.p_textvp) {
			if (buflen < sizeof(struct kinfo_file2))
				goto done;
			kf = (struct kinfo_file2 *)where;
			where += sizeof(struct kinfo_file2);
			buflen -= sizeof(struct kinfo_file2);
			n++;
			if (fill_file2(kd, kf, NULL, proc.p_textvp, &proc,
			    KERN_FILE_TEXT) == -1)
				return (NULL);
		}
		if (filed.fd_cdir) {
			if (buflen < sizeof(struct kinfo_file2))
				goto done;
			kf = (struct kinfo_file2 *)where;
			where += sizeof(struct kinfo_file2);
			buflen -= sizeof(struct kinfo_file2);
			n++;
			if (fill_file2(kd, kf, NULL, filed.fd_cdir, &proc,
			    KERN_FILE_CDIR) == -1)
				return (NULL);
		}
		if (filed.fd_rdir) {
			if (buflen < sizeof(struct kinfo_file2))
				goto done;
			kf = (struct kinfo_file2 *)where;
			where += sizeof(struct kinfo_file2);
			buflen -= sizeof(struct kinfo_file2);
			n++;
			if (fill_file2(kd, kf, NULL, filed.fd_rdir, &proc,
			    KERN_FILE_RDIR) == -1)
				return (NULL);
		}
		if (proc.p_tracep) {
			if (buflen < sizeof(struct kinfo_file2))
				goto done;
			kf = (struct kinfo_file2 *)where;
			where += sizeof(struct kinfo_file2);
			buflen -= sizeof(struct kinfo_file2);
			n++;
			if (fill_file2(kd, kf, NULL, proc.p_tracep, &proc,
			    KERN_FILE_TRACE) == -1)
				return (NULL);
		}

		if (filed.fd_nfiles < 0 ||
		    filed.fd_lastfile >= filed.fd_nfiles ||
		    filed.fd_freefile > filed.fd_lastfile + 1) {
			_kvm_err(kd, kd->program,
			    "filedesc corrupted at %x for pid %d",
			    proc.p_fd, proc.p_pid);
			return (NULL);
		}

		for (i = 0; i < filed.fd_nfiles; i++) {
			if (buflen < sizeof(struct kinfo_file2))
				goto done;
			if ((fp = filed.fd_ofiles[i]) == NULL)
				continue;
			if (KREAD(kd, (u_long)fp, &file)) {
				_kvm_err(kd, kd->program, "can't read file");
				return (NULL);
			}
			kf = (struct kinfo_file2 *)where;
			where += sizeof(struct kinfo_file2);
			buflen -= sizeof(struct kinfo_file2);
			n++;
			if (fill_file2(kd, kf, &file, NULL, &proc, i) == -1)
				return (NULL);
		}
	}
done:
	*cnt = n;
	return (kf);
}

static int
fill_file2(kvm_t *kd, struct kinfo_file2 *kf, struct file *fp, struct vnode *vp,
    struct proc *p, int fd)
{
	struct ucred f_cred;

	memset(kf, 0, sizeof(*kf));

	kf->fd_fd = fd;		/* might not really be an fd */

	if (fp != NULL) {
		/* Fill in f_cred */
		if (KREAD(kd, (u_long)fp->f_cred, &f_cred)) {
			_kvm_err(kd, kd->program, "can't read f_cred");
			return (-1);
		}
		fp->f_cred = &f_cred;

		kf->f_fileaddr = PTRTOINT64(fp);
		kf->f_flag = fp->f_flag;
		kf->f_iflags = fp->f_iflags;
		kf->f_type = fp->f_type;
		kf->f_count = fp->f_count;
		kf->f_msgcount = fp->f_msgcount;
		kf->f_ucred = PTRTOINT64(fp->f_cred);
		kf->f_uid = fp->f_cred->cr_uid;
		kf->f_gid = fp->f_cred->cr_gid;
		kf->f_ops = PTRTOINT64(fp->f_ops);
		kf->f_offset = fp->f_offset;
		kf->f_data = PTRTOINT64(fp->f_data);
		kf->f_usecount = fp->f_usecount;

		if (getuid() == 0 || p->p_ucred->cr_uid == fp->f_cred->cr_uid) {
			kf->f_rxfer = fp->f_rxfer;
			kf->f_rwfer = fp->f_wxfer;
			kf->f_seek = fp->f_seek;
			kf->f_rbytes = fp->f_rbytes;
			kf->f_wbytes = fp->f_rbytes;
		}
	} else if (vp != NULL) {
		/* fake it */
		kf->f_type = DTYPE_VNODE;
		kf->f_flag = FREAD;
		if (fd == KERN_FILE_TRACE)
			kf->f_flag |= FWRITE;
	}

	/* information about the object associated with this file */
	switch (kf->f_type) {
	case DTYPE_VNODE: {
		struct vnode vbuf;
		struct mount mount;

		if (KREAD(kd, (u_long)(fp ? fp->f_data : vp), &vbuf)) {
			_kvm_err(kd, kd->program, "can't read vnode");
			return (-1);
		}
		vp = &vbuf;

		if (KREAD(kd, (u_long)vp->v_mount, &mount)) {
			_kvm_err(kd, kd->program, "can't read v_mount");
			return (-1);
		}
		vp->v_mount = &mount;

		kf->v_un = PTRTOINT64(vp->v_un.vu_socket);
		kf->v_type = vp->v_type;
		kf->v_tag = vp->v_tag;
		kf->v_flag = vp->v_flag;
		kf->v_data = PTRTOINT64(vp->v_data);
		kf->v_mount = PTRTOINT64(vp->v_mount);
		strlcpy(kf->f_mntonname, vp->v_mount->mnt_stat.f_mntonname,
		    sizeof(kf->f_mntonname));

		/* Fill in va_fsid, va_fileid, va_mode, va_size, va_rdev */
		filestat(kd, kf, vp);
		break;
	    }

	case DTYPE_SOCKET: {
		struct socket sock;
		struct protosw protosw;
		struct domain domain;

		if (KREAD(kd, (u_long)fp->f_data, &sock)) {
			_kvm_err(kd, kd->program, "can't read socket");
			return (-1);
		}

		kf->so_type = sock.so_type;
		kf->so_state = sock.so_state;
		kf->so_pcb = PTRTOINT64(sock.so_pcb);
		if (KREAD(kd, (u_long)sock.so_proto, &protosw)) {
			_kvm_err(kd, kd->program, "can't read protosw");
			return (-1);
		}
		kf->so_protocol = protosw.pr_protocol;
		if (KREAD(kd, (u_long)protosw.pr_domain, &domain)) {
			_kvm_err(kd, kd->program, "can't read domain");
			return (-1);
		}
		kf->so_family = domain.dom_family;
		if (!sock.so_pcb)
			break;
		switch (kf->so_family) {
		case AF_INET: {
			struct inpcb inpcb;

			if (KREAD(kd, (u_long)sock.so_pcb, &inpcb)) {
				_kvm_err(kd, kd->program, "can't read inpcb");
				return (-1);
			}
			kf->inp_ppcb = PTRTOINT64(inpcb.inp_ppcb);
			kf->inp_lport = inpcb.inp_lport;
			kf->inp_laddru[0] = inpcb.inp_laddr.s_addr;
			kf->inp_fport = inpcb.inp_fport;
			kf->inp_faddru[0] = inpcb.inp_faddr.s_addr;
			break;
		    }
		case AF_INET6: {
			struct inpcb inpcb;
#define s6_addr32 __u6_addr.__u6_addr32

			if (KREAD(kd, (u_long)sock.so_pcb, &inpcb)) {
				_kvm_err(kd, kd->program, "can't read inpcb");
				return (-1);
			}
			kf->inp_ppcb = PTRTOINT64(inpcb.inp_ppcb);
			kf->inp_lport = inpcb.inp_lport;
			kf->inp_laddru[0] = inpcb.inp_laddr6.s6_addr32[0];
			kf->inp_laddru[1] = inpcb.inp_laddr6.s6_addr32[1];
			kf->inp_laddru[2] = inpcb.inp_laddr6.s6_addr32[2];
			kf->inp_laddru[3] = inpcb.inp_laddr6.s6_addr32[3];
			kf->inp_fport = inpcb.inp_fport;
			kf->inp_faddru[0] = inpcb.inp_laddr6.s6_addr32[0];
			kf->inp_faddru[1] = inpcb.inp_faddr6.s6_addr32[1];
			kf->inp_faddru[2] = inpcb.inp_faddr6.s6_addr32[2];
			kf->inp_faddru[3] = inpcb.inp_faddr6.s6_addr32[3];
			break;
		    }
		case AF_UNIX: {
			struct unpcb unpcb;

			if (KREAD(kd, (u_long)sock.so_pcb, &unpcb)) {
				_kvm_err(kd, kd->program, "can't read unpcb");
				return (-1);
			}
			kf->unp_conn = PTRTOINT64(unpcb.unp_conn);
			break;
		    }
		}
		break;
	    }

	case DTYPE_PIPE: {
		struct pipe pipe;

		if (KREAD(kd, (u_long)fp->f_data, &pipe)) {
			_kvm_err(kd, kd->program, "can't read pipe");
			return (-1);
		}
		kf->pipe_peer = PTRTOINT64(pipe.pipe_peer);
		kf->pipe_state = pipe.pipe_state;
		break;
	    }

	case DTYPE_KQUEUE: {
		struct kqueue kqi;

		if (KREAD(kd, (u_long)fp->f_data, &kqi)) {
			_kvm_err(kd, kd->program, "can't read kqi");
			return (-1);
		}
		kf->kq_count = kqi.kq_count;
		kf->kq_state = kqi.kq_state;
		break;
	    }
	case DTYPE_SYSTRACE: {
		struct fsystrace f;

		if (KREAD(kd, (u_long)fp->f_data, &f)) {
			_kvm_err(kd, kd->program, "can't read fsystrace");
			return (-1);
		}
		kf->str_npolicies = f.npolicies;
		break;
	    }
	}

	/* per-process information for KERN_FILE_BY[PU]ID */
	if (p != NULL) {
		kf->p_pid = p->p_pid;
		kf->p_uid = p->p_ucred->cr_uid;
		kf->p_gid = p->p_ucred->cr_gid;
		strlcpy(kf->p_comm, p->p_comm, sizeof(kf->p_comm));
		if (p->p_fd != NULL)
			kf->fd_ofileflags = p->p_fd->fd_ofileflags[fd];
	}

	return (0);
}

mode_t
_kvm_getftype(enum vtype v_type)
{
	mode_t ftype = 0;

	switch (v_type) {
	case VREG:
		ftype = S_IFREG;
		break;
	case VDIR:
		ftype = S_IFDIR;
		break;
	case VBLK:
		ftype = S_IFBLK;
		break;
	case VCHR:
		ftype = S_IFCHR;
		break;
	case VLNK:
		ftype = S_IFLNK;
		break;
	case VSOCK:
		ftype = S_IFSOCK;
		break;
	case VFIFO:
		ftype = S_IFIFO;
		break;
	case VNON:
	case VBAD:
		break;
	}
	
	return (ftype);
}

static int
ufs_filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	struct inode inode;
	struct ufs1_dinode di1;

	if (KREAD(kd, (u_long)VTOI(vp), &inode)) {
		_kvm_err(kd, kd->program, "can't read inode at %p", VTOI(vp));
		return (-1);
	}

	if (KREAD(kd, (u_long)inode.i_din1, &di1)) {
		_kvm_err(kd, kd->program, "can't read dinode at %p",
		    inode.i_din1);
		return (-1);
	}

	inode.i_din1 = &di1;

	kf->va_fsid = inode.i_dev & 0xffff;
	kf->va_fileid = (long)inode.i_number;
	kf->va_mode = inode.i_ffs1_mode;
	kf->va_size = inode.i_ffs1_size;
	kf->va_rdev = inode.i_ffs1_rdev;

	return (0);
}

static int
ext2fs_filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	struct inode inode;
	struct ext2fs_dinode e2di;

	if (KREAD(kd, (u_long)VTOI(vp), &inode)) {
		_kvm_err(kd, kd->program, "can't read inode at %p", VTOI(vp));
		return (-1);
	}

	if (KREAD(kd, (u_long)inode.i_e2din, &e2di)) {
		_kvm_err(kd, kd->program, "can't read dinode at %p",
		    inode.i_e2din);
		return (-1);
	}

	inode.i_e2din = &e2di;

	kf->va_fsid = inode.i_dev & 0xffff;
	kf->va_fileid = (long)inode.i_number;
	kf->va_mode = inode.i_e2fs_mode;
	kf->va_size = inode.i_e2fs_size;
	kf->va_rdev = 0;	/* XXX */

	return (0);
}

static int
msdos_filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	struct denode de;
	struct msdosfsmount mp;

	if (KREAD(kd, (u_long)VTODE(vp), &de)) {
		_kvm_err(kd, kd->program, "can't read denode at %p", VTODE(vp));
		return (-1);
	}
	if (KREAD(kd, (u_long)de.de_pmp, &mp)) {
		_kvm_err(kd, kd->program, "can't read mount struct at %p",
		    de.de_pmp);
		return (-1);
	}

	kf->va_fsid = de.de_dev & 0xffff;
	kf->va_fileid = 0; /* XXX see msdosfs_vptofh() for more info */
	kf->va_mode = (mp.pm_mask & 0777) | _kvm_getftype(vp->v_type);
	kf->va_size = de.de_FileSize;
	kf->va_rdev = 0;  /* msdosfs doesn't support device files */

	return (0);
}

static int
nfs_filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	struct nfsnode nfsnode;

	if (KREAD(kd, (u_long)VTONFS(vp), &nfsnode)) {
		_kvm_err(kd, kd->program, "can't read nfsnode at %p",
		    VTONFS(vp));
		return (-1);
	}
	kf->va_fsid = nfsnode.n_vattr.va_fsid;
	kf->va_fileid = nfsnode.n_vattr.va_fileid;
	kf->va_size = nfsnode.n_size;
	kf->va_rdev = nfsnode.n_vattr.va_rdev;
	kf->va_mode = (mode_t)nfsnode.n_vattr.va_mode | _kvm_getftype(vp->v_type);

	return (0);
}

static int
nnpfs_filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	struct nnpfs_node nnpfs_node;

	if (KREAD(kd, (u_long)VNODE_TO_XNODE(vp), &nnpfs_node)) {
		_kvm_err(kd, kd->program, "can't read nnpfs_node at %p",
		    VTOI(vp));
		return (-1);
	}
	kf->va_fsid = nnpfs_node.attr.va_fsid;
	kf->va_fileid = (long)nnpfs_node.attr.va_fileid;
	kf->va_mode = nnpfs_node.attr.va_mode;
	kf->va_size = nnpfs_node.attr.va_size;
	kf->va_rdev = nnpfs_node.attr.va_rdev;

	return (0);
}

static int
spec_filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	struct specinfo		specinfo;
	struct vnode		parent;

	if (KREAD(kd, (u_long)vp->v_specinfo, &specinfo)) {
		_kvm_err(kd, kd->program, "can't read specinfo at %p",
		     vp->v_specinfo);
		return (-1);
	}

	vp->v_specinfo = &specinfo;

	if (KREAD(kd, (u_long)vp->v_specparent, &parent)) {
		_kvm_err(kd, kd->program, "can't read parent vnode at %p",
		     vp->v_specparent);
		return (-1);
	}

	if (ufs_filestat(kd, kf, vp))
		return (-1);

	return (0);
}

static int
filestat(kvm_t *kd, struct kinfo_file2 *kf, struct vnode *vp)
{
	int ret = 0;

	if (vp->v_type != VNON && vp->v_type != VBAD) {
		switch (vp->v_tag) {
		case VT_UFS:
		case VT_MFS:
			ret = ufs_filestat(kd, kf, vp);
			break;
		case VT_NFS:
			ret = nfs_filestat(kd, kf, vp);
			break;
		case VT_EXT2FS:
			ret = ext2fs_filestat(kd, kf, vp);
			break;
		case VT_ISOFS:
			ret = _kvm_stat_cd9660(kd, kf, vp);
			break;
		case VT_MSDOSFS:
			ret = msdos_filestat(kd, kf, vp);
			break;
		case VT_NNPFS:
			ret = nnpfs_filestat(kd, kf, vp);
			break;
		case VT_UDF:
			ret = _kvm_stat_udf(kd, kf, vp);
			break;
		case VT_NTFS:
			ret = _kvm_stat_ntfs(kd, kf, vp);
			break;
		case VT_NON:
			if (vp->v_flag & VCLONE)
				ret = spec_filestat(kd, kf, vp);
			break;
		}
	}
	return (ret);
}
