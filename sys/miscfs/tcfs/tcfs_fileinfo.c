#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <miscfs/tcfs/tcfs.h>
#include "tcfs_rw.h"


tcfs_fileinfo tcfs_xgetflags(struct vnode *v, struct proc *p, struct ucred *c)
{
        tcfs_fileinfo r;
        struct vop_getattr_args x;
        struct vattr att;
        int retval;

        att=va_null;
        x.a_desc=VDESC(vop_getattr);
        x.a_vp=v;
        x.a_vap=&att;
        x.a_cred=c;
        x.a_p=p;

        retval=tcfs_bypass((void*)&x);
        r.flag=(unsigned long)(x.a_vap->va_flags);
        r.end_of_file=x.a_vap->va_size;

        return r;
}

int tcfs_xsetflags(struct vnode *v, struct proc *p, struct ucred *c, tcfs_fileinfo *i)
{
        struct vop_setattr_args x;
        struct vattr att;
        int retval;

        att=va_null;

	att.va_flags=i->flag;

        x.a_desc=VDESC(vop_setattr);
        x.a_vp=v;
        x.a_vap=&att;
        x.a_cred=c;
        x.a_p=p;

        retval=tcfs_bypass((void*)&x);
        return retval;
}



tcfs_fileinfo tcfs_get_fileinfo(void *a)
{
	struct vop_read_args *arg;

	arg=(struct vop_read_args*)a;
	return tcfs_xgetflags(arg->a_vp,arg->a_uio->uio_procp,arg->a_cred);
}


int tcfs_set_fileinfo(void *a, tcfs_fileinfo *i)
{
	struct vop_read_args *arg;

	arg=(struct vop_read_args*)a;
	return tcfs_xsetflags(arg->a_vp, arg->a_uio->uio_procp,arg->a_cred,i); ;
}

