/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $arla: vld.c,v 1.63 2003/02/15 14:57:32 map Exp $ */

#include <sys/types.h>
#include <stdio.h>

#include <assert.h>

#include <rx/rx.h>

#include <vldb.h>
#include <pts.h>

#include <voldb.h>
#include <vld.h>
#include <vld_ops.h>

#include <vstatus.h>

#include <svol.h>
#include <fvol.h>

#include <mdir.h>

#include <err.h>

#include <hash.h>
#include <list.h>

#include <mlog.h>
#include <mdebug.h>

#include <errno.h>

/*
 * Local define and limitations
 */

int vld_storestatus_to_ent (struct voldb_entry *e, const AFSStoreStatus *ss,
			    struct msec *sec);
static int vld_ent_to_fetchstatus  (struct volume_handle *vol,
				    struct voldb_entry *e,
				    struct mnode *n);
static void vld_set_author(struct voldb_entry *e, struct msec *m);
static int vld_update_volsize (volume_handle *vol, int diff);
static int vld_check_quota (volume_handle *vol, int diff);

/*
 * Local variables
 */

/* hashtable of volumes */
static Hashtab *volume_htab = NULL;
static int volume_htab_sz = 1024;

/* lru for db in use */
static List *db_lru = NULL;

/* list of volumes to be able to do sysbackup and salvage */
static List *vol_list = NULL;

/* volume types */
vol_op *backstoretypes[VLD_MAX_BACKSTORE_TYPES];

/*
 * Useful macro's and short functions
 */

#define VLD_VALID_BACKSTORETYPE(backstoretype) (!(backstoretype >= 0 && backstoretype < VLD_MAX_BACKSTORE_TYPES && backstoretypes[backstoretype] != NULL))

#define ENTRY_DISK_SIZE 1
#define DIR_DISK_SIZE 2

static int 
VLD_VALID_VOLNAME(const char *volname) 
{
    while (volname && *volname) {
	if (!((*volname >= 'a' && *volname <= 'z') ||
	      (*volname >= 'A' && *volname <= 'Z') ||
	      (*volname >= '0' && *volname <= '9') ||
	      *volname == '.'))
	    return *volname;
	volname++;
    }
    return 0;
}


/*
 * Translate to real-name
 */

const char *
vld_backstoretype_name (int32_t backstoretype)
{
    if (VLD_VALID_BACKSTORETYPE(backstoretype))
	return "UKWN";

    return backstoretypes[backstoretype]->name;
}

/*
 * Bootstrap
 */

int
vld_boot (void)
{
    memset(backstoretypes, 0, sizeof (backstoretypes));
    backstoretypes[VLD_SVOL] = &svol_volume_ops;
    backstoretypes[VLD_FVOL] = &fvol_volume_ops;
    return 0;
}

/*
 * Free volume `vol'
 */

void
vld_free (volume_handle *vol)
{
    if (--vol->ref == 0) {
	VOLOP_FREE(vol);
	if (vol->flags.voldbp) {
	    voldb_close (VLD_VOLH_DIR(vol));
	    voldb_close (VLD_VOLH_FILE(vol));
	    vol->flags.voldbp = FALSE;
	} else {
	    assert (VLD_VOLH_DIR(vol) == NULL);
	    assert (VLD_VOLH_FILE(vol) == NULL);
	}
	dp_free (vol->dp);
	free (vol);
    }    
}

/*
 * Ref `vol'
 */

void
vld_ref (volume_handle *vol)
{
    assert (vol->ref);
    vol->ref++;
}

/*
 * create a `vol' from the bootstrap infomation in `vs'.
 */

static int
vstatus2volume_handle (vstatus *vs, struct dp_part *dp, volume_handle **vol)
{
    volume_handle *v;

    assert (vs && dp && vol);

    v = malloc (sizeof(*v));
    if (v == NULL)
	return ENOMEM;

    memset (v, 0, sizeof(*v));
    v->ref = 1;
    v->vol  = vs->volid;
    dp_ref (dp);
    v->dp   = dp;
    memcpy (&v->sino, &vs->volinfoinode, sizeof(vs->volinfoinode));
    memcpy (&v->dino, &vs->dirinode, sizeof(vs->dirinode));
    memcpy (&v->fino, &vs->fileinode, sizeof(vs->fileinode));
    v->flags.infop = FALSE;
    v->flags.voldbp = FALSE;
    v->flags.offlinep = FALSE;
    v->voldbtype = vs->voldbtype;
    v->type = vs->bstype;

    *vol = v;

    return 0;
}

/*
 * Read in all partitions and register all volumes.
 */

static void
register_vols_cb (void *data, int fd)
{
    struct dp_part *dp = (struct dp_part *)data;
    vstatus vs;
    int ret;
    volume_handle *vol;

    ret = vstatus_read (fd, &vs);
    if (ret)
	return;

    ret = vstatus2volume_handle (&vs, dp, &vol);
    if (ret) {
	mlog_log (MDEBERROR,
		  "register_vols_cb: failed to convert vstatus");
	return;
    }

    ret = VOLOP_OPEN(vol->type, dp, vol->vol, 
		     VOLOP_NOFLAGS, &vol->data);
    if (ret) {
	mlog_log (MDEBERROR,
		  "register_vols_cb: failed to open volume");
	vol->flags.attacherr = TRUE;
    } else {
	vol->flags.attacherr = FALSE;
    }
    vol->flags.offlinep = TRUE;
    vol->flags.salvaged = FALSE;
    vol->li = listaddtail (vol_list, vol);
    if (vol->li == NULL)
	errx (1, "register_vols_cb: listaddtail failed");

    hashtabadd (volume_htab, vol);
    
    return;
}

/*
 *
 */

static int
volume_cmp (void *ptr1, void *ptr2)
{
    volume_handle *v1 = (volume_handle *) ptr1;
    volume_handle *v2 = (volume_handle *) ptr2;

    return v1->vol - v2->vol;
}

/*
 *
 */

static unsigned
volume_hash (void *ptr)
{
    volume_handle *v = (volume_handle *) ptr;

    return v->vol;
}

/*
 *
 */

int
vld_init (void)
{
    struct dp_part *dp;
    int ret, partnum, i;
    

    db_lru = listnew();
    if (db_lru == NULL)
	errx (1, "vld_init: db_lru == NULL");

    for (i = 0; i < 100 /* XXX */ ; i++)
	listaddhead (db_lru, NULL);

    vol_list = listnew();
    if (vol_list == NULL)
	errx (1, "vld_init: vol_list == NULL");

    volume_htab = hashtabnew(volume_htab_sz, volume_cmp, volume_hash);
    if (volume_htab == NULL)
	errx (1, "vld_init: volume_htab == NULL");

    for (partnum = 0; partnum < 'z'-'a'; partnum++) {

	ret = dp_create (partnum , &dp);
	if (ret) {
	    warnx ("vld_init: dp_create(%d) returned %d", partnum, ret);
	    continue;
	}
	
	ret = dp_findvol (dp, register_vols_cb, dp);
	if (ret)
	    warnx ("vld_init: dp_findvol returned: %d", ret);

	dp_free (dp);
    }	
    return 0;
}

struct iter_vol_s {
    int (*func) (volume_handle *vol, void *arg);
    void *arg;
};

static int 
iter_vol (List *list, Listitem *li, void *arg)
{
    struct iter_vol_s *vof = (struct iter_vol_s *)arg;
    volume_handle *vol = listdata (li);
    int ret;
    
    vld_ref (vol);    
    ret = (vof->func) (vol, vof->arg);
    vld_free (vol);
    return ret;
}

void
vld_iter_vol (int (*func)(volume_handle *vol, void *arg), void *arg)
{
    struct iter_vol_s vof;

    vof.func = func;
    vof.arg = arg;

    listiter (vol_list, iter_vol, &vof);
}

/*
 *
 */

int
vld_create_entry (volume_handle *vol, struct mnode *parent, AFSFid *child,
		  int type, const AFSStoreStatus *ss, struct mnode **ret_n,
		  struct msec *m)
{
    struct voldb_entry e;
    onode_opaque child_ino;
    onode_opaque dummy; /* used when removing when failed */
    int ret;
    struct mnode *n;
    uint32_t real_mnode, unique;
    struct voldb *db;
    int (*convert_local2afs)(int32_t);
    node_type ntype;
    int space_needed = ENTRY_DISK_SIZE;

