/*	$OpenBSD: nfs_var.h,v 1.1 1996/02/29 09:24:57 niklas Exp $	*/
/*	$NetBSD: nfs_var.h,v 1.2 1996/02/13 17:06:52 christos Exp $	*/

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
struct nfsd_cargs;


/* nfs_bio.c */
int nfs_bioread __P((struct vnode *, struct uio *, int, struct ucred *));
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
struct nfsnodehashhead *nfs_hash __P((nfsv2fh_t *));
int nfs_nget __P((struct mount *, nfsv2fh_t *, struct nfsnode **));

/* nfs_vnops.c */
int nfs_null __P((struct vnode *, struct ucred *, struct proc *));
int nfs_readlinkrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_readrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_writerpc __P((struct vnode *, struct uio *, struct ucred *, int));
int nfs_removeit __P((struct sillyrename *));
int nfs_renameit __P((struct vnode *, struct componentname *,
		      struct sillyrename *));
int nfs_readdirrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_readdirlookrpc __P((struct vnode *, struct uio *, struct ucred *));
int nfs_sillyrename __P((struct vnode *, struct vnode *,
			 struct componentname *));
int nfs_lookitup __P((struct sillyrename *, nfsv2fh_t *, struct proc *));

/* nfs_nqlease.c */
int nqsrv_getlease __P((struct vnode *, u_int *, int, struct nfsd *,
			struct mbuf *, int *, u_quad_t *, struct ucred *));
int lease_check __P((void *));
void nqsrv_addhost __P((struct nqhost *, struct nfssvc_sock *, struct mbuf *));
void nqsrv_instimeq __P((struct nqlease *, u_long));
int nqsrv_cmpnam __P((struct nfssvc_sock *, struct mbuf *, struct nqhost *));
void nqsrv_send_eviction __P((struct vnode *, struct nqlease *,
			      struct nfssvc_sock *, struct mbuf *,
			      struct ucred *));
void nqsrv_waitfor_expiry __P((struct nqlease *));
void nqnfs_serverd __P((void));
int nqnfsrv_getlease __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
			  struct ucred *, struct mbuf *, struct mbuf **));
int nqnfsrv_vacated __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
			 struct ucred *, struct mbuf *, struct mbuf **));
int nqnfs_getlease __P((struct vnode *, int, struct ucred *, struct proc *));
int nqnfs_vacated __P((struct vnode *, struct ucred *));
int nqnfs_callback __P((struct nfsmount *, struct mbuf *, struct mbuf *,
		 	caddr_t));
int nqnfs_clientd __P((struct nfsmount *, struct ucred *, struct nfsd_cargs *,
	               int, caddr_t, struct proc *));
void nqnfs_clientlease __P((struct nfsmount *, struct nfsnode *, int, int ,
			    time_t, u_quad_t));
void lease_updatetime __P((int));
void nqsrv_locklease __P((struct nqlease *));
void nqsrv_unlocklease __P((struct nqlease *));

/* nfs_serv.c */
int nqnfsrv_access __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
			struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_getattr __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		       struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_setattr __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		       struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_lookup __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		      struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_readlink __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
			struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_read __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		    struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_write __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		     struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_create __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		      struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_remove __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		      struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_rename __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		      struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_link __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		    struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_symlink __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		       struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_mkdir __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		     struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_rmdir __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		     struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_readdir __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		       struct ucred *, struct mbuf *, struct mbuf **));
int nqnfsrv_readdirlook __P((struct nfsd *, struct mbuf *, struct mbuf *,
			     caddr_t, struct ucred *, struct mbuf *,
			     struct mbuf **));
int nfsrv_statfs __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		      struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_null __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		    struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_noop __P((struct nfsd *, struct mbuf *, struct mbuf *, caddr_t,
		    struct ucred *, struct mbuf *, struct mbuf **));
int nfsrv_access __P((struct vnode *, int, struct ucred *, int, struct proc *));

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
int nfs_rephead __P((int, struct nfsd *, int, int, u_quad_t *, struct mbuf **,
		     struct mbuf **, caddr_t *));
void nfs_timer __P((void *));
int nfs_sigintr __P((struct nfsmount *, struct nfsreq *, struct proc *));
int nfs_sndlock __P((int *, struct nfsreq *));
void nfs_sndunlock __P((int *));
int nfs_rcvlock __P((struct nfsreq *));
void nfs_rcvunlock __P((int *));
void nfs_realign __P((struct mbuf *, int));
int nfs_getreq __P((struct nfsd *, int));
void nfs_msg __P((struct proc *, char *, char *));
void nfsrv_rcv __P((struct socket *, caddr_t, int));
int nfsrv_getstream __P((struct nfssvc_sock *, int));
int nfsrv_dorec __P((struct nfssvc_sock *, struct nfsd *));
void nfsrv_wakenfsd __P((struct nfssvc_sock *));

/* nfs_srvcache.c */
void nfsrv_initcache __P((void));
int nfsrv_getcache __P((struct mbuf *, struct nfsd *, struct mbuf **));
void nfsrv_updatecache __P((struct mbuf *, struct nfsd *, int, struct mbuf *));
void nfsrv_cleancache __P((void));

/* nfs_subs.c */
struct mbuf *nfsm_reqh __P((struct vnode *, u_long, int, caddr_t *));
struct mbuf *nfsm_rpchead __P((struct ucred *, int, int, int, int, char *,
			       struct mbuf *, int, struct mbuf **,
			       u_int32_t *));
int nfsm_mbuftouio __P((struct mbuf **, struct uio *, int, caddr_t *));
int nfsm_uiotombuf __P((struct uio *, struct mbuf **, int, caddr_t *));
int nfsm_disct __P((struct mbuf **, caddr_t *, int, int, caddr_t *));
int nfs_adv __P((struct mbuf **, caddr_t *, int, int));
int nfsm_strtmbuf __P((struct mbuf **, char **, char *, long));
int nfs_loadattrcache __P((struct vnode **, struct mbuf **, caddr_t *,
			   struct vattr *));
int nfs_getattrcache __P((struct vnode *, struct vattr *));
int nfs_namei __P((struct nameidata *, fhandle_t *, int, struct nfssvc_sock *,
		   struct mbuf *, struct mbuf **, caddr_t *, struct proc *));
void nfsm_adj __P((struct mbuf *, int, int));
int nfsrv_fhtovp __P((fhandle_t *, int, struct vnode **, struct ucred *,
		      struct nfssvc_sock *, struct mbuf *, int *));
int netaddr_match __P((int, union nethostaddr *, struct mbuf *));

/* nfs_syscalls.c */
int nfssvc_addsock __P((struct file *, struct mbuf *));
int nfssvc_nfsd __P((struct nfsd_srvargs *, caddr_t, struct proc *));
void nfsrv_zapsock __P((struct nfssvc_sock *));
void nfsrv_slpderef __P((struct nfssvc_sock *));
void nfsrv_init __P((int));
int nfssvc_iod __P((struct proc *));
int nfs_getauth __P((struct nfsmount *, struct nfsreq *, struct ucred *,
		     int *, char **, int *));

