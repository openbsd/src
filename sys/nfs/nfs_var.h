/*	$OpenBSD: nfs_var.h,v 1.10 2000/02/07 04:48:42 assar Exp $	*/
/*	$NetBSD: nfs_var.h,v 1.3 1996/02/18 11:53:54 fvdl Exp $	*/

/*
 * Copyright (c) 1996 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * XXX needs <nfs/rpcv2.h> and <nfs/nfs.h> because of typedefs
 */

struct vnode;
struct uio;
struct ucred;
struct proc;
struct buf;
struct nfs_diskless;
struct sockaddr_in;
struct nfs_dlmount;
struct vnode;
struct nfsd;
struct mbuf;
struct file;
struct nqlease;
struct nqhost;
struct nfssvc_sock;
struct nfsmount;
struct socket;
struct nfsreq;
struct vattr;
struct nameidata;
struct nfsnode;
struct sillyrename;
struct componentname;
struct nfsd_srvargs;
struct nfsrv_descript;
struct nfs_fattr;
union nethostaddr;

/* nfs_bio.c */
int nfs_bioread __P((struct vnode *, struct uio *, int, struct ucred *));
int nfs_write __P((void *));
struct buf *nfs_getcacheblk __P((struct vnode *, daddr_t, int, struct proc *));
int nfs_vinvalbuf __P((struct vnode *, int, struct ucred *, struct proc *,
		       int));
int nfs_asyncio __P((struct buf *, struct ucred *));
int nfs_doio __P((struct buf *, struct ucred *, struct proc *));

/* nfs_boot.c */
int nfs_boot_init __P((struct nfs_diskless *, struct proc *));
int nfs_boot_init __P((struct nfs_diskless *, struct proc *));

/* nfs_node.c */
void nfs_nhinit __P((void));
u_long nfs_hash __P((nfsfh_t *, int));
int nfs_nget __P((struct mount *, nfsfh_t *, int, struct nfsnode **));
int nfs_inactive __P((void *));
int nfs_reclaim __P((void *));

/* nfs_vnops.c */
int nfs_null __P((struct vnode *, struct ucred *, struct proc *));
int nfs_access __P((void *));
int nfs_open __P((void *));
int nfs_close __P((void *));
int nfs_getattr __P((void *));
int nfs_setattr __P((void *));
int nfs_setattrrpc __P((struct vnode *, struct vattr *, struct ucred *,
			struct proc *));
int nfs_lookup __P((void *));
int nfs_read __P((void *));
int nfs_readlink __P((void *));
int nfs_readlinkrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_readrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_writerpc __P((struct vnode *, struct uio *, struct ucred *, int *,
		      int *));
int nfs_mknodrpc __P((struct vnode *, struct vnode **, struct componentname *,
		      struct vattr *));
int nfs_mknod __P((void *));
int nfs_create __P((void *));
int nfs_remove __P((void *));
int nfs_removeit __P((struct sillyrename *));
int nfs_removerpc __P((struct vnode *, char *, int, struct ucred *,
		       struct proc *));
int nfs_rename __P((void *));
int nfs_renameit __P((struct vnode *, struct componentname *,
		      struct sillyrename *));
int nfs_renamerpc __P((struct vnode *, char *, int, struct vnode *, char *, int,
		       struct ucred *, struct proc *));
int nfs_link __P((void *));
int nfs_symlink __P((void *));
int nfs_mkdir __P((void *));
int nfs_rmdir __P((void *));
int nfs_readdir __P((void *));
int nfs_readdirrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_readdirplusrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_sillyrename __P((struct vnode *, struct vnode *,
			 struct componentname *));
int nfs_lookitup __P((struct vnode *, char *, int, struct ucred *,
		      struct proc *, struct nfsnode **));
int nfs_commit __P((struct vnode *, u_quad_t, int, struct ucred *,
		    struct proc *));
int nfs_bmap __P((void *));
int nfs_strategy __P((void *));
int nfs_mmap __P((void *));
int nfs_fsync __P((void *));
int nfs_flush __P((struct vnode *, struct ucred *, int, struct proc *, int));
int nfs_pathconf __P((void *));
int nfs_advlock __P((void *));
int nfs_print __P((void *));
int nfs_blkatoff __P((void *));
int nfs_valloc __P((void *));
int nfs_vfree __P((void *));
int nfs_truncate __P((void *));
int nfs_update __P((void *));
int nfs_bwrite __P((void *));
int nfs_writebp __P((struct buf *, int));
int nfsspec_access __P((void *));
int nfsspec_read __P((void *));
int nfsspec_write __P((void *));
int nfsspec_close __P((void *));
int nfsfifo_read __P((void *));
int nfsfifo_write __P((void *));
int nfsfifo_close __P((void *));