    switch (type) {
    case TYPE_DIR:
	db = VLD_VOLH_DIR(vol);
	convert_local2afs = dir_local2afs;
	ntype = NODE_DIR;
	space_needed += DIR_DISK_SIZE;
	break;
    case TYPE_FILE:
    case TYPE_LINK:
	db = VLD_VOLH_FILE(vol);
	convert_local2afs = file_local2afs;
	ntype = NODE_REG;
	break;
    default:
	errx(-1, "vld_create_entry, bad type\n");
    }

    ret = vld_check_quota (vol, space_needed);
    if (ret)
	return ret;

    ret = voldb_new_entry (db, &real_mnode, &unique);
    if (ret)
	return ret;

    ret = voldb_get_entry (db, real_mnode, &e);
    if (ret)
	goto out_bad_icreate;

    e.type = type;
    
    child->Volume = parent->fid.Volume;
    child->Vnode = (convert_local2afs)(real_mnode);
    child->Unique = unique;

    ret = mnode_find (child, &n);
    if (ret)
	goto out_bad_icreate;

    ret = VOLOP_ICREATE(vol, &child_ino, ntype, n);
    if (ret) {
	mnode_free (n, FALSE);
	goto out_bad_icreate;
    }

    if (type == TYPE_DIR) {
	AFSFid dot, dot_dot;

	dot_dot.Volume = dot.Volume = vol->vol;
	dot_dot.Vnode = parent->fid.Vnode;
	dot_dot.Unique = parent->fid.Unique;
	dot.Vnode = child->Vnode;
	dot.Unique = child->Unique;
	
	ret = mdir_mkdir (n, dot, dot_dot);
	if (ret) {
	    ret = EIO;
	    goto out_bad_put;
	}
    }

    if (type == TYPE_DIR) {
	e.u.dir.ino = child_ino;
	e.u.dir.FileType = type;
	e.u.dir.LinkCount = 2;
	e.u.dir.DataVersion = 0;
	e.u.dir.ParentVnode = parent->fid.Vnode;
	e.u.dir.ParentUnique = parent->fid.Unique;
	memcpy (&e.u.dir.negacl, &parent->e.u.dir.negacl, 
		sizeof(parent->e.u.dir.negacl));
	memcpy (&e.u.dir.acl, &parent->e.u.dir.acl, 
		sizeof(parent->e.u.dir.acl));
    } else {
	e.u.file.ino = child_ino;
	e.u.file.FileType = type;
	e.u.file.LinkCount = 1;
	e.u.file.DataVersion = 0;
	e.u.file.ParentVnode = parent->fid.Vnode;
	e.u.file.ParentUnique = parent->fid.Unique;
    }

    voldb_update_time(&e, time(NULL));
    vld_set_author(&e, m);
    
    ret = vld_storestatus_to_ent (&e, ss, m);
    if (ret)
	goto out_bad_put;

    ret = voldb_put_entry (db, real_mnode, &e);
    if (ret) {
	ret = EIO;
	goto out_bad_put;
    }

    if (type == TYPE_DIR) {
	ret = voldb_put_acl (db, real_mnode, &e.u.dir);
	if (ret)
	    goto out_bad_put;
    }


    if (ret_n) {
	if (n->flags.ep == FALSE) {
	    ret = voldb_get_entry (db, real_mnode, &n->e); 
	    if (ret)
		goto out_bad_put;
	    n->flags.ep = TRUE;
	}

	if (m && (m->flags & VOLOP_GETSTATUS) == VOLOP_GETSTATUS) {
	    assert(n->flags.fdp);
	    	    
	    ret = vld_ent_to_fetchstatus(vol, &e, n);
	    if (ret)
		goto out_bad_put;
	}

	ret = vld_update_volsize (vol, ENTRY_DISK_SIZE + n->e.u.dir.Length/1024);
	if (ret)
	    goto out_bad_put;

	*ret_n = n;
    } else
	mnode_free (n, FALSE);
    
    return 0;

 out_bad_put:
    mnode_free (n, TRUE);
    VOLOP_IUNLINK(vol, &child_ino);
 out_bad_icreate:
    voldb_del_entry (db, real_mnode, &dummy);

    return ret;
}

/*
 *
 */

int
vld_set_onode (volume_handle *vol, int32_t vno, onode_opaque *new,
	       onode_opaque *old)
{
    int32_t real_mnode;
    struct voldb_entry e;
    struct voldb *db;
    onode_opaque *onode;
    int ret;
    
    assert (vol && new);

    if (afs_dir_p (vno)) {
	real_mnode = dir_afs2local(vno);
	db = VLD_VOLH_DIR(vol);
	onode = &e.u.dir.ino;
    } else {
	real_mnode = file_afs2local(vno);
	db = VLD_VOLH_FILE(vol);
	onode = &e.u.file.ino;
    }

    ret = voldb_get_entry (db, real_mnode, &e);
    if (ret)
	return ret;
    
    if (old)
	*old = *onode;

    *onode = *new;
    
    ret = voldb_put_entry (db, real_mnode, &e);
    if (ret)
	return ret;

    return 0;
}

/*
 *
 */

int
vld_adjust_linkcount (volume_handle *vol, struct mnode *n, int adjust)
{
    int32_t real_mnode;
    struct voldb *db;
    int32_t *LinkCount;
    int32_t vno;
    int ret;

    assert (vol && n);
    
    vno = n->fid.Vnode;
    if (afs_dir_p (vno)) {
	real_mnode = dir_afs2local(vno);
	db = VLD_VOLH_DIR(vol);
	LinkCount = &n->e.u.dir.LinkCount;
    } else {
	real_mnode = file_afs2local(vno);
	db = VLD_VOLH_FILE(vol);
	LinkCount = &n->e.u.file.LinkCount;
    }

    if (n->flags.ep == FALSE) {
	ret = voldb_get_entry (db, real_mnode, &n->e);
	if (ret)
	    return ret;
    }

    *LinkCount += adjust;
    n->fs.LinkCount += adjust;
    
    assert(*LinkCount >= 0);

    if (*LinkCount == 0) {
	ret = vld_remove_node(vol, n);
    } else {
	/* XXX is this necessary? */
	ret = voldb_put_entry (db, real_mnode, &n->e);
	if (ret)
	    return ret;
	
	n->flags.ep = TRUE;
    }

    return ret;
}

static struct trans *transactions[MAX_TRANSACTIONS] = {NULL};
static int32_t perm_tid = 1;


int
vld_create_trans(int32_t partition, int32_t volume, int32_t *trans)
{
    time_t now = time(NULL);
    int i;
    int ret;

    ret = vld_check_busy(volume, partition);
    if (ret)
	return ret;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] == NULL)
	    break;
    }
    if (transactions[i] == NULL) {
	transactions[i] = malloc(sizeof(struct trans));
	if (transactions[i] == NULL)
	    return ENOMEM;
	*trans = perm_tid++;
	transactions[i]->tid = *trans; /* XXX */
	transactions[i]->time = now;
	transactions[i]->creationTime = now;
	transactions[i]->returnCode = 0;
	transactions[i]->volume = NULL;
	transactions[i]->volid = volume;
	transactions[i]->partition = partition;
	transactions[i]->refCount = 1;
	transactions[i]->iflags = 0;
	transactions[i]->vflags = 0;
	transactions[i]->tflags = 0;
	transactions[i]->incremental = 0;
	return 0;
    } else
	return ENOMEM;
}

int
vld_get_trans(int32_t transid, struct trans **trans)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] && transactions[i]->tid == transid) {
	    *trans = transactions[i];
	    transactions[i]->refCount++;
	    return 0;
	}
    }
    return EINVAL;    
}

int
vld_put_trans(struct trans *trans)
{
    trans->refCount--;
    if (trans->refCount < 1)
	free(trans);
    return 0;
}

int
vld_verify_trans(int32_t trans)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] && transactions[i]->tid == trans)
	    return 0;
    }
    return ENOENT;
}

int
vld_end_trans(int32_t trans, int32_t *rcode)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] && transactions[i]->tid == trans) {
	    if (rcode)
		*rcode = transactions[i]->returnCode;
	    vld_put_trans(transactions[i]);
	    transactions[i] = NULL;
	    return 0;
	}
    }
    return ENOENT;
}

