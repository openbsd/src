/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#include <config.h>

RCSID("$arla: salvage.c,v 1.15 2002/03/06 22:43:02 tol Exp $");

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <assert.h>
#include <unistd.h>

#include <fs.h>
#include <rx/rx.h>
#include <fbuf.h>
#include <fdir.h>

#include <vld.h>
#include <afs_dir.h>

#include <mlog.h>
#include <mdebug.h>

#include <salvage.h>


struct inodeinfo {
    struct inodeinfo *parentp, *childp, *siblingp;
    struct inodeinfo *lfnodes;
    int32_t inode_num;
    int32_t parent_num;
    int32_t parent_unique;
    int32_t filetype;
    int32_t num_indir;
    struct {			/* misc dir related flags */
	unsigned dot:1;			/* have a "." */
	unsigned dotdot:1;			/* have a ".." */
	unsigned invalid_dot:1;		/* . doesn't point to this node */
	unsigned invalid_dotdot:1;		/* .. doesn't point to this node */
	unsigned no_data:1;			/* this node doesn't have data */
    } flags;
    enum { IUNUSED = 0,			/* not used */
	   IFOUND,			/* is used */
	   ITREE,			/* node-tree correct */
	   IDIR,			/* exists in a directory tree */
	   IFREE } status;
};

enum { SALVAGE_RN_WRITE = 1, SALVAGE_RN_UNUSED = 2 };

struct vinfo {
    unsigned nnodes;		/* number of nodes in array */
    unsigned fsize;		/* # of allocated file nodes */
    unsigned dsize;		/* # of allocated dir nodes */
    struct inodeinfo **nodes;		/* XXX add a hash */
};

/*
 *
 */

static struct inodeinfo *
find_node (struct vinfo *info, int32_t inode)
{
    uint32_t i;
    assert (info);

    for (i = 0; i < info->nnodes; i++) {
	if (info->nodes[i]->inode_num == inode)
	    return info->nodes[i];
    }
    return NULL;
}

/*
 * Allocate a new node, if there isn't room the the table make room
 * for it.
 */

static struct inodeinfo *
allocate_node (struct vinfo *info, int32_t ino, int type)
{
    struct inodeinfo *n; 
    unsigned oldsize, i;

    n = find_node (info, ino);
    assert (n == NULL);
    assert (info->nnodes >= info->fsize + info->dsize);

    if (info->nnodes == info->fsize + info->dsize) {
	oldsize = info->nnodes;
	if (info->nnodes == 0)
	    info->nnodes = 1;
	info->nodes = erealloc (info->nodes,
				info->nnodes * 2 * sizeof(struct inodeinfo *));
	info->nnodes = info->nnodes * 2;

	for (i = oldsize; i < info->nnodes; i++) {
	    n = emalloc (sizeof(struct inodeinfo));
	    memset (n, 0, sizeof(struct inodeinfo));
	    info->nodes[i] = n;
	}
    }
    n = info->nodes[info->fsize + info->dsize];
    switch (type) {
    case TYPE_FILE:
	++info->fsize;
	break;
    case TYPE_DIR:
	++info->dsize;
	break;
    default:
	abort();
    }
    return n;
}

/*
 *
 */

static void
free_vinfo (struct vinfo *info)
{
    unsigned i;

    for (i = 0; i < info->nnodes; i++)
	free (info->nodes[i]);
    free (info->nodes);
}


/*
 *
 */

static int
opennode (volume_handle *vol, struct vinfo *info, struct inodeinfo *node,
	  int (*func) (volume_handle *, struct vinfo *, 
		       struct inodeinfo *, int, size_t))
{
    struct mnode *n;
    struct msec m;
    VenusFid fid;
    int ret;

    fid.Cell = 0;
    fid.fid.Volume = VLD_VOLH_NUM(vol);
    fid.fid.Vnode = node->inode_num;
    fid.fid.Unique = 0;

    mlog_log (MDEBSALVAGE, "opennode %u.%u", 
	      fid.fid.Volume, fid.fid.Vnode);

    memset (&n, 0, sizeof(n));

    mnode_find (&fid.fid, &n);

    m.flags = VOLOP_WRITE|VOLOP_GETSTATUS|VOLOP_NOCHECK;

