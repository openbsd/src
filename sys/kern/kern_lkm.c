/*	$OpenBSD: kern_lkm.c,v 1.24 1999/02/19 17:17:49 art Exp $	*/
/*	$NetBSD: kern_lkm.c,v 1.31 1996/03/31 21:40:27 christos Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1992 Terrence R. Lambert.
 * All rights reserved.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * XXX it's not really safe to unload *any* of the types which are
 * currently loadable; e.g. you could unload a syscall which was being
 * blocked in, etc.  In the long term, a solution should be come up
 * with, but "not right now." -- cgd
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/syscallargs.h>
#include <sys/conf.h>

#include <sys/lkm.h>
#include <sys/syscall.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#endif

#define	LKM_ALLOC	0x01
#define	LKM_WANT	0x02

#define	LKMS_IDLE	0x00
#define	LKMS_RESERVED	0x01
#define	LKMS_LOADING	0x02
#define	LKMS_LOADING_SYMS	0x03
#define	LKMS_LOADED	0x04
#define	LKMS_UNLOADING	0x08

static int	lkm_v = 0;
static int	lkm_state = LKMS_IDLE;

static TAILQ_HEAD(, lkm_table)	lkmods;	/* table of loaded modules */
static struct lkm_table	*curp;		/* global for in-progress ops */
static size_t		nlkms = 0;	/* number of loaded lkms */

static struct lkm_table *lkmalloc __P((void));  /* allocate new lkm table entry */
static void lkmfree __P((struct lkm_table *)); /* free it */
static struct lkm_table *lkmlookup __P((int, char *, int *));
static void lkmunreserve __P((void));
static int _lkm_syscall __P((struct lkm_table *, int));
static int _lkm_vfs __P((struct lkm_table *, int));
static int _lkm_dev __P((struct lkm_table *, int));
#ifdef STREAMS
static int _lkm_strmod __P((struct lkm_table *, int));
#endif
static int _lkm_exec __P((struct lkm_table *, int));

void lkminit __P((void));
int lkmexists __P((struct lkm_table *));

void
lkminit()
{
	TAILQ_INIT(&lkmods);
}

/*ARGSUSED*/
int
lkmopen(dev, flag, devtype, p)
	dev_t dev;
	int flag;
	int devtype;
	struct proc *p;
{
	int error;

	if (minor(dev) != 0)
		return (ENXIO);		/* bad minor # */

	/*
	 * Use of the loadable kernel module device must be exclusive; we
	 * may try to remove this restriction later, but it's really no
	 * hardship.
	 */
	while (lkm_v & LKM_ALLOC) {
		if (flag & FNONBLOCK)		/* don't hang */
			return (EBUSY);
		lkm_v |= LKM_WANT;
		/*
		 * Sleep pending unlock; we use tsleep() to allow
		 * an alarm out of the open.
		 */
		error = tsleep((caddr_t)&lkm_v, TTIPRI|PCATCH, "lkmopn", 0);
		if (error)
			return (error);	/* leave LKM_WANT set -- no problem */
	}
	lkm_v |= LKM_ALLOC;

	if (nlkms == 0)
		lkminit();	/* XXX */

	return (0);		/* pseudo-device open */
}

/*
 * Alocates new LKM table entry, fills module id, inserts in the list.
 * Returns NULL on failure.
 *
 */
static struct lkm_table *
lkmalloc()
{
	struct lkm_table *ret = NULL;

	MALLOC(ret, struct lkm_table *, sizeof(*ret), M_DEVBUF, M_WAITOK);
	if (ret != NULL) {
		ret->refcnt =
		ret->depcnt = 0;
		ret->id = nlkms++;
		TAILQ_INSERT_TAIL(&lkmods, ret, list);
	}

	return ret;
}

/*
 * Frees the slot, decreases the number of modules.
 */