int
vld_trans_set_iflags(int32_t trans, int32_t iflags)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] && transactions[i]->tid == trans) {
	    transactions[i]->iflags = iflags;
	    return 0;
	}
    }
    return EINVAL;
}

int
vld_trans_set_vflags(int32_t trans, int32_t vflags)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] && transactions[i]->tid == trans) {
	    transactions[i]->vflags = vflags;
	    return 0;
	}
    }
    return EINVAL;
}

int
vld_trans_get_vflags(int32_t trans, int32_t *vflags)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i] && transactions[i]->tid == trans) {
	    *vflags = transactions[i]->vflags;
	    return 0;
	}
    }
    return EINVAL;
}

/*
 * Check if the {volume, partition} pair is busy. If it is, return the
 * appropriate error code. Otherwise, return 0. If the partition
 * argument is -1, any partition will do (this is for fsprocs).
 */

int
vld_check_busy(int32_t volid, int32_t partition)
{
    int i;

    for (i = 0; i < MAX_TRANSACTIONS; i++) {
	if (transactions[i]
	    && (partition == -1 || transactions[i]->partition == partition)
	    && transactions[i]->volid == volid) {
	    if (transactions[i]->iflags & ITOffline)
		return VOFFLINE;
	    if (transactions[i]->iflags & ITBusy)
		return VBUSY;
	    return EINVAL;
	}
    }
    return 0;
}

/*
 * create a new volume on partition `part' with volume id `volid' and name
 * `name' of type `backstoretype'
 */

int
vld_create_volume (struct dp_part *dp, int32_t volid, 
		   const char *name, int32_t backstoretype, int32_t voltype,
		   int flags)
{
    volume_handle *vol;
    int ret;

    if (VLD_VALID_BACKSTORETYPE(backstoretype) ||
	VLD_VALID_VOLNAME(name))
	return EINVAL;

    if (volume_htab) {
	ret = vld_find_vol (volid, &vol);
	if (ret == 0) {
	    vld_free (vol);
	    return EEXIST;
	} else if (ret != ENOENT)
	    return ret;
    } else {
	ret = vld_open_volume_by_num (dp, volid, &vol);
	if (ret == 0) {
	    vld_free (vol);
	    return EEXIST;
	} else if (ret != ENOENT)
	    return ret;
    }

    vol = malloc (sizeof(*vol));
    if (vol == NULL)
	return ENOMEM;

    memset (vol, 0, sizeof (*vol));
    vol->vol = volid;
    dp_ref (dp);
    vol->dp = dp;

    vol->ref = 1;

    ret = VOLOP_OPEN(backstoretype, dp, volid, VOLOP_CREATE, &vol->data);
    if (ret) {
	dp_free (dp);
	return ret;
    }

    ret = VOLOP_ICREATE(vol, &vol->fino, NODE_VOL, NULL);
    if (ret) {
	VOLOP_REMOVE (vol);
	dp_free (dp);
	return ret;
    }

    ret = VOLOP_ICREATE(vol, &vol->dino, NODE_VOL, NULL);
    if (ret) {
	VOLOP_IUNLINK(vol, &vol->fino);
	VOLOP_REMOVE(vol);
	dp_free (dp);
	return ret;
    }

    ret = VOLOP_ICREATE(vol, &vol->sino, NODE_VOL, NULL);
    if (ret) {
	VOLOP_IUNLINK(vol, &vol->fino);
	VOLOP_IUNLINK(vol, &vol->dino);
	VOLOP_REMOVE(vol);
	dp_free (dp);
	return ret;
    }

    {
	int fd;

	ret = VOLOP_IOPEN(vol,&vol->sino,O_RDWR,&fd);
	if (ret)
	    VOLOP_IUNLINK(vol, &vol->fino);
	    VOLOP_IUNLINK(vol, &vol->dino);
	    VOLOP_IUNLINK(vol, &vol->sino);
	    VOLOP_REMOVE (vol);
	    dp_free (dp);
	    return ret;
	}

	ret = vol_create (fd, volid, name, voltype, 0);
	if (ret) {
	    VOLOP_IUNLINK(vol, &vol->fino);
	    VOLOP_IUNLINK(vol, &vol->dino);
	    VOLOP_IUNLINK(vol, &vol->sino);
	    VOLOP_REMOVE (vol);
	    close (fd);
	    dp_free (dp);
	    return ret;
	}
	close (fd);
    }

    {
	char path[MAXPATHLEN];
	int fd = -1;
	vstatus vs;

#define ERR_OUT_VSTATUS(ret) \
	do { \
	VOLOP_IUNLINK(vol, &vol->fino); \
	VOLOP_IUNLINK(vol, &vol->dino); \
	VOLOP_IUNLINK(vol, &vol->sino); \
	VOLOP_REMOVE (vol); \
	dp_free (dp); \
	if (fd != -1) \
	    close (fd); \
	return ret; \
	} while (0)
	
	ret = vol_getfullname (DP_NUMBER(dp), volid, path, sizeof(path));
	if (ret)
	    ERR_OUT_VSTATUS(ret);

	fd = open (path, O_RDWR|O_CREAT, 0600);
	if (fd < 0)
	    ERR_OUT_VSTATUS(errno);

	vol->type = backstoretype;
	vol->vol = volid;
	vol->voldbtype = VOLDB_DEFAULT_TYPE;

	memset (&vs, 0, sizeof(vs));
	vs.volid = volid;
	vs.type = voltype;
	vs.bstype = backstoretype;
	vs.voldbtype = VOLDB_DEFAULT_TYPE;
	memcpy (&vs.volinfoinode, &vol->sino, sizeof(vol->sino));
	memcpy (&vs.dirinode, &vol->dino, sizeof(vol->dino));
	memcpy (&vs.fileinode, &vol->fino, sizeof(vol->fino));

	ret = vstatus_write (fd, &vs);
	if (ret)
	    ERR_OUT_VSTATUS(ret);

	close (fd);
#undef ERR_OUT_VSTATUS
    }

    {
	int fd;
	ret = VOLOP_IOPEN(vol,&vol->fino,O_RDWR,&fd);
	if (ret) 
	    abort();
	    
	ret = voldb_create_header (fd, VOLDB_DEFAULT_TYPE, VOLDB_FILE);
	if (ret)
	    abort();

	ret = VOLOP_IOPEN(vol,&vol->dino,O_RDWR,&fd);
	if (ret)
	    abort();
	    
	ret = voldb_create_header (fd, VOLDB_DEFAULT_TYPE, VOLDB_DIR);
	if (ret)
	    abort();

	/* voldb_create_header will close `fd' */
    }
    {
	AFSFid child;
	AFSStoreStatus ss;
	struct mnode *n;
	struct mnode parent_n;

	memset (&ss, 0, sizeof (ss));
	ss.Mask = SS_MODEBITS;
	ss.UnixModeBits = 0755;

	ret = vld_db_uptodate (vol);
	if (ret)
	    abort();

	parent_n.fid.Volume = vol->vol;
	parent_n.fid.Vnode = 1;
	parent_n.fid.Unique = 1;


	memset (parent_n.e.u.dir.acl, 0, 
		sizeof (parent_n.e.u.dir.acl));
	memset (parent_n.e.u.dir.negacl, 0, 
		sizeof (parent_n.e.u.dir.negacl));
	parent_n.e.u.dir.acl[0].owner = PR_SYSADMINID;
	parent_n.e.u.dir.acl[0].flags =
	    PRSFS_LOOKUP | 
	    PRSFS_READ | 
	    PRSFS_WRITE |
	    PRSFS_INSERT |
	    PRSFS_DELETE |
	    PRSFS_LOCK |
	    PRSFS_ADMINISTER;
	parent_n.e.u.dir.acl[1].owner = PR_ANYUSERID;
	parent_n.e.u.dir.acl[1].flags = PRSFS_LOOKUP | PRSFS_READ;

	ret = vld_create_entry (vol, &parent_n, &child, 
				TYPE_DIR, &ss, &n, NULL);
	if (ret)
	    abort();

	mnode_free(n, FALSE);
	
	assert (child.Vnode == 1 && child.Unique == 1);

	ret = vld_db_flush (vol);
	if (ret)
	    abort();
    }

    if (volume_htab)
	hashtabadd (volume_htab, vol);
    else
	vld_free(vol);

    return 0;
}