    ret = vld_open_vnode (vol, n, &m);
    if (ret) {
	mlog_log (MDEBSALVAGE, "opennode %u.%u: failed to open node", 
		  fid.fid.Volume, fid.fid.Vnode);
	node->flags.no_data = 1;
	mnode_free (n, FALSE);
	return 1;
    }

    if (func) {
	ret = (*func) (vol, info, node, n->fd, n->sb.st_size);
    }

    mnode_free (n, FALSE);
    return ret;
}

/*
 *
 */

#define CHECK_VALUE_EQ(check_val,val,string,modified)			     \
do {									     \
    if ((val) != (check_val)) {						     \
        (check_val) = (val);						     \
	(modified) = 1;							     \
	mlog_log (MDEBSALVAGE, "%s had a errorous value resetting", string); \
    }									     \
} while (0)

/*
 * 
 */

static int
check_value_bitmask(uint32_t *checkval, uint32_t val, uint32_t mask,
		    const char *string)
{
    int32_t newval;

    if ((mask & (*checkval)) & ~val) {
	newval = (*checkval) & (val | (~mask & (*checkval)));
	mlog_log (MDEBSALVAGE, "%s had a errorous value 0%o resetting to 0%o",
		  string, *checkval, newval);
	*checkval = newval;
	return 1;
    }
    return 0;
}

/*
 *
 */

static int
read_nodes_dir (uint32_t num, struct voldb_entry *entry,
		struct vinfo *info, struct inodeinfo **ret_node)
{
    struct inodeinfo *node;
    int mod = 0;

    assert (entry->type == TYPE_DIR);

    if (entry->u.dir.FileType == 0 ||
	entry->u.file.nextptr != VOLDB_ENTRY_USED)
    {
	mlog_log (MDEBSALVAGE, "%d is a unused node (dir)", num);
	return SALVAGE_RN_UNUSED;
    }


    CHECK_VALUE_EQ(entry->u.dir.InterfaceVersion,1, "InterfaceVersion",mod);
    mod |= check_value_bitmask(&entry->u.dir.UnixModeBits, 04777, 07777,
			       "Unix dir rights");
    
    CHECK_VALUE_EQ(entry->u.dir.FileType,TYPE_DIR,"FileType",mod);

    node = allocate_node (info, dir_local2afs(num), TYPE_DIR);
    node->inode_num 	= dir_local2afs (num);
    node->parent_num 	= entry->u.dir.ParentVnode;
    node->parent_unique	= entry->u.dir.ParentUnique;
    if (entry->u.dir.nextptr != VOLDB_ENTRY_USED)
	node->status	= IUNUSED;
    else
	node->status	= IFOUND;
    node->filetype	= TYPE_DIR;
    
    *ret_node = node;

    if (mod)
	return SALVAGE_RN_WRITE;
    return 0;
}

/*
 *
 */

static int
read_nodes_file (uint32_t num, struct voldb_entry *entry,
		 struct vinfo *info, struct inodeinfo **ret_node)
{
    struct inodeinfo *node;
    int mod = 0;

    if (entry->u.file.FileType == 0 ||
	entry->u.file.nextptr != VOLDB_ENTRY_USED)
    {
	mlog_log (MDEBSALVAGE, "%d is a unused node (file)", num);
	return 0; /* XXX ? */
    }
    
    CHECK_VALUE_EQ(entry->u.file.InterfaceVersion,1, "InterfaceVersion",mod);
    /* Remove sgid and sticky bit */
    mod |= check_value_bitmask(&entry->u.file.UnixModeBits, 04777, 07777,
			       "Unix file rights");

    switch (entry->u.file.FileType) {
    case TYPE_FILE:
    case TYPE_LINK:
	break;
    default:
	mlog_log (MDEBSALVAGE, "File type incorrect, resetting");
	entry->u.file.FileType = TYPE_FILE;
	mod |= 1;
    }

    node = allocate_node (info, file_local2afs(num), TYPE_FILE);
    node->inode_num 	= file_local2afs(num);
    node->parent_num 	= entry->u.file.ParentVnode;
    node->parent_unique	= entry->u.file.ParentUnique;
    if (entry->u.file.nextptr != VOLDB_ENTRY_USED)
	node->status	= IUNUSED;
    else
	node->status	= IFOUND;
    node->filetype	= entry->u.file.FileType;