static void
lkmfree(p)
	struct lkm_table *p;
{
#ifdef	PARANOIA
	if (p == NULL)
		panic("lkmfree: freeing NULL");
#endif
	TAILQ_REMOVE(&lkmods, p, list);
	free(p, M_DEVBUF);
	nlkms--;
}

struct lkm_table *
lkm_list(p)
	struct lkm_table *p;
{
	if (p == NULL)
		p = lkmods.tqh_first;
	else
		p = p->list.tqe_next;

	return p;
}

static struct lkm_table *
lkmlookup(i, name, error)
	int	i;
	char	*name;
	int	*error;
{
	register struct lkm_table *p = NULL;
	char	istr[MAXLKMNAME];

	if (i < 0) {		/* unload by name */
		/*
		 * Copy name and lookup id from all loaded
		 * modules.  May fail.
		 */
	 	*error = copyinstr(name, istr, MAXLKMNAME-1, NULL);
		if (*error)
			return NULL;
		istr[MAXLKMNAME-1] = '\0';

		for (p = lkmods.tqh_first; p != NULL; p = p->list.tqe_next)
			if (!strcmp(istr, p->private.lkm_any->lkm_name))
				break;
	} else if (i >= nlkms) {
		*error = EINVAL;
		return NULL;
	} else
		for (p = lkmods.tqh_first; p != NULL && i--;
		    p = p->list.tqe_next)
			;

	if (p == NULL)
		*error = ENOENT;

	return p;
}

/*
 * Unreserve the memory associated with the current loaded module; done on
 * a coerced close of the lkm device (close on premature exit of modload)
 * or explicitly by modload as a result of a link failure.
 */
static void
lkmunreserve()
{

	if (lkm_state == LKMS_IDLE)
		return;

#ifdef DDB
	if (curp && curp->private.lkm_any && curp->private.lkm_any->lkm_name)
	    db_del_symbol_table(curp->private.lkm_any->lkm_name);
#endif

	/*
	 * Actually unreserve the memory
	 */
	if (curp && curp->area) {
		kmem_free(kmem_map, curp->area, curp->size);/**/
		curp->area = 0;
	}

	lkm_state = LKMS_IDLE;
}

int
lkmclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{

	if (!(lkm_v & LKM_ALLOC)) {
#ifdef LKM_DEBUG
		printf("LKM: close before open!\n");
#endif	/* LKM_DEBUG */
		return (EBADF);
	}

	/* do this before waking the herd... */
	if (curp != NULL) {
		/*
		 * If we close before setting used, we have aborted
		 * by way of error or by way of close-on-exit from
		 * a premature exit of "modload".
		 */
		lkmunreserve();	/* coerce state to LKM_IDLE */
	}

	lkm_v &= ~LKM_ALLOC;
	wakeup((caddr_t)&lkm_v);	/* thundering herd "problem" here */

	return (0);		/* pseudo-device closed */
}