/* nfs_nqlease.c */
void nqnfs_lease_updatetime __P((int));
void nqnfs_clientlease __P((struct nfsmount *, struct nfsnode *, int, int,
			    time_t, u_quad_t));
void nqsrv_locklease __P((struct nqlease *));
void nqsrv_unlocklease __P((struct nqlease *));
int nqsrv_getlease __P((struct vnode *, u_int32_t *, int, struct nfssvc_sock *,
			struct proc *, struct mbuf *, int *, u_quad_t *,
			struct ucred *));
int nqnfs_vop_lease_check __P((void *));
void nqsrv_addhost __P((struct nqhost *, struct nfssvc_sock *, struct mbuf *));
void nqsrv_instimeq __P((struct nqlease *, u_int32_t));
int nqsrv_cmpnam __P((struct nfssvc_sock *, struct mbuf *, struct nqhost *));
void nqsrv_send_eviction __P((struct vnode *, struct nqlease *,
			      struct nfssvc_sock *, struct mbuf *,
			      struct ucred *));
void nqsrv_waitfor_expiry __P((struct nqlease *));
void nqnfs_serverd __P((void));
int nqnfsrv_getlease __P((struct nfsrv_descript *, struct nfssvc_sock *,
			  struct proc *, struct mbuf **));
int nqnfsrv_vacated __P((struct nfsrv_descript *, struct nfssvc_sock *,
			 struct proc *, struct mbuf **));
int nqnfs_getlease __P((struct vnode *, int, struct ucred *, struct proc *));
int nqnfs_vacated __P((struct vnode *, struct ucred *));
int nqnfs_callback __P((struct nfsmount *, struct mbuf *, struct mbuf *,
			caddr_t));

/* nfs_serv.c */
int nfsrv3_access __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct proc *, struct mbuf **));
int nfsrv_getattr __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct proc *, struct mbuf **));
int nfsrv_setattr __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct proc *, struct mbuf **));
int nfsrv_lookup __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_readlink __P((struct nfsrv_descript *, struct nfssvc_sock *,
			struct proc *, struct mbuf **));
int nfsrv_read __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct proc *, struct mbuf **));
int nfsrv_write __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct proc *, struct mbuf **));
int nfsrv_writegather __P((struct nfsrv_descript **, struct nfssvc_sock *,
			   struct proc *, struct mbuf **));
void nfsrvw_coalesce __P((struct nfsrv_descript *, struct nfsrv_descript *));
int nfsrv_create __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_mknod __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct proc *, struct mbuf **));
int nfsrv_remove __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_rename __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_link __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct proc *, struct mbuf **));
int nfsrv_symlink __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct proc *, struct mbuf **));
int nfsrv_mkdir __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct proc *, struct mbuf **));
int nfsrv_rmdir __P((struct nfsrv_descript *, struct nfssvc_sock *,
		     struct proc *, struct mbuf **));
int nfsrv_readdir __P((struct nfsrv_descript *, struct nfssvc_sock *,
		       struct proc *, struct mbuf **));
int nfsrv_readdirplus __P((struct nfsrv_descript *, struct nfssvc_sock *,
			   struct proc *, struct mbuf **));
int nfsrv_commit __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_statfs __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_fsinfo __P((struct nfsrv_descript *, struct nfssvc_sock *,
		      struct proc *, struct mbuf **));
int nfsrv_pathconf __P((struct nfsrv_descript *, struct nfssvc_sock *,
		        struct proc *, struct mbuf **));
int nfsrv_null __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct proc *, struct mbuf **));
int nfsrv_noop __P((struct nfsrv_descript *, struct nfssvc_sock *,
		    struct proc *, struct mbuf **));
int nfsrv_access __P((struct vnode *, int, struct ucred *, int, struct proc *,
		    int));

/* nfs_socket.c */
int nfs_connect __P((struct nfsmount *, struct nfsreq *));
int nfs_reconnect __P((struct nfsreq *));
void nfs_disconnect __P((struct nfsmount *));
int nfs_send __P((struct socket *, struct mbuf *, struct mbuf *,
		  struct nfsreq *));
int nfs_receive __P((struct nfsreq *, struct mbuf **, struct mbuf **));
int nfs_reply __P((struct nfsreq *));
int nfs_request __P((struct vnode *, struct mbuf *, int, struct proc *,
		     struct ucred *, struct mbuf **, struct mbuf **,
		     caddr_t *));
