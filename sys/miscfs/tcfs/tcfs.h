#ifndef _TCFS_MOUNT_H_
#include "tcfs_mount.h"
#endif

#ifdef _KERNEL
/*
 * A cache of vnode references
 */
struct tcfs_node {
	LIST_ENTRY(tcfs_node)	tcfs_hash;	/* Hash list */
	struct vnode	        *tcfs_lowervp;	/* VREFed once */
	struct vnode		*tcfs_vnode;	/* Back pointer */
};

extern int tcfs_node_create __P((struct mount *mp, struct vnode *target, struct vnode **vpp, int lockit));

#define	MOUNTTOTCFSMOUNT(mp) ((struct tcfs_mount *)((mp)->mnt_data))
#define	VTOTCFS(vp) ((struct tcfs_node *)(vp)->v_data)
#define	TCFSTOV(xp) ((xp)->tcfs_vnode)
#ifdef TCFS_DIAGNOSTIC
extern struct vnode *tcfs_checkvp __P((struct vnode *vp, char *fil, int lno));
#define	TCFSVPTOLOWERVP(vp) tcfs_checkvp((vp), __FILE__, __LINE__)
#else
#define	TCFSVPTOLOWERVP(vp) (VTOTCFS(vp)->tcfs_lowervp)
#endif

#define TCFS_VP2UKT(vp) ((MOUNTTOTCFSMOUNT(((vp)->v_mount)))->tcfs_uid_kt)
#define TCFS_VP2GKT(vp) ((MOUNTTOTCFSMOUNT(((vp)->v_mount)))->tcfs_gid_kt)

#define tcfs_fhtovp ((int (*) __P((struct mount *, struct fid *, \
	struct vnode **)))eopnotsupp)
#define tcfs_vptofh ((int (*) __P((struct vnode *, struct fid *)))eopnotsupp)

extern int (**tcfs_vnodeop_p) __P((void *));
extern struct vfsops tcfs_vfsops;

int tcfs_init __P((struct vfsconf *));

#define BLOCKSIZE       1024
#define SBLOCKSIZE         8

#define ABS(a)          ((a)>=0?(a):(-a))

/*      variabili esterne       */


/*      prototyphes             */

int     tcfs_bypass __P((void *));
int     tcfs_open __P((void *));
int     tcfs_getattr __P((void *));
int     tcfs_setattr __P((void *));
int     tcfs_inactive __P((void *));
int     tcfs_reclaim __P((void *));
int     tcfs_print __P((void *));
int     tcfs_strategy __P((void *));
int     tcfs_bwrite __P((void *));
int     tcfs_lock __P((void *));
int     tcfs_unlock __P((void *));
int     tcfs_islocked __P((void *));
int     tcfs_read __P((void *));
int     tcfs_readdir __P((void *));
int     tcfs_write __P((void *));
int     tcfs_create __P((void *));
int     tcfs_mknod __P((void *));
int     tcfs_mkdir __P((void *));
int     tcfs_link __P((void *));
int     tcfs_symlink __P((void *));
int     tcfs_rename __P((void *));
int     tcfs_lookup __P((void *));

void *tcfs_getukey(struct ucred *, struct proc *, struct vnode *);
void *tcfs_getpkey(struct ucred *, struct proc *, struct vnode *);
void *tcfs_getgkey(struct ucred *, struct proc *, struct vnode *);
int tcfs_checkukey(struct ucred *, struct proc *, struct vnode *);
int tcfs_checkpkey(struct ucred *, struct proc *, struct vnode *);
int tcfs_checkgkey(struct ucred *, struct proc *, struct vnode *);
int     tcfs_exec_cmd(struct tcfs_mount*, struct tcfs_args *);
int     tcfs_init_mp(struct tcfs_mount*, struct tcfs_args *);
int     tcfs_set_status(struct tcfs_mount *, struct tcfs_args *, int);
 
#define TCFS_CHECK_AKEY(c,p,v) (\
        tcfs_checkukey((c),(p),(v)) || \
        tcfs_checkpkey((c),(p),(v)) || \
        tcfs_checkgkey((c),(p),(v)) )

#endif /* _KERNEL */