    if (mod)
	return SALVAGE_RN_WRITE;
    return 0;
}

/*
 *
 */

static int
read_nodes (struct volume_handle *volh, struct voldb *db,
	    uint32_t size, struct vinfo *info,
	    int (*func) (uint32_t, struct voldb_entry *, struct vinfo *,
			 struct inodeinfo **))
{
    uint32_t i;
    int ret;
    int changed = 0;
    int check_data;
    struct inodeinfo *node;

    for (i = 0; i < size; i++) {
	struct voldb_entry entry;
	
	ret = voldb_get_entry (db, i, &entry);
	if (ret) {
	    mlog_warn (MDEBSALVAGE, ret, "tree_connectivity: "
		       "read_nodes: get_entry failed: %d.%d",
		       voldb_get_volume(db), i);
	    continue;
	}

	check_data = 0;
	node = NULL;
	ret = (*func) (i, &entry, info, &node);
	switch (ret) {
	case 0:
	    check_data = 1;
	    break;
	case SALVAGE_RN_UNUSED:
	    break;
	case SALVAGE_RN_WRITE:
	    ret = voldb_put_entry (db, i, &entry);
	    if (ret) {
		mlog_warn (MDEBSALVAGE, ret, "tree_connectivity: "
			   "read_nodes: put_entry failed %d.%d", 
			   voldb_get_volume(db), i);
	    }
	    changed = 1;
	    check_data = 1;
	    break;
	default:
	    abort();
	}
	if (check_data && node) {
	    ret = opennode (volh, info, node, NULL);
	    if (ret) {
		mlog_log (MDEBSALVAGE,
			  "reading node %u.%u: failed to open node", 
			  VLD_VOLH_NUM(volh), node->inode_num);
		node->flags.no_data = 1;
	    }
	}
    }
    if (changed)
	voldb_flush (db);
    return 0;
}

/*
 *
 */

static int
remove_node (struct volume_handle *volh, struct inodeinfo *node)
{
    uint32_t ino;
    struct voldb *db;
    
    if (afs_dir_p (node->inode_num)) {
	ino = dir_afs2local(node->inode_num);
	db = VLD_VOLH_DIR(volh);
    } else {
	ino = file_afs2local(node->inode_num);
	db = VLD_VOLH_FILE(volh);
    }
    return voldb_del_entry (db, ino, NULL);
}

/*
 *
 */

struct dir_func_s {
    struct inodeinfo *parent;
    struct vinfo *info;
    fbuf *parent_fbuf;
    struct volume_handle *volh;
};

static int
check_dir_func (VenusFid *fid, const char *name, void *arg)
{
    struct dir_func_s *f = (struct dir_func_s *)arg;
    int ret;
    
    /*
     * XXX to make sure that the list node->childp and the content of
     * the directory is right
     */

    mlog_warnx (MDEBSALVAGE, "check_dir_func: %s %u", name, fid->fid.Vnode);

    if (strcmp (name, ".") == 0) {
	f->parent->flags.dot = 1;
	if (f->parent->inode_num != fid->fid.Vnode)
	    f->parent->flags.invalid_dot = 1;
    } else if (strcmp (name, "..") == 0) {
	f->parent->flags.dotdot = 1;
	if (f->parent->parent_num != fid->fid.Vnode
	    || f->parent->parent_unique != fid->fid.Unique)
	    f->parent->flags.invalid_dotdot = 1;
    } else if (strncmp (name, ".__afs", 6) == 0) {
	struct inodeinfo *child;
	ret = fdir_remove (f->parent_fbuf, name, NULL);
	assert (ret == 0);
	child = find_node (f->info, fid->fid.Vnode);
	if (child)
	    remove_node (f->volh, child);
    } else {
	struct inodeinfo *child;

	child = find_node (f->info, fid->fid.Vnode);
	if (child == NULL || child->flags.no_data) {
	    mlog_log (MDEBSALVAGE,
		      "name `%s' found in %d.%d w/o node, removing",
		      name, fid->fid.Volume, f->parent->inode_num);
	    ret = fdir_remove (f->parent_fbuf, name, NULL);
	    if (ret != 0)
		mlog_log (MDEBSALVAGE, 
			  "removal of lost name `%s' failed in %d.%d with %d",
			  name, fid->fid.Volume, f->parent->inode_num, ret);
	    if (child)
		remove_node (f->volh, child);
	} else {
	    child->status = IDIR;

	    ++child->num_indir;

	    if (strchr (name, '/')) {
		abort(); /* XXX check name better */
	    }
	}
    }
    return 0;
}