static int
delete_all_nodes (struct volume_handle *volh)
{
    uint32_t dsize, fsize;
    int ret, i;

    /* XXX check volintInfo */

    ret = vld_db_uptodate (volh);
    if (ret)
	return ret;

    ret = voldb_header_info(VLD_VOLH_FILE(volh), &fsize, NULL);
    if (ret)
	return ret;

    for (i = 0; i < fsize; i++) {
	struct voldb_entry entry;
	onode_opaque o;
	
	ret = voldb_get_entry (VLD_VOLH_FILE(volh), i, &entry);
	if (ret)
	    continue;

	if (entry.u.file.FileType != 0 &&
	    entry.u.file.nextptr == VOLDB_ENTRY_USED) {
	    mlog_log (MDEBVLD, "removing file %d\n", i);
	    ret = voldb_del_entry(VLD_VOLH_FILE(volh), i, &o);
	    if (ret)
		return ret;
	    VOLOP_IUNLINK(volh, &o);
	}
    }

    ret = voldb_header_info(VLD_VOLH_DIR(volh), &dsize, NULL);
    if (ret)
	return ret;

    for (i = 0; i < dsize; i++) {
	struct voldb_entry entry;
	onode_opaque o;
	
	ret = voldb_get_entry (VLD_VOLH_DIR(volh), i, &entry);
	if (ret)
	    continue;

	if (entry.u.file.FileType != 0 &&
	    entry.u.file.nextptr == VOLDB_ENTRY_USED) {
	    mlog_log (MDEBVLD, "removing dir %d\n", i);
	    ret = voldb_del_entry(VLD_VOLH_DIR(volh), i, &o);
	    if (ret)
		return ret;
	    VOLOP_IUNLINK(volh, &o);
	}
    }

    VOLOP_IUNLINK(volh, &volh->fino);
    VOLOP_IUNLINK(volh, &volh->dino);
    VOLOP_IUNLINK(volh, &volh->sino);
    VOLOP_REMOVE (volh);
    
    return 0;
}

int
vld_foreach_dir (struct volume_handle *volh,
		 int (*func)(int fd,
			     uint32_t vnode,
			     uint32_t uniq,
			     uint32_t length,
			     uint32_t dataversion,
			     uint32_t author,
			     uint32_t owner,
			     uint32_t group,
			     uint32_t parent,
			     uint32_t client_date,
			     uint32_t server_date,
			     uint16_t nlinks,
			     uint16_t mode,
			     uint8_t type,
			     int32_t *acl,
			     void *arg),
		 void *arg)
{
    struct voldb_entry entry;
    onode_opaque *o;
    uint32_t size;
    int ret, i;
    int num;
    int fd;
    int32_t acl[48]; /* XXX */

    ret = vld_db_uptodate (volh);
    if (ret)
	return ret;

    ret = voldb_header_info(VLD_VOLH_DIR(volh), &size, NULL);
    if (ret)
	return ret;

    for (num = 0; num < size; num++) {

	ret = voldb_get_entry (VLD_VOLH_DIR(volh), num, &entry);
	if (ret)
	    continue;

	{
	    int32_t size;
	    int32_t version;
	    int32_t total;
	    int32_t positive;
	    int32_t negative;

	    memset(acl, 0, sizeof(acl));
	    for (i = 0; entry.u.dir.acl[i].owner; i++) {
		acl[i*2+5] = htonl(entry.u.dir.acl[i].owner);
		acl[i*2+6] = htonl(entry.u.dir.acl[i].flags);
	    }
	    positive = i;
	    for (i = 0; entry.u.dir.negacl[i].owner; i++) {
		acl[(i+positive)*2+5] = htonl(entry.u.dir.negacl[i].owner);
		acl[(i+positive)*2+6] = htonl(entry.u.dir.negacl[i].flags);
	    }
	    negative = i;
	    total = positive + negative;
	    version = 1;
	    size = sizeof(acl);
	    acl[0] = htonl(size);
	    acl[1] = htonl(version);
	    acl[2] = htonl(total);
	    acl[3] = htonl(positive);
	    acl[4] = htonl(negative);
	}

	o = &entry.u.dir.ino;

	if (entry.u.dir.FileType != 0 &&
	    entry.u.dir.nextptr == VOLDB_ENTRY_USED) {
	    ret = VOLOP_IOPEN(volh, o, O_RDONLY, &fd);
	    if (ret)
		return ret;

	    mlog_log(MDEBVOLDB, "vnode %i length %d",
		     dir_local2afs(num), entry.u.dir.Length);

	    ret = func(fd,
		       dir_local2afs(num),
		       entry.u.dir.unique,
		       entry.u.dir.Length,
		       entry.u.dir.DataVersion,
		       entry.u.dir.Author,
		       entry.u.dir.Owner,
		       entry.u.dir.Group,
		       entry.u.dir.ParentVnode,
		       entry.u.dir.ServerModTime, /* XXX */
		       entry.u.dir.ServerModTime,
		       entry.u.dir.LinkCount,
		       entry.u.dir.UnixModeBits,
		       entry.u.dir.FileType,
		       acl,
		       arg);
	    close(fd);
	    if (ret)
		return ret;
	}
    }
    
    return 0;
}

int
vld_foreach_file (struct volume_handle *volh,
		  int (*func)(int fd,
			      uint32_t vnode,
			      uint32_t uniq,
			      uint32_t length,
			      uint32_t dataversion,
			      uint32_t author,
			      uint32_t owner,
			      uint32_t group,
			      uint32_t parent,
			      uint32_t client_date,
			      uint32_t server_date,
			      uint16_t nlinks,
			      uint16_t mode,
			      uint8_t type,
			      int32_t *acl,
			      void *arg),
		  void *arg)
{
    struct voldb_entry entry;
    onode_opaque *o;
    uint32_t size;
    int ret;
    int num;
    int fd;

    ret = vld_db_uptodate (volh);
    if (ret)
	return ret;

    ret = voldb_header_info(VLD_VOLH_FILE(volh), &size, NULL);
    if (ret)
	return ret;

    for (num = 0; num < size; num++) {

	ret = voldb_get_entry (VLD_VOLH_FILE(volh), num, &entry);
	if (ret)
	    continue;

	o = &entry.u.dir.ino;

	if (entry.u.dir.FileType != 0 &&
	    entry.u.dir.nextptr == VOLDB_ENTRY_USED) {
	    ret = VOLOP_IOPEN(volh, o, O_RDONLY, &fd);
	    if (ret)
		return ret;

	    mlog_log(MDEBVOLDB, "vnode %i length %d",
		     file_local2afs(num), entry.u.file.Length);

	    ret = func(fd,
		       file_local2afs(num),
		       entry.u.file.unique,
		       entry.u.file.Length,
		       entry.u.file.DataVersion,
		       entry.u.file.Author,
		       entry.u.file.Owner,
		       entry.u.file.Group,
		       entry.u.file.ParentVnode,
		       entry.u.file.ServerModTime, /* XXX */
		       entry.u.file.ServerModTime,
		       entry.u.file.LinkCount,
		       entry.u.file.UnixModeBits,
		       entry.u.file.FileType,
		       NULL,
		       arg);
	    close(fd);
	    if (ret)
		return ret;
	}
    }
    
    return 0;
}


int
vld_delete_volume (struct dp_part *dp, int32_t volid, 
		   int32_t backstoretype,
		   int flags)
{
    int ret;
    volume_handle *vol;
    char path[MAXPATHLEN];

    ret = vld_open_volume_by_num (dp, volid, &vol);
    if (ret)
	return ret;

    ret = delete_all_nodes (vol);
    if (ret) {
	vld_free (vol);
	return ret;
    }

    hashtabdel (volume_htab, vol);

    ret = vol_getfullname (DP_NUMBER(dp), volid, path, sizeof(path));
    if (ret) {
	vld_free (vol);
	return ret;
    }

    ret = unlink(path);
    if (ret) {
	vld_free (vol);
	return ret;
    }

    vld_free (vol);

    return 0;
}

/*
 * Register a new volume type
 */