/*ARGSUSED*/
int
lkmioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0, i;
	struct lmc_resrv *resrvp;
	struct lmc_loadbuf *loadbufp;
	struct lmc_unload *unloadp;
	struct lmc_stat	 *statp;

	switch(cmd) {
	case LMRESERV:		/* reserve pages for a module */
	case LMRESERV_O:	/* reserve pages for a module */
		if (securelevel > 0)
			return EPERM;

		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		resrvp = (struct lmc_resrv *)data;

		if ((curp = lkmalloc()) == NULL) {
			error = ENOMEM;		/* no slots available */
			break;
		}
		curp->ver = (cmd == LMRESERV) ? LKM_VERSION : LKM_OLDVERSION;
		resrvp->slot = curp->id;	/* return slot */

		/*
		 * Get memory for module
		 */
		curp->size = resrvp->size;

		curp->area = kmem_alloc(kmem_map, curp->size);/**/

		curp->offset = 0;		/* load offset */

		resrvp->addr = curp->area; /* ret kernel addr */

		if (cmd == LMRESERV && resrvp->sym_size) {
			curp->sym_size = resrvp->sym_size;
			curp->sym_symsize = resrvp->sym_symsize;
			curp->syms = (caddr_t)kmem_alloc(kmem_map, curp->sym_size);
			curp->sym_offset = 0;
			resrvp->sym_addr = curp->syms; /* ret symbol addr */
		} else {
			curp->sym_size = 0;
			curp->syms = 0;
			curp->sym_offset = 0;
			if (cmd == LMRESERV)
				resrvp->sym_addr = 0;
		}
#ifdef LKM_DEBUG
		printf("LKM: LMRESERV (actual   = 0x%08lx)\n", curp->area);
		printf("LKM: LMRESERV (syms     = 0x%08x)\n", curp->syms);
		printf("LKM: LMRESERV (adjusted = 0x%08lx)\n",
			trunc_page(curp->area));
#endif	/* LKM_DEBUG */
		lkm_state = LKMS_RESERVED;
		break;

	case LMLOADBUF:		/* Copy in; stateful, follows LMRESERV */
		if (securelevel > 0)
			return EPERM;

		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		loadbufp = (struct lmc_loadbuf *)data;
		if ((lkm_state != LKMS_RESERVED && lkm_state != LKMS_LOADING)
		    || loadbufp->cnt < 0
		    || loadbufp->cnt > MODIOBUF
		    || loadbufp->cnt > curp->size - curp->offset) {
			error = ENOMEM;
			break;
		}

		/* copy in buffer full of data */
		error = copyin(loadbufp->data, 
			(caddr_t)curp->area + curp->offset, loadbufp->cnt);
		if (error)
			break;

		if ((curp->offset + loadbufp->cnt) < curp->size) {
			lkm_state = LKMS_LOADING;
#ifdef LKM_DEBUG
			printf("LKM: LMLOADBUF (loading @ %ld of %ld, i = %d)\n",
			curp->offset, curp->size, loadbufp->cnt);
#endif	/* LKM_DEBUG */
		} else {
			lkm_state = LKMS_LOADING_SYMS;
#ifdef LKM_DEBUG
			printf("LKM: LMLOADBUF (loaded)\n");
#endif	/* LKM_DEBUG */
		}
		curp->offset += loadbufp->cnt;
		break;

	case LMLOADSYMS:	/* Copy in; stateful, follows LMRESERV*/
		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		loadbufp = (struct lmc_loadbuf *)data;
		i = loadbufp->cnt;
		if ((lkm_state != LKMS_LOADING &&
		    lkm_state != LKMS_LOADING_SYMS)
		    || i < 0
		    || i > MODIOBUF
		    || i > curp->sym_size - curp->sym_offset) {
			error = ENOMEM;
			break;
		}

		/* copy in buffer full of data*/
		error = copyin(loadbufp->data, curp->syms +
		    curp->sym_offset, i);
		if (error)
			break;

		if ((curp->sym_offset + i) < curp->sym_size) {
			lkm_state = LKMS_LOADING_SYMS;
#ifdef LKM_DEBUG
			printf( "LKM: LMLOADSYMS (loading @ %d of %d, i = %d)\n",
			curp->sym_offset, curp->sym_size, i);
#endif	/* LKM_DEBUG*/
		} else {
			lkm_state = LKMS_LOADED;
#ifdef LKM_DEBUG
			printf( "LKM: LMLOADSYMS (loaded)\n");
#endif	/* LKM_DEBUG*/
		}
		curp->sym_offset += i;
		break;

	case LMUNRESRV:		/* discard reserved pages for a module */
		if (securelevel > 0)
			return EPERM;

		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		lkmunreserve();	/* coerce state to LKM_IDLE */
#ifdef LKM_DEBUG
		printf("LKM: LMUNRESERV\n");
#endif	/* LKM_DEBUG */
		break;

	case LMREADY:		/* module loaded: call entry */
		if (securelevel > 0)
			return EPERM;

		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		switch (lkm_state) {
		case LKMS_LOADED:
			break;
		case LKMS_LOADING:
		case LKMS_LOADING_SYMS:
			if (curp->size - curp->offset > 0)
			    /* The remainder must be bss, so we clear it */
			    bzero((caddr_t)curp->area + curp->offset,
				  curp->size - curp->offset);
			break;
		default:

#ifdef LKM_DEBUG
			printf("lkm_state is %02x\n", lkm_state);
#endif	/* LKM_DEBUG */
			return ENXIO;
		}

		curp->entry = (int (*) __P((struct lkm_table *, int, int)))
		    (*((long *) (data)));

#ifdef LKM_DEBUG
		printf("LKM: call entrypoint %x\n", curp->entry);
#endif /* LKM_DEBUG */
		/* call entry(load)... (assigns "private" portion) */
		error = (*(curp->entry))(curp, LKM_E_LOAD, curp->ver);
		if (error) {
			/*
			 * Module may refuse loading or may have a
			 * version mismatch...
			 */
			lkm_state = LKMS_UNLOADING;	/* for lkmunreserve */
			lkmunreserve();			/* free memory */
			lkmfree(curp);			/* free slot */
			break;
		}

#ifdef LKM_DEBUG
		printf("LKM: LMREADY, id=%d, dev=%d\n", curp->id, curp->private.lkm_any->lkm_offset);
#endif	/* LKM_DEBUG */
#ifdef DDB
		if (curp->syms && curp->sym_offset >= curp->sym_size) {
			curp->sym_id = db_add_symbol_table(curp->syms,
			    curp->syms + curp->sym_symsize,
			    curp->private.lkm_any->lkm_name,
			    curp->syms, curp->syms + curp->sym_size);
			printf("DDB symbols added: %ld bytes\n",
			    curp->sym_symsize);
		}
#endif /* DDB */
		curp->refcnt++;
		lkm_state = LKMS_IDLE;
		break;

	case LMUNLOAD:		/* unload a module */
		if (securelevel > 0)
			return EPERM;

		if ((flag & FWRITE) == 0) /* only allow this if writing */
			return EPERM;

		unloadp = (struct lmc_unload *)data;

		if ((curp = lkmlookup(unloadp->id, unloadp->name, &error)) == NULL)
			break; /* error set in lkmlookup */

		/* call entry(unload) */
		if ((*(curp->entry))(curp, LKM_E_UNLOAD, curp->ver)) {
			error = EBUSY;
			break;
		}

		lkm_state = LKMS_UNLOADING;	/* non-idle for lkmunreserve */
		lkmunreserve();			/* free memory */
		lkmfree(curp);			/* free slot */
		break;

	case LMSTAT:		/* stat a module by id/name */
		/* allow readers and writers to stat */

		statp = (struct lmc_stat *)data;

		if ((curp = lkmlookup(statp->id, statp->name, &error)) == NULL)
			break; /* error set in lkmlookup */

		/*
		 * Copy out stat information for this module...
		 */
		statp->id	= curp->id;
		statp->offset	= curp->private.lkm_any->lkm_offset;
		statp->type	= curp->private.lkm_any->lkm_type;
		statp->area	= curp->area;
		statp->size	= curp->size / CLBYTES;
		statp->private	= (unsigned long)curp->private.lkm_any;
		statp->ver	= curp->private.lkm_any->lkm_ver;
		copyoutstr(curp->private.lkm_any->lkm_name, 
			  statp->name, MAXLKMNAME, NULL);

		break;

	default:		/* bad ioctl()... */
		error = ENOTTY;
		break;
	}

	return (error);
}