/*
 *
 */

static int
check_content_dir_func (volume_handle *vol,
			struct vinfo *info,
			struct inodeinfo *node, 
			int fd,
			size_t size)
{
    VenusFid fid;
    fbuf the_fbuf;
    struct dir_func_s f;
    int ret;

    if ((size % AFSDIR_PAGESIZE) != 0) {
	mlog_log (MDEBSALVAGE, "check_content_dir_func: dir has wrong size");
	ret = ftruncate (fd, size / AFSDIR_PAGESIZE);
	if (ret != 0) {
	    mlog_log (MDEBSALVAGE,
		      "check_content_dir_func: ftruncate: %d",
		      errno);
	    return 1;
	}
    }

    if (node->inode_num == 1)
	node->status = IDIR;

    fid.Cell = 0;
    fid.fid.Volume = VLD_VOLH_NUM(vol);

    f.info   = info;
    f.parent = node;
    f.parent_fbuf = &the_fbuf;
    f.volh   = vol;

    ret = fbuf_create (&the_fbuf, fd, size,
		       FBUF_READ|FBUF_WRITE|FBUF_SHARED);

    ret = fdir_readdir (&the_fbuf, check_dir_func, &f, fid, NULL);
    if (ret)
	mlog_log (MDEBSALVAGE, "check_content_dir_func: fbuf_readdir failed");

    fbuf_end (&the_fbuf);
    return ret;
}

/*
 * Check that the `node' in `volume' have a sane content.
 */

static int
check_dir (volume_handle *vol, struct vinfo *info, struct inodeinfo *node)
{
    int ret;

    ret = opennode (vol, info, node, check_content_dir_func);
    if (ret) {
	mlog_log (MDEBSALVAGE, "check_dir failed with %d", ret);
	return 1;
    }
    return 0;
}

/*
 *
 */

static int
add_node_lf (struct volume_handle *volh,
	     struct inodeinfo *node)
{
    mlog_log (MDEBSALVAGE, "XXX add node to lost and found %u.%u",
	      voldb_get_volume (VLD_VOLH_DIR(volh)), node->inode_num);
#if 0
    /* XXX */
    abort();
#endif
    return 0;
}

/*
 *
 */

static int
readd_node (struct volume_handle *volh,
	    struct inodeinfo *node)
{
    mlog_log (MDEBSALVAGE, "XXX add node to tree again %u.%u",
	      voldb_get_volume (VLD_VOLH_DIR(volh)), node->inode_num);
#if 0
    /* XXX */
    abort();
#endif
    return 0;
}

/*
 *
 */

static void
find_and_readd_node (struct volume_handle *volh, 
		     struct vinfo *info,
		     int foundnodes)
{
    int i;

    for (i = 0; i < foundnodes; i++) {
	switch (info->nodes[i]->status) {
	case IDIR:
	    if (info->nodes[i]->filetype == TYPE_DIR)
		/* XXX check tree consistency */
		;
	    /* XXX check linkcount */
	    break;
	case ITREE:
	    if (readd_node (volh, info->nodes[i]))
		add_node_lf (volh, info->nodes[i]);
	    break;
	case IUNUSED:
	    break;
	case IFOUND:
	default:
	    abort();   
	}
    }
}

/*
 *
 */

static void
mark_dir_node (struct volume_handle *volh, 
	       struct vinfo *info,
	       int foundnodes)
{
    int i;

    for (i = 0; i < foundnodes; i++) {
	switch (info->nodes[i]->status) {
	case IDIR:
	case ITREE:
	    if (info->nodes[i]->filetype == TYPE_DIR)
		check_dir (volh, info, info->nodes[i]);
	    break;
	case IUNUSED:
	    break;
	case IFOUND:
	default:
	    abort();   
	}
    }
}

/*
 *
 */