int
vld_register_vol_type (const int32_t backstoretype, vol_op *ops)
{
    if (backstoretype < 0 || backstoretype >= VLD_MAX_BACKSTORE_TYPES)
	return EINVAL;

    assert (ops);

    backstoretypes[backstoretype] = ops;

    return 0;
}

/*
 * Find volume in incore db
 */

int
vld_find_vol (const int32_t volid, struct volume_handle **vol)
{
    volume_handle key, *r;

    key.vol = volid;
    
    r = hashtabsearch (volume_htab, &key);
    if (r == NULL) {
	*vol = NULL;
	return ENOENT;
    } else {
	vld_ref (r);
	*vol = r;
    }

    return 0;
}

/*
 *
 */

static int
vld_storestatus_to_dent (struct voldb_dir_entry *e, 
			 const AFSStoreStatus *ss,
			 struct msec *m)
{
    if (ss->Mask & SS_OWNER)
	e->Owner = ss->Owner;

    if (ss->Mask & SS_MODTIME) {
	e->ServerModTime = ss->ClientModTime; /* XXX should modify
						 ClientModTime */
    }
    if (ss->Mask & SS_GROUP)
	e->Group = ss->Group;
    if (ss->Mask & SS_MODEBITS)
	e->UnixModeBits = 07777 & ss->UnixModeBits;
    if (ss->Mask & SS_SEGSIZE)
	e->SegSize = ss->SegSize;
    return 0;
}

/*
 *
 */

static int
vld_storestatus_to_fent (struct voldb_file_entry *e, 
			 const AFSStoreStatus *ss,
			 struct msec *m)
{
    if (ss->Mask & SS_OWNER)
	e->Owner = ss->Owner;

    if (ss->Mask & SS_MODTIME) {
	e->ServerModTime = ss->ClientModTime; /* XXX should modify
						 ClientModTime */
    }
    if (ss->Mask & SS_GROUP)
	e->Group = ss->Group;
    if (ss->Mask & SS_MODEBITS)
	e->UnixModeBits = 0777 & ss->UnixModeBits;
    if (ss->Mask & SS_SEGSIZE)
	e->SegSize = ss->SegSize;
    
    return 0;
}

/*
 *
 */

int
vld_storestatus_to_ent (struct voldb_entry *e, 
			const AFSStoreStatus *ss,
			struct msec *m)
{
    switch (e->type) {
    case TYPE_DIR:
	return vld_storestatus_to_dent (&e->u.dir, ss, m);
    case TYPE_FILE:
    case TYPE_LINK:
	return vld_storestatus_to_fent (&e->u.file, ss, m);
    }
    abort();
}

/*
 *
 */

static int
vld_dent_to_fetchstatus (struct volume_handle *vol, 
			 struct voldb_dir_entry *e,
			 struct mnode *n)
{
    int ret;
    AFSFetchStatus *fs;

    ret = mnode_update_size_cached (n);
    if (ret)
	return ret;

    fs = &n->fs;

    fs->InterfaceVersion = 1;
    fs->FileType 	= e->FileType;
    fs->LinkCount 	= e->LinkCount;
    fs->DataVersion 	= e->DataVersion;
    
    fs->ParentVnode	= e->ParentVnode;
    fs->ParentUnique	= e->ParentUnique;
    
    fs->SegSize 	= e->SegSize;
    fs->ClientModTime 	= e->ServerModTime;
    fs->ServerModTime	= e->ServerModTime;
    fs->SyncCount 	= 0;
    fs->DataVersionHigh	= vol->info.creationDate;
    fs->LockCount	= 0;
    fs->LengthHigh	= 0;
    fs->ErrorCode	= 0;
    fs->Author		= e->Author;
    fs->Owner		= e->Owner;
    fs->Group		= e->Group;
    fs->UnixModeBits	= e->UnixModeBits;
    
    /*
     * Dummy
     */
    
    fs->CallerAccess = 0;
    fs->AnonymousAccess = 0;

    return 0;
}

/*
 *
 */

static int
vld_fent_to_fetchstatus (struct volume_handle *vol,
			 struct voldb_file_entry *e,
			 struct mnode *n)
{
    int ret;
    AFSFetchStatus *fs;

    ret = mnode_update_size_cached (n);
    if (ret)
	return ret;

    fs = &n->fs;

    fs->InterfaceVersion = 1;
    fs->FileType 	= e->FileType;
    fs->LinkCount 	= e->LinkCount;
    fs->DataVersion 	= e->DataVersion;
    
    fs->ParentVnode	= e->ParentVnode;
    fs->ParentUnique	= e->ParentUnique;
    
    fs->SegSize 	= e->SegSize;
    fs->ClientModTime 	= e->ServerModTime;
    fs->ServerModTime	= e->ServerModTime;
    fs->SyncCount 	= 0;
    fs->DataVersionHigh	= vol->info.creationDate;
    fs->LockCount	= 0;
    fs->LengthHigh	= 0;
    fs->ErrorCode	= 0;
    fs->Author		= e->Author;
    fs->Owner		= e->Owner;
    fs->Group		= e->Group;
    fs->UnixModeBits	= e->UnixModeBits;
    
    /*
     * Dummy
     */
    
    fs->CallerAccess = 0;
    fs->AnonymousAccess = 0;

    return 0;
}

/*
 *
 */

static int
vld_ent_to_fetchstatus  (struct volume_handle *vol,
			 struct voldb_entry *e,
			 struct mnode *n)
{
    int ret;

    if (n->flags.fsp)
	return 0;

    assert (n->flags.ep);

    switch (e->type) {
    case TYPE_DIR:
	ret = vld_dent_to_fetchstatus (vol, &e->u.dir, n);
	break;
    case TYPE_FILE:
    case TYPE_LINK:
	ret = vld_fent_to_fetchstatus (vol, &e->u.file, n);
	break;
    default:
	abort();
	break;
    }
    if (ret == 0)
	n->flags.fsp = TRUE;
    return ret;
}

/*
 *
 */

static void
vld_set_author(struct voldb_entry *e, struct msec *m)
{
    if (m == NULL)
	return;
    assert (m->sec);
    switch (e->type) {
    case TYPE_DIR:
	e->u.dir.Author = m->sec->uid;
	break;
    case TYPE_FILE:
    case TYPE_LINK:
	e->u.file.Author = m->sec->uid;
	break;
    default:
	abort();
	break;
    }
}

/*
 *
 */

int
volop_iopen (volume_handle *vol, onode_opaque *o, struct mnode *n)
{
    int ret;

    if (n->flags.fdp)
	return 0;

    ret = VOLOP_IOPEN(vol, o, O_RDWR, &n->fd);
    if (ret) 
	return ret;
    n->flags.fdp = TRUE;
    return 0;
}

/*
 *
 */

#if 0
int
foo(void)
{
    *callers_right = 0;
    *anonymous_right = 0;

    foreach (parent_n->e.positive_acl) {
	if (member ($_.uid, anonymous))
	    *anonymous_right =  $_.rights;
	if (member ($_.uid, m->cps)) {
	    *callers_right |= $_.rights:
	}
    }
    foreach (parent_n->e.negative_acl) {
	if (member ($_.uid, anonymous))
	    *anonymous_right &= ~ $_.rights;
	if (member ($_.uid, m->cps)
	    *callers_rights &= ~ $_.rights;
	}
    }
    if (validop_p(*callers_rights, operation))
	return TRUE;
    return FALSE;
}
#endif

/*
 * check the right to use `n'
 *
 * We also need to take care of the special case (1,1) since it
 * doesn't have a parent directory.
 */

struct {
    volop_flags op;
    int32_t afsprivs;
} check_flags[] = {
    { VOLOP_READ,	PRSFS_READ },
    { VOLOP_WRITE,	PRSFS_WRITE },
    { VOLOP_INSERT,	PRSFS_INSERT },
    { VOLOP_DELETE,	PRSFS_DELETE },
    { VOLOP_LOOKUP,	PRSFS_LOOKUP},
    { VOLOP_LOCK,	PRSFS_LOCK },
    { VOLOP_ADMIN,	PRSFS_ADMINISTER },
    { VOLOP_GETSTATUS,	PRSFS_READ }
};