/*
 * Acts like "nosys" but can be identified in sysent for dynamic call
 * number assignment for a limited number of calls.
 *
 * Place holder for system call slots reserved for loadable modules.
 */
int
sys_lkmnosys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	return (sys_nosys(p, v, retval));
}

/*
 * Acts like "enodev", but can be identified in cdevsw and bdevsw for
 * dynamic driver major number assignment for a limited number of
 * drivers.
 *
 * Place holder for device switch slots reserved for loadable modules.
 */
int
lkmenodev()
{

	return (enodev());
}

/*
 * A placeholder function for load/unload/stat calls; simply returns zero.
 * Used where people don't wnat tp specify a special function.
 */
int
lkm_nofunc(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{

	return (0);
}

int
lkmexists(lkmtp)
	struct lkm_table *lkmtp;
{
	register struct lkm_table *p;

	/*
	 * see if name exists...
	 */
	for (p = lkmods.tqh_first; p != NULL ; p = p->list.tqe_next)
		if (!strcmp(lkmtp->private.lkm_any->lkm_name,
			p->private.lkm_any->lkm_name) && p->refcnt)
			return (1);		/* already loaded... */

	return (0);		/* module not loaded... */
}

/*
 * For the loadable system call described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_syscall(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_syscall *args = lkmtp->private.lkm_syscall;
	int i;
	int error = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);

		if ((i = args->lkm_offset) == -1) {	/* auto */
			/*
			 * Search the table looking for a slot...
			 */
			for (i = 0; i < SYS_MAXSYSCALL; i++)
				if (sysent[i].sy_call == sys_lkmnosys)
					break;		/* found it! */
			/* out of allocable slots? */
			if (i == SYS_MAXSYSCALL) {
				error = ENFILE;
				break;
			}
		} else {				/* assign */
			if (i < 0 || i >= SYS_MAXSYSCALL) {
				error = EINVAL;
				break;
			}
		}

		/* save old */
		bcopy(&sysent[i], &args->lkm_oldent, sizeof(struct sysent));

		/* replace with new */
		bcopy(args->lkm_sysent, &sysent[i], sizeof(struct sysent));

		/* done! */
		args->lkm_offset = i;	/* slot in sysent[] */

		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		/* replace current slot contents with old contents */
		bcopy(&args->lkm_oldent, &sysent[i], sizeof(struct sysent));

		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return (error);
}