static void
find_list_tree_nodes (struct volume_handle *volh,
		      struct vinfo *info,
		      int foundnodes)
{
    int i;
    int db_flush = 0;
    
    for (i = 0; i < foundnodes; i++) {
	switch (info->nodes[i]->status) {
	case ITREE:
	    if (info->nodes[i]->flags.no_data) {
		remove_node (volh, info->nodes[i]);
		db_flush = 1;
	    }
	    break;
	case IFOUND:
	    /* 
	     * this is a lost node 
	     *  XXX attach it to the lost+found directory 
	     */
	    
	    mlog_log (MDEBSALVAGE, "lost inode found");

	    /* XXX make sure is on the lfnodes list */

	    if (info->nodes[i]->flags.no_data) {
		remove_node (volh, info->nodes[i]);
		db_flush = 1;
	    } else if (readd_node (volh, info->nodes[i])) {
		add_node_lf (volh, info->nodes[i]);
	    }

	    break;
	case IUNUSED:
	    break;
	case IDIR:
	default:
	    abort();   
	}
    }
    if (db_flush)
	vld_db_flush (volh);
}

/*
 *
 */

int
salvage_volume (struct volume_handle *volh)
{
    uint32_t dsize, fsize;
    uint32_t foundnodes;
    int ret, i;
    struct vinfo info;
    struct inodeinfo *lfnodes;

    memset (&info, 0, sizeof(info));

    /* XXX check volintInfo */

    ret = vld_db_uptodate (volh);
    if (ret) {
	mlog_warnx (MDEBERROR, "tree_connectity: vld_db_uptodate"
		    "failed with %d on volume %u",
		    ret, VLD_VOLH_NUM(volh));
	return ret;
    }

    ret = voldb_header_info(VLD_VOLH_DIR(volh), &dsize, NULL);
    if (ret) {
	mlog_warnx (MDEBERROR, "tree_connectity: voldb_header_info "
		    "failed with %d on volume %u (dir)",
		    ret, VLD_VOLH_NUM(volh));
	return ret;
    }

    ret = voldb_header_info(VLD_VOLH_FILE(volh), &fsize, NULL);
    if (ret) {
	mlog_warnx (MDEBERROR, "tree_connectity: voldb_header_info "
		    "failed with %d on volume %u (file)",
		    ret, VLD_VOLH_NUM(volh));
	return ret;
    }

    ret = read_nodes (volh,
		      VLD_VOLH_DIR(volh),
		      dsize,
		      &info,
		      read_nodes_dir);
    if (ret)
	abort(); /* XXX */
    ret = read_nodes (volh,
		      VLD_VOLH_FILE(volh),
		      fsize,
		      &info,
		      read_nodes_file);
    if (ret)
	abort(); /* XXX */

    mlog_log (MDEBSALVAGE, 
	      "tree_connectvity: status: found %d dirs, %d files",
	      info.dsize, info.fsize);

    foundnodes = info.dsize + info.fsize;

    for (i = 0; i < foundnodes; i++) {
	struct inodeinfo *p, *n = info.nodes[i] ;

	/* Skip root node */
	if (i == 0) {
	    n->status = ITREE;
	    continue;
	}
	
	p = find_node (&info, n->parent_num);
	if (p == NULL) {
	    mlog_log (MDEBSALVAGE, "tree_connectivity: lost node %u.%u",
		      VLD_VOLH_NUM(volh), i);
	    info.nodes[i]->lfnodes = lfnodes;
	    lfnodes = info.nodes[i];
	} else {
	    if (n->parentp) {
		struct inodeinfo **old_p = &n->parentp->childp;

		/* 
		 * Old parent, obviois something wrong. Let's remove
		 * the node from the old parent.
		 */
 
		mlog_log (MDEBSALVAGE, "tree_connectivity: have a old parent");
		while (*old_p != NULL || *old_p != n)
		    old_p = &(*old_p)->siblingp;
		if (*old_p == NULL) /* Not on old parent\s child list, wrong */
		    abort();
		*old_p = (*old_p)->siblingp;
	    }
	    n->status = ITREE;
	    n->siblingp = p->childp;
	    p->childp = n;
	    n->parentp = p;
	}
    }

    find_list_tree_nodes (volh, &info, foundnodes);
    mark_dir_node (volh, &info, foundnodes);
    find_and_readd_node (volh, &info, foundnodes);

    free_vinfo (&info);
    return 0;
}