int
vld_check_rights (volume_handle *vol, struct mnode *n,
		  struct msec *m)
{
    struct mnode *parent_n;
    AFSFid parent_fid;
    struct msec pm;
    int ret, i, j;
    int right_ret = 0;

    assert (n->flags.fsp);
    assert (n->flags.ep);

    if (m->loop > 1)
	abort();

#if 0
    if ((m->flags & VOLOP_NOCHECK) == VOLOP_NOCHECK)
	return 0;
#endif

    assert(m->sec);

    if (m->sec->cps == NULL)
	return EPERM;
    
    if (n->e.type == TYPE_DIR) {
	parent_n = n;
    } else {
	memset (&pm, 0, sizeof (pm));
	pm.sec 			= m->sec;
	pm.caller_access	= 0;
	pm.anonymous_access	= 0;
	pm.flags		= VOLOP_GETSTATUS;
	pm.loop			= m->loop + 1;
	
	parent_fid.Volume	= n->fid.Volume;
	parent_fid.Vnode	= n->fs.ParentVnode;
	parent_fid.Unique	= n->fs.ParentUnique;
	
	ret = mnode_find (&parent_fid, &parent_n);
	if (ret)
	    return EACCES;
	
	ret = vld_open_vnode (vol, parent_n, &pm);
	if (ret) {
	    mnode_free (parent_n, FALSE);
	    return EACCES;
	}
	assert (parent_n->flags.ep);

    }

    m->caller_access = m->anonymous_access = 0;

    for (i = 0; i < FS_MAX_ACL; i++) {
	for (j = 0; j < m->sec->cps->len; j++)
	    if (parent_n->e.u.dir.acl[i].owner == m->sec->cps->val[j])
		m->caller_access |= parent_n->e.u.dir.acl[i].flags;
	if (parent_n->e.u.dir.acl[i].owner == PR_ANYUSERID)
	    m->anonymous_access |= parent_n->e.u.dir.acl[i].flags;
    }

    for (i = 0; i < FS_MAX_ACL; i++) {
	for (j = 0; j < m->sec->cps->len; j++)
	    if (parent_n->e.u.dir.negacl[i].owner == m->sec->cps->val[j])
		m->caller_access &= ~parent_n->e.u.dir.negacl[i].flags;
	if (parent_n->e.u.dir.negacl[i].owner == PR_ANYUSERID)
	    m->anonymous_access &= ~parent_n->e.u.dir.negacl[i].flags;
    }

    if (m->sec->superuser)
	m->caller_access |= PRSFS_LOOKUP | PRSFS_ADMINISTER;

    for (i = 0; 
	 i < sizeof(check_flags)/sizeof(*check_flags) 
	     && right_ret == 0;
	 i++) 
    {
	if ((m->flags & 
	     check_flags[i].op) == check_flags[i].op
	    && (check_flags[i].afsprivs & 
		m->caller_access) !=  check_flags[i].afsprivs)
	    right_ret = EACCES;
    }
    
    if (parent_n != n)
	mnode_free (parent_n, FALSE);

    return right_ret;
}

/*
 * open an mnode `n' in volume `vol' with `flags' in `m' as specifed above.
 */

int
vld_open_vnode (volume_handle *vol, struct mnode *n, struct msec *m)
{
    int ret = 0;
    int32_t real_mnode;
    struct voldb *db;
    onode_opaque *o;
    
    assert (vol);

    /*
     * is everything cached ?
     */

    if ((m->flags & VOLOP_GETSTATUS) == 0  /* XXX why? */
	&& n->flags.fsp == TRUE
	&& n->flags.ep == TRUE)
	return 0;

    if (afs_dir_p (n->fid.Vnode)) {
	real_mnode = dir_afs2local (n->fid.Vnode);
	db = VLD_VOLH_DIR(vol);
	o = &n->e.u.dir.ino;
    } else {
	real_mnode = file_afs2local (n->fid.Vnode);
	db = VLD_VOLH_FILE(vol);
	o = &n->e.u.file.ino;
    }

    if (n->flags.ep == FALSE) {
	ret = voldb_get_entry (db, real_mnode, &n->e); 
	if (ret)
	    return ret;
	n->flags.ep = TRUE;
    }

    if (n->flags.fdp == FALSE) {
	ret = volop_iopen (vol, o, n);
	if (ret)
	    return ret;
    }

    n->flags.fdp = TRUE;

    if ((m->flags & VOLOP_GETSTATUS) == VOLOP_GETSTATUS)
	ret = vld_ent_to_fetchstatus (vol, &n->e, n);

    return ret;
}


/*
 * Modify node `n' in volume `vol' with context bits `m'
 * with StoreStatus bits `ss' and length `len'.
 */

int
vld_modify_vnode (volume_handle *vol, struct mnode *n, struct msec *m,
		  const AFSStoreStatus *ss, int32_t *len)
{
    int ret;
    int32_t real_mnode;
    struct voldb *db;

    assert (vol);
    assert (n->flags.fdp);
    assert (n->flags.ep);

    if (afs_dir_p (n->fid.Vnode)) {
	real_mnode = dir_afs2local (n->fid.Vnode);
	db = VLD_VOLH_DIR(vol);
    } else {
	real_mnode = file_afs2local (n->fid.Vnode);
	db = VLD_VOLH_FILE(vol);
    }

    if (ss && ss->Mask) {
	ret = vld_storestatus_to_ent (&n->e, ss, m);
	if (ret)
	    return ret;
    }

    if (len) {
	int diff;
	uint32_t *Length;

	assert (vol->flags.infop);
	
	if (afs_dir_p (n->fid.Vnode))
	    Length = &n->e.u.dir.Length;
	else
	    Length = &n->e.u.file.Length;

	diff = *len / 1024 - *Length / 1024; 

	mlog_log (MDEBFS,
		  "vld_modify_vnode: olen=%d, nlen=%d, diff=%d", *Length, *len, diff);

	ret = vld_update_volsize (vol, diff);
	if (ret)
	    return ret;

	ret = ftruncate (n->fd, *len);
	if (ret) {
	    vld_update_volsize (vol, -diff);
	    return ret;
	}

	*Length = *len;

	n->sb.st_size = *len;
    }

    if (m->flags & (VOLOP_WRITE|VOLOP_INSERT|VOLOP_DELETE)) {
	if (n->e.type == TYPE_DIR) {
	    n->e.u.dir.DataVersion++;
	    n->e.u.dir.ServerModTime = time(0);
	} else {
	    n->e.u.file.DataVersion++;
	    n->e.u.file.ServerModTime = time(0);
	}
    }

    ret = voldb_put_entry (db, real_mnode, &n->e);
    if (ret)
	return ret;

    n->flags.fsp = FALSE; /* Force vld_ent_to_fetchstatus
			     to fill in the field */
    ret = vld_ent_to_fetchstatus (vol, &n->e, n);

    return ret;
}

/*
 * Put acl for node `n' in volume `vol' with context bits `m'.
 */

int
vld_put_acl (volume_handle *vol, struct mnode *n, struct msec *m)
{
    int ret;
    int32_t real_mnode;
    struct voldb *db;

    assert (vol);
    assert (n->flags.fdp);
    assert (n->flags.ep);

    if (afs_dir_p (n->fid.Vnode)) {
	real_mnode = dir_afs2local (n->fid.Vnode);
	db = VLD_VOLH_DIR(vol);
    } else {
	return EINVAL;
    }
    
    ret = voldb_put_acl (db, real_mnode, &n->e.u.dir);
    if (ret)
	return ret;

    n->flags.fsp = FALSE; /* Force vld_ent_to_fetchstatus
			     to fill in the field */
	
    if ((m->flags & VOLOP_GETSTATUS) == VOLOP_GETSTATUS)
	ret = vld_ent_to_fetchstatus (vol, &n->e, n);

    return ret;
}


/*
 * Convert a volume handle to AFS volsync
 */

int
vld_vld2volsync (const struct volume_handle *vld, AFSVolSync *volsync)
{
    if (volsync)
	memset (volsync, 0, sizeof (*volsync));
    return 0;
}

/*
 * Make sure the db is stored to disk
 */