/*
 * For the loadable virtual file system described by the structure pointed
 * to by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_vfs(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	int error = 0;
	struct vfsconf *vfs = lkmtp->private.lkm_vfs->lkm_vfsconf;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);
		error = vfs_register(vfs);
		break;

	case LKM_E_UNLOAD:
		error = vfs_unregister(vfs);
		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return (error);
}

/*
 * For the loadable device driver described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_dev(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_dev *args = lkmtp->private.lkm_dev;
	int i;
	int error = 0;
	extern int nblkdev, nchrdev;	/* from conf.c */

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);

		switch(args->lkm_devtype) {
		case LM_DT_BLOCK:
			if ((i = args->lkm_offset) == -1) {	/* auto */
				/*
				 * Search the table looking for a slot...
				 */
				for (i = 0; i < nblkdev; i++)
					if (bdevsw[i].d_open == 
					    (dev_type_open((*))) lkmenodev)
						break;		/* found it! */
				/* out of allocable slots? */
				if (i == nblkdev) {
					error = ENFILE;
					break;
				}
			} else {				/* assign */
				if (i < 0 || i >= nblkdev) {
					error = EINVAL;
					break;
				}
			}

			/* save old */
			bcopy(&bdevsw[i], &args->lkm_olddev.bdev, sizeof(struct bdevsw));

			/* replace with new */
			bcopy(args->lkm_dev.bdev, &bdevsw[i], sizeof(struct bdevsw));

			/* done! */
			args->lkm_offset = i;	/* slot in bdevsw[] */
			break;

		case LM_DT_CHAR:
			if ((i = args->lkm_offset) == -1) {	/* auto */
				/*
				 * Search the table looking for a slot...
				 */
				for (i = 0; i < nchrdev; i++)
					if (cdevsw[i].d_open ==
					    (dev_type_open((*))) lkmenodev)
						break;		/* found it! */
				/* out of allocable slots? */
				if (i == nchrdev) {
					error = ENFILE;
					break;
				}
			} else {				/* assign */
				if (i < 0 || i >= nchrdev) {
					error = EINVAL;
					break;
				}
			}

			/* save old */
			bcopy(&cdevsw[i], &args->lkm_olddev.cdev, sizeof(struct cdevsw));

			/* replace with new */
			bcopy(args->lkm_dev.cdev, &cdevsw[i], sizeof(struct cdevsw));

			/* done! */
			args->lkm_offset = i;	/* slot in cdevsw[] */

			break;

		default:
			error = ENODEV;
			break;
		}
		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		switch(args->lkm_devtype) {
		case LM_DT_BLOCK:
			/* replace current slot contents with old contents */
			bcopy(&args->lkm_olddev.bdev, &bdevsw[i], sizeof(struct bdevsw));
			break;

		case LM_DT_CHAR:
			/* replace current slot contents with old contents */
			bcopy(&args->lkm_olddev.cdev, &cdevsw[i], sizeof(struct cdevsw));
			break;

		default:
			error = ENODEV;
			break;
		}
		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return (error);
}