int nfs_rephead __P((int, struct nfsrv_descript *, struct nfssvc_sock *,
		     int, int, u_quad_t *, struct mbuf **, struct mbuf **,			     caddr_t *));
void nfs_timer __P((void *));
int nfs_sigintr __P((struct nfsmount *, struct nfsreq *, struct proc *));
int nfs_sndlock __P((int *, struct nfsreq *));
void nfs_sndunlock __P((int *));
int nfs_rcvlock __P((struct nfsreq *));
void nfs_rcvunlock __P((int *));
void nfs_realign __P((struct mbuf *, int));
int nfs_getreq __P((struct nfsrv_descript *, struct nfsd *, int));
int nfs_msg __P((struct proc *, char *, char *));
void nfsrv_rcv __P((struct socket *, caddr_t, int));
int nfsrv_getstream __P((struct nfssvc_sock *, int));
int nfsrv_dorec __P((struct nfssvc_sock *, struct nfsd *,
		     struct nfsrv_descript **));
void nfsrv_wakenfsd __P((struct nfssvc_sock *));

/* nfs_srvcache.c */
void nfsrv_initcache __P((void ));
int nfsrv_getcache __P((struct nfsrv_descript *, struct nfssvc_sock *,
			struct mbuf **));
void nfsrv_updatecache __P((struct nfsrv_descript *, int, struct mbuf *));
void nfsrv_cleancache __P((void));

/* nfs_subs.c */
struct mbuf *nfsm_reqh __P((struct vnode *, u_long, int, caddr_t *));
struct mbuf *nfsm_rpchead __P((struct ucred *, int, int, int, int, char *, int,
			       char *, struct mbuf *, int, struct mbuf **,
			       u_int32_t *));
int nfsm_mbuftouio __P((struct mbuf **, struct uio *, int, caddr_t *));
int nfsm_uiotombuf __P((struct uio *, struct mbuf **, int, caddr_t *));
int nfsm_disct __P((struct mbuf **, caddr_t *, int, int, caddr_t *));
int nfs_adv __P((struct mbuf **, caddr_t *, int, int));
int nfsm_strtmbuf __P((struct mbuf **, char **, char *, long));
int nfs_vfs_init __P((struct vfsconf *));
void nfs_init __P((void));
int nfs_loadattrcache __P((struct vnode **, struct mbuf **, caddr_t *,
			   struct vattr *));
int nfs_getattrcache __P((struct vnode *, struct vattr *));
int nfs_namei __P((struct nameidata *, fhandle_t *, int, struct nfssvc_sock *,
		   struct mbuf *, struct mbuf **, caddr_t *, struct vnode **,
		   struct proc *, int));
void nfsm_adj __P((struct mbuf *, int, int));
void nfsm_srvwcc __P((struct nfsrv_descript *, int, struct vattr *, int,
		      struct vattr *, struct mbuf **, char **));
void nfsm_srvpostopattr __P((struct nfsrv_descript *, int, struct vattr *,
			     struct mbuf **, char **));
void nfsm_srvfattr __P((struct nfsrv_descript *, struct vattr *,
			struct nfs_fattr *));
int nfsrv_fhtovp __P((fhandle_t *, int, struct vnode **, struct ucred *,
		      struct nfssvc_sock *, struct mbuf *, int *, int));
int netaddr_match __P((int, union nethostaddr *, struct mbuf *));
nfsuint64 *nfs_getcookie __P((struct nfsnode *, off_t off, int));
void nfs_invaldir __P((struct vnode *));
void nfs_clearcommit __P((struct mount *));
int nfsrv_errmap __P((struct nfsrv_descript *, int));
void nfsrvw_sort __P((gid_t *, int));
void nfsrv_setcred __P((struct ucred *, struct ucred *));

/* nfs_syscalls.c */
int sys_nfssvc __P((struct proc *, void *, register_t *));
int nfssvc_addsock __P((struct file *, struct mbuf *));
int nfssvc_nfsd __P((struct nfsd_srvargs *, caddr_t, struct proc *));
void nfsrv_zapsock __P((struct nfssvc_sock *));
void nfsrv_slpderef __P((struct nfssvc_sock *));
void nfsrv_init __P((int));
int nfssvc_iod __P((struct proc *));
int nfs_getauth __P((struct nfsmount *, struct nfsreq *, struct ucred *,
		     char **, int *, char *, int *, NFSKERBKEY_T));
int nfs_getnickauth __P((struct nfsmount *, struct ucred *, char **, int *,
			 char *, int));
int nfs_savenickauth __P((struct nfsmount *, struct ucred *, int, NFSKERBKEY_T,
			  struct mbuf **, char **, struct mbuf *));