int
vld_db_flush (volume_handle *vol)
{
    int ret;
    if (vol->flags.voldbp) {
	assert (VLD_VOLH_DIR(vol) != NULL && VLD_VOLH_FILE(vol) != NULL);

	ret = voldb_flush (VLD_VOLH_DIR(vol));
	assert (ret == 0);
	ret = voldb_flush (VLD_VOLH_FILE(vol));
	assert (ret == 0);
    } else {
	assert (VLD_VOLH_DIR(vol) == NULL && VLD_VOLH_FILE(vol) == NULL);
    }
    return 0;
}

/*
 * Bring db in volume uptodate
 *
 * XXX when failing bring volume offline
 */

int
vld_db_uptodate (volume_handle *vol)
{
    int ret, fd;

    if (vol->flags.voldbp) {
	if (db_lru) {
	    listdel (db_lru, vol->db_li);
	    vol->db_li = listaddhead (db_lru, vol);
	    if (vol->db_li == NULL)
		abort (); /* XXX */
	} else {
	    assert (vol->db_li == NULL);
	}
	return 0;
    }
    assert (VLD_VOLH_DIR(vol) == NULL && VLD_VOLH_FILE(vol) == NULL);

    if (db_lru && !listemptyp(db_lru)) {
	volume_handle *old_vol;

	old_vol = listdeltail (db_lru);
	if (old_vol) {
	    assert (VLD_VOLH_DIR(old_vol) && VLD_VOLH_FILE(old_vol));
	    
	    voldb_close (VLD_VOLH_DIR(old_vol));
	    voldb_close (VLD_VOLH_FILE(old_vol));
	    VLD_VOLH_DIR(old_vol) = VLD_VOLH_FILE(old_vol) = NULL;
	    old_vol->db_li = NULL;
	}
    }

    /*
     * Note there isn't a fd-leek here, they are picked up
     * by voldb_init, and closed at will by voldb.
     */
	    
    ret = VOLOP_IOPEN(vol, &vol->dino, O_RDWR, &fd);
    if (ret)
	return ret;

    ret = voldb_init (fd, vol->voldbtype, vol->vol, &VLD_VOLH_DIR(vol));
    if (ret) {
	close (fd);
	VLD_VOLH_DIR(vol) = NULL;
	return ret;
    }

    ret = VOLOP_IOPEN(vol, &vol->fino, O_RDWR, &fd);
    if (ret) {
	voldb_close (VLD_VOLH_DIR(vol));
	VLD_VOLH_DIR(vol) = NULL;
	return ret;
    }

    ret = voldb_init (fd, vol->voldbtype, vol->vol, &VLD_VOLH_FILE(vol));
    if (ret) {
	close (fd);
	voldb_close (VLD_VOLH_DIR(vol));
	VLD_VOLH_DIR(vol) = NULL;
	VLD_VOLH_FILE(vol) = NULL;
	return ret;
    }

    vol->flags.voldbp = TRUE;

    if (db_lru)
	vol->db_li = listaddhead (db_lru, vol);

    return 0;
}

static int
vld_open_volume_by_handle (struct dp_part *dp, volume_handle *vol)
{
    int ret, fd;
    char path[MAXPATHLEN];
    vstatus vs;

    vld_ref(vol);

    ret = vol_getfullname (DP_NUMBER(dp), vol->vol, path, sizeof (path));
    if (ret)
	return ret;

    fd = open (path, O_RDONLY, 0600);
    if (fd < 0)
	return errno;

    ret = vstatus_read (fd, &vs);
    if (ret) {
	close (fd);
	return ret;
    }
    close (fd);

    ret = vstatus2volume_handle (&vs, dp, &vol);
    if (ret)
	return ret;

    ret = VOLOP_OPEN(vol->type, vol->dp, vol->vol, 
		     VOLOP_NOFLAGS, &vol->data);

    return ret;
}


/*
 * Open volume on partition `dp' with volume id `volid'
 * and return it ref:ed in `vol'.
 */

int
vld_open_volume_by_num (struct dp_part *dp, int32_t volid,
			volume_handle **vol)
{
    int ret, fd;
    char path[MAXPATHLEN];
    vstatus vs;

    if (volume_htab) {
	volume_handle *r, key;
	key.vol = volid;

	r = hashtabsearch (volume_htab, &key);
	if (r == NULL)
	    return ENOENT;
	else
	    vld_ref (r);
	*vol = r;
	return 0;
    }

    ret = vol_getfullname (DP_NUMBER(dp), volid, path, sizeof (path));
    if (ret)
	return ret;

    fd = open (path, O_RDONLY, 0600);
    if (fd < 0)
	return errno;

    ret = vstatus_read (fd, &vs);
    if (ret) {
	close (fd);
	return ret;
    }
    close (fd);

    ret = vstatus2volume_handle (&vs, dp, vol);
    if (ret)
	return ret;

    ret = VOLOP_OPEN((*vol)->type, (*vol)->dp, (*vol)->vol, 
		     VOLOP_NOFLAGS, &(*vol)->data);
    if (ret)
	vld_free (*vol);

    return ret;
}

int
vld_remove_node (volume_handle *vol, struct mnode *n)
{
    int ret;
    onode_opaque o;
    int32_t node = n->fid.Vnode;
    int diff;

    assert (vol->flags.infop);

    mnode_remove(&n->fid);

    if (afs_dir_p (node)) {
	ret = voldb_del_entry (VLD_VOLH_DIR(vol), dir_afs2local(node), &o);
	if (ret)
	    return ret;

	diff = n->e.u.dir.Length;
    } else {
	ret = voldb_del_entry (VLD_VOLH_FILE(vol), file_afs2local(node), &o);
	if (ret)
	    return ret;

	diff = n->e.u.file.Length;
    }

    mlog_log (MDEBFS,
	      "vld_remove_node: olen=%d, diff=%d", diff, -(ENTRY_DISK_SIZE + diff/1024));

    vld_update_volsize (vol, -(ENTRY_DISK_SIZE + diff/1024));

    return VOLOP_IUNLINK (vol, &o);
}


/*
 * Open volume that `fd' points to, if volume_hash is loaded try to
 * find the volume in the hash.
 */

int
vld_open_volume_by_fd (struct dp_part *dp, int fd,
		       volume_handle **vol)
{
    int ret;
    vstatus vs;

    ret = vstatus_read (fd, &vs);
    if (ret)
	return ret;

    if (volume_htab) {
	volume_handle *r, key;
	key.vol = vs.volid;

	r = hashtabsearch (volume_htab, &key);
	if (r == NULL)
	    return ENOENT;
	else
	    vld_ref (r);
	*vol = r;
	return 0;
    }

    ret = vstatus2volume_handle (&vs, dp, vol);
    if (ret)
	return ret;

    ret = VOLOP_OPEN((*vol)->type, (*vol)->dp, (*vol)->vol, 
		     VOLOP_NOFLAGS, &(*vol)->data);
    if (ret)
	vld_free (*vol);

    return ret;
}

/*
 * Make sure info is uptodate
 */

int
vld_info_uptodatep (volume_handle *vol)
{
    int ret, fd;

    assert (vol);
    
    if (vol->flags.infop)
	return 0;

    ret = VOLOP_IOPEN(vol, &vol->sino, O_RDONLY, &fd);
    if (ret)
	return ret;

    ret = vol_read_header (fd, &vol->info);

    close (fd);
    if (ret == 0)
	vol->flags.infop = TRUE;
    return ret;
}

int
vld_info_write (volume_handle *vol)
{
    int ret, fd;

    assert (vol);
    
    ret = VOLOP_IOPEN(vol, &vol->sino, O_WRONLY, &fd);
    if (ret)
	return ret;

    ret = vol_write_header (fd, &vol->info);

    close (fd);

    return ret;
}

/*
 * check quota and update volume size
 */

int
vld_update_volsize  (volume_handle *vol, int diff) {
    int ret = vld_info_uptodatep(vol);
    if (ret)
	return ret;
    
    if (vol->info.maxquota < vol->info.size + diff)
	    return VOVERQUOTA;
    
    vol->info.size += diff;
    
    assert (vol->info.size >= 0);
    ret = vld_info_write (vol); 

    return ret;
}