#ifdef STREAMS
/*
 * For the loadable streams module described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_strmod(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_strmod *args = lkmtp->private.lkm_strmod;
	int i;
	int error = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);
		break;

	case LKM_E_UNLOAD:
		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return (error);
}
#endif	/* STREAMS */

/*
 * For the loadable execution class described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_exec(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_exec *args = lkmtp->private.lkm_exec;
	int i;
	int error = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);

		if ((i = args->lkm_offset) == -1) {	/* auto */
			/*
			 * Search the table looking for a slot...
			 */
			for (i = 0; i < nexecs; i++)
				if (execsw[i].es_check == NULL)
					break;		/* found it! */
			/* out of allocable slots? */
			if (i == nexecs) {
				error = ENFILE;
				break;
			}
		} else {				/* assign */
			if (i < 0 || i >= nexecs) {
				error = EINVAL;
				break;
			}
		}

		/* save old */
		bcopy(&execsw[i], &args->lkm_oldexec, sizeof(struct execsw));

		/* replace with new */
		bcopy(args->lkm_exec, &execsw[i], sizeof(struct execsw));

		/* realize need to recompute max header size */
		exec_maxhdrsz = 0;

		/* done! */
		args->lkm_offset = i;	/* slot in execsw[] */

		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		/* replace current slot contents with old contents */
		bcopy(&args->lkm_oldexec, &execsw[i], sizeof(struct execsw));

		/* realize need to recompute max header size */
		exec_maxhdrsz = 0;

		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return (error);
}

/*
 * This code handles the per-module type "wiring-in" of loadable modules
 * into existing kernel tables.  For "LM_MISC" modules, wiring and unwiring
 * is assumed to be done in their entry routines internal to the module
 * itself.
 */
int
lkmdispatch(lkmtp, cmd)
	struct lkm_table *lkmtp;	
	int cmd;
{
	int error = 0;		/* default = success */

#ifdef LKM_DEBUG
	printf("lkmdispatch: %x %d\n", lkmtp, cmd);
#endif /* LKM_DEBUG */

	switch(lkmtp->private.lkm_any->lkm_type) {
	case LM_SYSCALL:
		error = _lkm_syscall(lkmtp, cmd);
		break;

	case LM_VFS:
		error = _lkm_vfs(lkmtp, cmd);
		break;

	case LM_DEV:
		error = _lkm_dev(lkmtp, cmd);
		break;

#ifdef STREAMS
	case LM_STRMOD:
	    {
		struct lkm_strmod *args = lkmtp->private.lkm_strmod;
	    }
		break;

#endif	/* STREAMS */

	case LM_EXEC:
		error = _lkm_exec(lkmtp, cmd);
		break;

	case LM_MISC:	/* ignore content -- no "misc-specific" procedure */
		break;

	default:
		error = ENXIO;	/* unknown type */
		break;
	}

	return (error);
}
