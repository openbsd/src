/*	$OpenBSD: kern_lkm.c,v 1.45 2007/11/03 22:23:35 mikeb Exp $	*/
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

#include <uvm/uvm_extern.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#endif

/* flags */
#define	LKM_ALLOC		0x01
#define	LKM_WANT		0x02
#define	LKM_INIT		0x04

#define	LKMS_IDLE		0x00
#define	LKMS_RESERVED		0x01
#define	LKMS_LOADING		0x02
#define	LKMS_LOADING_SYMS	0x03
#define	LKMS_LOADED		0x04
#define	LKMS_UNLOADING		0x08

struct vm_map *lkm_map = NULL;

static int lkm_v = 0;
static int lkm_state = LKMS_IDLE;

static TAILQ_HEAD(lkmods, lkm_table) lkmods;	/* table of loaded modules */
static struct lkm_table	*curp;		/* global for in-progress ops */

static struct lkm_table *lkmalloc(void);
static void lkmfree(struct lkm_table *);
static struct lkm_table *lkmlookup(int, char *, int *);
static void lkmunreserve(void);
static int _lkm_syscall(struct lkm_table *, int);
static int _lkm_vfs(struct lkm_table *, int);
static int _lkm_dev(struct lkm_table *, int);
static int _lkm_exec(struct lkm_table *, int);

void lkminit(void);
int lkmexists(struct lkm_table *);

void init_exec(void);

void
lkminit(void)
{

	/*
	 * If machine-dependent code hasn't initialized the lkm_map
	 * then just use kernel_map.
	 */
	if (lkm_map == NULL)
		lkm_map = kernel_map;
	TAILQ_INIT(&lkmods);
	lkm_v |= LKM_INIT;
}

/*ARGSUSED*/
int
lkmopen(dev_t dev, int flag, int devtype, struct proc *p)
{
	int error;

	if (minor(dev) != 0)
		return (ENXIO);

	if (!(lkm_v & LKM_INIT))
		lkminit();

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
		error = tsleep(&lkm_v, TTIPRI|PCATCH, "lkmopn", 0);
		if (error)
			return (error);	/* leave LKM_WANT set -- no problem */
	}
	lkm_v |= LKM_ALLOC;

	return (0);		/* pseudo-device open */
}

/*
 * Alocates new LKM table entry, fills module id, inserts in the list.
 * Returns NULL on failure.
 *
 */
static struct lkm_table *
lkmalloc(void)
{
	struct lkm_table *p, *ret = NULL;
	int id = 0;

	ret = malloc(sizeof(*ret), M_DEVBUF, M_WAITOK);
	ret->refcnt = ret->depcnt = 0;
	ret->sym_id = -1;
	/* 
	 * walk the list finding the first free id. as long as the list is
	 * kept sorted this is not too inefficient, which is why we insert in
	 * order below.
 	 */
	TAILQ_FOREACH(p, &lkmods, list) {
		if (id == p->id)
			id++;
		else
			break;
	}
	ret->id = id;
	if (p == NULL) /* either first or last entry */
		TAILQ_INSERT_TAIL(&lkmods, ret, list);
	else 
		TAILQ_INSERT_BEFORE(p, ret, list);

	return ret;
}

/*
 * Frees the slot, decreases the number of modules.
 */
static void
lkmfree(struct lkm_table *p)
{

	TAILQ_REMOVE(&lkmods, p, list);
	free(p, M_DEVBUF);
	curp = NULL;
}

struct lkm_table *
lkm_list(struct lkm_table *p)
{

	if (p == NULL)
		p = TAILQ_FIRST(&lkmods);
	else
		p = TAILQ_NEXT(p, list);

	return (p);
}