int
vld_get_volstats (volume_handle *vol,
		  struct AFSFetchVolumeStatus *volstat,
		  char *volName,
		  char *offLineMsg,
		  char *motd) 
{
    int ret;
    long availblocks, totalblocks;

    ret = vld_info_uptodatep(vol);
    if (ret)
	return ret;

    ret = dp_getstats(vol->dp, &availblocks, &totalblocks);
    if (ret)
	return ret;

    volstat->Vid = vol->vol;
    volstat->ParentId = vol->info.parentID;
    volstat->Online = !vol->flags.offlinep;
    volstat->InService = vol->info.inUse;
    volstat->Blessed = vol->info.inUse;
    volstat->NeedsSalvage = vol->info.needsSalvaged;
    volstat->Type = vol->type;
    volstat->MinQuota = 0;
    volstat->MaxQuota = vol->info.maxquota;
    volstat->BlocksInUse = vol->info.size;
    volstat->PartBlocksAvail = availblocks;
    volstat->PartMaxBlocks = totalblocks;
    
    strlcpy(volName, vol->info.name, VNAMESIZE);

    *offLineMsg = '\0';
    *motd = '\0';

    return 0;
}

int
vld_set_volstats (volume_handle *vol,
		  const struct AFSStoreVolumeStatus *volstat,
		  const char *volName,
		  const char *offLineMsg,
		  const char *motd) 
{
    int ret;

    ret = vld_info_uptodatep(vol);
    if (ret)
	return ret;

    if ((volstat->Mask & AFS_SETMAXQUOTA)== AFS_SETMAXQUOTA)
	vol->info.maxquota = volstat->MaxQuota;

    /* XXX store minquota, volName, offLineMsg, motd */

    return 0;
}

struct collect_volumes_args {
    struct dp_part *dp;
    List *vollist;
};

static Bool
vld_collect_volumes (void *ptr, void *arg)
{
    volume_handle *vol = (volume_handle *) ptr;
    struct collect_volumes_args *args = (struct collect_volumes_args *) arg;
    List *vollist = args->vollist;
    struct dp_part *dp = args->dp;

    vld_open_volume_by_handle (dp, vol);

    listaddtail(vollist, vol);

    return 0;
}

int
vld_list_volumes(struct dp_part *dp, List **retlist)
{
    struct collect_volumes_args args;
    List *vollist;

    vollist = listnew();

    if (vollist == NULL)
	return ENOMEM;

    args.vollist = vollist;
    args.dp = dp;

    hashtabforeach(volume_htab, vld_collect_volumes, &args);
    *retlist = vollist;

    return 0;
}

/*
 * Shutdown time
 */

void
vld_end (void)
{
    /* XXX flush volume_htab to disk */

    return;
}

int
restore_file(struct rx_call *call,
	     uint32_t vnode,
	     uint32_t uniq,
	     uint32_t length,
	     uint32_t dataversion,
	     uint32_t author,
	     uint32_t owner,
	     uint32_t group,
	     uint32_t parent,
	     uint32_t client_date,
	     uint32_t server_date,
	     uint16_t nlinks,
	     uint16_t mode,
	     uint8_t type,
	     volume_handle *vol,
	     int32_t *acl)
{
    int ret;
    struct voldb *db;
    int (*convert_afs2local)(int32_t);
    struct voldb_entry e;
    AFSFid node;
    onode_opaque ino;
    node_type ntype;
    struct mnode *n;
    int i;
    int exists;

    switch (type) {
    case TYPE_DIR:
        db = VLD_VOLH_DIR(vol);
        convert_afs2local = dir_afs2local;
        ntype = NODE_DIR;
        break;
    case TYPE_FILE:
    case TYPE_LINK:
        db = VLD_VOLH_FILE(vol);
        convert_afs2local = file_afs2local;
        ntype = NODE_REG;
        break;
    default:
        abort();
    }

    node.Volume = vol->vol;
    node.Vnode = vnode;
    node.Unique = uniq;
    
#if 0
    printf("create_file: %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
	   vnode, uniq, length,
	   dataversion, author, owner, group, parent, client_date,
	   server_date, (uint32_t) nlinks, (uint32_t) mode,
	   (uint32_t) type);
#endif

    ret = voldb_expand(db, convert_afs2local(vnode));
    if (ret)
	return ret;
    
    ret = voldb_get_entry(db, convert_afs2local(vnode), &e);
    if (ret)
	return ret;

    if (type == TYPE_DIR)
	exists = (e.u.dir.nextptr == VOLDB_ENTRY_USED);
    else
	exists = (e.u.file.nextptr == VOLDB_ENTRY_USED);

    if (exists) {
	onode_opaque o;

	ret = voldb_del_entry(db, convert_afs2local(vnode), &o);
	if (ret)
	    return ret;
	VOLOP_IUNLINK(vol, &o);
    }

    e.type = type;

    ret = mnode_find(&node, &n);
    if (ret)
	return ret;

    ret = VOLOP_ICREATE(vol, &ino, ntype, n);
    if (ret)
	return ret;

    if (type == TYPE_DIR) {
	e.u.dir.nextptr = VOLDB_ENTRY_USED;
	e.u.dir.ino = ino;
	e.u.dir.FileType = type;
	e.u.dir.LinkCount = nlinks;
	e.u.dir.DataVersion = dataversion;
	e.u.dir.Length = length;
	e.u.dir.Author = author;
	e.u.dir.Owner = owner;
	e.u.dir.Group = group;
	e.u.dir.ParentVnode = parent;
	e.u.dir.ParentUnique = 0 /* XXX */;
	e.u.dir.ServerModTime = server_date;
	e.u.dir.UnixModeBits = 0x40000 | mode;
	e.u.dir.InterfaceVersion = 1;
	memset (e.u.dir.acl, 0, 
		sizeof (e.u.dir.acl));
	memset (e.u.dir.negacl, 0, 
		sizeof (e.u.dir.negacl));
	{
#if 0
	    int32_t size = ntohl(acl[0]);
	    int32_t version = ntohl(acl[1]);
	    int32_t total = ntohl(acl[2]);
#endif
	    int32_t positive = ntohl(acl[3]);
	    int32_t negative = ntohl(acl[4]);
	    if (positive <= FS_MAX_ACL && negative <= FS_MAX_ACL) {
		for (i = 0; i < positive; i++) {
		    e.u.dir.acl[i].owner = ntohl(acl[i*2+5]);
		    e.u.dir.acl[i].flags = ntohl(acl[i*2+6]);
		}
		for (i = 0; i < negative; i++) {
		    e.u.dir.negacl[i].owner = ntohl(acl[(i+positive)*2+5]);
		    e.u.dir.negacl[i].flags = ntohl(acl[(i+positive)*2+6]);
		}
	    }
	}
	ret = voldb_put_acl (db, convert_afs2local(vnode), &e.u.dir);
	if (ret)
	    return ret;
    } else {
	e.u.file.nextptr = VOLDB_ENTRY_USED;
	e.u.file.ino = ino;
	e.u.file.FileType = type;
	e.u.file.LinkCount = nlinks;
	e.u.file.DataVersion = dataversion;
	e.u.file.Length = length;
	e.u.file.Author = author;
	e.u.file.Owner = owner;
	e.u.file.Group = group;
	e.u.file.ParentVnode = parent;
	e.u.file.ParentUnique = 0 /* XXX */;
	e.u.file.ServerModTime = server_date;
	e.u.file.UnixModeBits = mode;
	e.u.dir.InterfaceVersion = 1;
    }

    ret = voldb_put_entry(db, convert_afs2local(vnode), &e);
    if (ret)
	return ret;

    ret = vld_update_volsize (vol, ENTRY_DISK_SIZE + length/1024);
    if (ret)
	return ret;

    ret = ftruncate(n->fd, length);
    if (ret)
	return errno;

    ret = copyrx2fd (call, n->fd, 0, length);
    if (ret)
	return ret;

    mnode_free (n, FALSE);

    return 0;
}

int
vld_rebuild (struct volume_handle *vol)
{
    int ret;

    assert (vol->flags.voldbp);

    ret = voldb_rebuild (VLD_VOLH_DIR(vol));

    if (ret)
	return ret;

    ret = voldb_rebuild (VLD_VOLH_FILE(vol));

    return ret;
}

static int vld_check_quota (volume_handle *vol, int diff)
{ 
    int ret;

    ret = vld_info_uptodatep(vol);
    if (ret)
	return ret;

    if (vol->info.maxquota < vol->info.size + diff)
	return VOVERQUOTA;

    return 0;
}