static struct lkm_table *
lkmlookup(int i, char *name, int *error)
{
	struct lkm_table *p = NULL;
	char istr[MAXLKMNAME];
	
	/* 
 	 * p being NULL here implies the list is empty, so any lookup is
 	 * invalid (name based or otherwise). Since the list of modules is
 	 * kept sorted by id, lowest to highest, the id of the last entry 
 	 * will be the highest in use. 
	 */ 
	p = TAILQ_LAST(&lkmods, lkmods);
	if (p == NULL || i > p->id) {
		*error = EINVAL;
		return NULL;
	}

	if (i < 0) {		/* unload by name */
		/*
		 * Copy name and lookup id from all loaded
		 * modules.  May fail.
		 */
	 	*error = copyinstr(name, istr, MAXLKMNAME-1, NULL);
		if (*error)
			return NULL;
		istr[MAXLKMNAME-1] = '\0';

		TAILQ_FOREACH(p, &lkmods, list) {
			if (!strcmp(istr, p->private.lkm_any->lkm_name))
				break;
		}
	} else 
		TAILQ_FOREACH(p, &lkmods, list) 
			if (i == p->id)
				break;
		
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
lkmunreserve(void)
{

	if (lkm_state == LKMS_IDLE)
		return;

#ifdef DDB
	if (curp && curp->sym_id != -1)
		db_del_symbol_table(curp->private.lkm_any->lkm_name);
#endif

	if (curp && curp->syms) {
		uvm_km_free(lkm_map, (vaddr_t)curp->syms, curp->sym_size);
		curp->syms = NULL;
	}

	/*
	 * Actually unreserve the memory
	 */
	if (curp && curp->area) {
		uvm_km_free(lkm_map, curp->area, curp->size);
		curp->area = 0;
	}
	lkm_state = LKMS_IDLE;
}

int
lkmclose(dev_t dev, int flag, int mode, struct proc *p)
{

	if (minor(dev) != 0)
		return (ENXIO);

	if (!(lkm_v & LKM_ALLOC))
		return (EBADF);

	/* do this before waking the herd... */
	if (curp != NULL && !curp->refcnt) {
		/*
		 * If we close before setting used, we have aborted
		 * by way of error or by way of close-on-exit from
		 * a premature exit of "modload".
		 */
		lkmunreserve();
		lkmfree(curp);
	}
	lkm_v &= ~LKM_ALLOC;
	wakeup(&lkm_v);	/* thundering herd "problem" here */

	return (0);
}

/*ARGSUSED*/
int
lkmioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	int error = 0;

	if (securelevel > 0) {
		switch (cmd) {
		case LMSTAT:
			break;
		default:
			return (EPERM);
		}
	}

	if (!(flags & FWRITE)) {
		switch (cmd) {
		case LMSTAT:
			break;
		default:
			return (EACCES);
		}
	}

	switch (cmd) {
	case LMRESERV:
	case LMRESERV_O: {
		struct lmc_resrv *resrvp = (struct lmc_resrv *)data;

		if ((curp = lkmalloc()) == NULL) {
			error = ENOMEM;	
			break;
		}
		curp->ver = (cmd == LMRESERV) ? LKM_VERSION : LKM_OLDVERSION;
		resrvp->slot = curp->id;	/* return slot */

		/*
		 * Get memory for module
		 */
		curp->size = resrvp->size;
		curp->area = uvm_km_zalloc(lkm_map, curp->size);
		curp->offset = 0;
		resrvp->addr = curp->area;

		if (cmd == LMRESERV && resrvp->sym_size) {
			curp->sym_size = resrvp->sym_size;
			curp->sym_symsize = resrvp->sym_symsize;
			curp->syms = (caddr_t)uvm_km_zalloc(lkm_map,
							    curp->sym_size);
			curp->sym_offset = 0;
			resrvp->sym_addr = curp->syms;
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
	}

	case LMLOADBUF: {
		struct lmc_loadbuf *loadbufp = (struct lmc_loadbuf *)data;

		if ((lkm_state != LKMS_RESERVED && lkm_state != LKMS_LOADING)
		    || loadbufp->cnt < 0
		    || loadbufp->cnt > MODIOBUF
		    || loadbufp->cnt > (curp->size - curp->offset)) {
			error = ENOMEM;
			break;
		}

		/* copy in buffer full of data */
		error = copyin(loadbufp->data, 
			(caddr_t)curp->area + curp->offset, loadbufp->cnt);
		if (error)
			break;

		if ((curp->offset + loadbufp->cnt) < curp->size)
			lkm_state = LKMS_LOADING;
		else
			lkm_state = LKMS_LOADING_SYMS;

		curp->offset += loadbufp->cnt;
		break;
	}

	case LMLOADSYMS: {
		struct lmc_loadbuf *loadbufp = (struct lmc_loadbuf *)data;

		if ((lkm_state != LKMS_LOADING &&
		    lkm_state != LKMS_LOADING_SYMS)
		    || loadbufp->cnt < 0
		    || loadbufp->cnt > MODIOBUF
		    || loadbufp->cnt > (curp->sym_size - curp->sym_offset)) {
			error = ENOMEM;
			break;
		}

		/* copy in buffer full of data*/
		error = copyin(loadbufp->data, curp->syms +
		    curp->sym_offset, loadbufp->cnt);
		if (error)
			break;

		if ((curp->sym_offset + loadbufp->cnt) < curp->sym_size)
			lkm_state = LKMS_LOADING_SYMS;
		else
			lkm_state = LKMS_LOADED;

		curp->sym_offset += loadbufp->cnt;
		break;
	}

	case LMUNRESRV:
		lkmunreserve();
		if (curp)
			lkmfree(curp);
		break;

	case LMREADY:
		switch (lkm_state) {
		case LKMS_LOADED:
			break;
		case LKMS_LOADING:
		case LKMS_LOADING_SYMS:
			if ((curp->size - curp->offset) > 0)
			    /* The remainder must be bss, so we clear it */
			    bzero((caddr_t)curp->area + curp->offset,
				  curp->size - curp->offset);
			break;
		default:
			return (ENXIO);
		}

		curp->entry = (int (*)(struct lkm_table *, int, int))
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
		printf("LKM: LMREADY, id=%d, dev=%d\n", curp->id,
		    curp->private.lkm_any->lkm_offset);
#endif	/* LKM_DEBUG */

#ifdef DDB
		if (curp->syms && curp->sym_offset >= curp->sym_size) {
			curp->sym_id = db_add_symbol_table(curp->syms,
			    curp->syms + curp->sym_symsize,
			    curp->private.lkm_any->lkm_name,
			    curp->syms);
			printf("DDB symbols added: %ld bytes\n",
			    curp->sym_symsize);
		}
#endif /* DDB */

		curp->refcnt++;
		lkm_state = LKMS_IDLE;
		break;

	case LMUNLOAD: {
		struct lmc_unload *unloadp = (struct lmc_unload *)data;

		curp = lkmlookup(unloadp->id, unloadp->name, &error);
		if (curp == NULL)
			break;

		/* call entry(unload) */
		if ((*(curp->entry))(curp, LKM_E_UNLOAD, curp->ver)) {
			error = EBUSY;
			break;
		}

		lkm_state = LKMS_UNLOADING;	/* non-idle for lkmunreserve */
		lkmunreserve();			/* free memory */
		lkmfree(curp);			/* free slot */
		break;
	}

	case LMSTAT: {
		struct lmc_stat *statp = (struct lmc_stat *)data;

		if ((curp = lkmlookup(statp->id, statp->name, &error)) == NULL)
			break;

		if ((error = (*curp->entry)(curp, LKM_E_STAT, curp->ver)))
			break;

		/*
		 * Copy out stat information for this module...
		 */
		statp->id	= curp->id;
		statp->offset	= curp->private.lkm_any->lkm_offset;
		statp->type	= curp->private.lkm_any->lkm_type;
		statp->area	= curp->area;
		statp->size	= curp->size / PAGE_SIZE;
		statp->private	= (unsigned long)curp->private.lkm_any;
		statp->ver	= curp->private.lkm_any->lkm_ver;
		copyoutstr(curp->private.lkm_any->lkm_name, 
			  statp->name, MAXLKMNAME, NULL);

		break;
	}

	default:
		error = ENODEV;
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
sys_lkmnosys(struct proc *p, void *v, register_t *retval)
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
lkmenodev(void)
{

	return (enodev());
}

/*
 * A placeholder function for load/unload/stat calls; simply returns zero.
 * Used where people don't want to specify a special function.
 */
int
lkm_nofunc(struct lkm_table *lkmtp, int cmd)
{

	return (0);
}

int
lkmexists(struct lkm_table *lkmtp)
{
	struct lkm_table *p;

	TAILQ_FOREACH(p, &lkmods, list) {
		if (!strcmp(lkmtp->private.lkm_any->lkm_name,
		     p->private.lkm_any->lkm_name) && p->refcnt)
			return (1);
	}
	return (0);
}

/*
 * For the loadable system call described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_syscall(struct lkm_table *lkmtp, int cmd)
{
	struct lkm_syscall *args = lkmtp->private.lkm_syscall;
	int i;
	int error = 0;

	switch (cmd) {
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
_lkm_vfs(struct lkm_table *lkmtp, int cmd)
{
	int error = 0;
	struct lkm_vfs *args = lkmtp->private.lkm_vfs;

	switch (cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);
		error = vfs_register(args->lkm_vfsconf);
		break;

	case LKM_E_UNLOAD:
		error = vfs_unregister(args->lkm_vfsconf);
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
_lkm_dev(struct lkm_table *lkmtp, int cmd)
{
	struct lkm_dev *args = lkmtp->private.lkm_dev;
	int i;
	int error = 0;

	switch (cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return (EEXIST);

		switch (args->lkm_devtype) {
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
			bcopy(&bdevsw[i], &args->lkm_olddev.bdev,
			    sizeof(struct bdevsw));

			/* replace with new */
			bcopy(args->lkm_dev.bdev, &bdevsw[i],
			    sizeof(struct bdevsw));

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
			bcopy(&cdevsw[i], &args->lkm_olddev.cdev,
			    sizeof(struct cdevsw));

			/* replace with new */
			bcopy(args->lkm_dev.cdev, &cdevsw[i],
			    sizeof(struct cdevsw));

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

		switch (args->lkm_devtype) {
		case LM_DT_BLOCK:
			/* replace current slot contents with old contents */
			bcopy(&args->lkm_olddev.bdev, &bdevsw[i],
			    sizeof(struct bdevsw));
			break;

		case LM_DT_CHAR:
			/* replace current slot contents with old contents */
			bcopy(&args->lkm_olddev.cdev, &cdevsw[i],
			    sizeof(struct cdevsw));
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

/*
 * For the loadable execution class described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_exec(struct lkm_table *lkmtp, int cmd)
{
	struct lkm_exec *args = lkmtp->private.lkm_exec;
	int i;
	int error = 0;

	switch (cmd) {
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

		/* need to recompute max header size */
		init_exec();

		/* done! */
		args->lkm_offset = i;	/* slot in execsw[] */

		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		/* replace current slot contents with old contents */
		bcopy(&args->lkm_oldexec, &execsw[i], sizeof(struct execsw));

		/* need to recompute max header size */
		init_exec();

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
lkmdispatch(struct lkm_table *lkmtp, int cmd)
{
	int error =  0;

#ifdef LKM_DEBUG
	printf("lkmdispatch: %x %d\n", lkmtp, cmd);
#endif /* LKM_DEBUG */

	switch (lkmtp->private.lkm_any->lkm_type) {
	case LM_SYSCALL:
		error = _lkm_syscall(lkmtp, cmd);
		break;

	case LM_VFS:
		error = _lkm_vfs(lkmtp, cmd);
		break;

	case LM_DEV:
		error = _lkm_dev(lkmtp, cmd);
		break;

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
