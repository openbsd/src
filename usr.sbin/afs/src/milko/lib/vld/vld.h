/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

/* $arla: vld.h,v 1.32 2002/02/07 17:59:52 lha Exp $ */

#ifndef MILKO_VLD_H
#define MILKO_VLD_H 1

#include <dpart.h>
#include <voldb.h>

#include <fs.h>
#include <volumeserver.h>

#include <fbuf.h>
#include <list.h>

#include <mnode.h>

/*
 * Type of volumes
 */

#define VLD_SVOL 0
#define VLD_FVOL 1

/*
 * structures
 */

#define VLD_VOLH_DIR(volh)	((volh)->db[0])
#define VLD_VOLH_FILE(volh)	((volh)->db[1])

#define VLD_VOLH_NUM(volh)	((volh)->vol)

typedef struct volume_handle {
    int type;			/* type of volume backstore */
    int voldbtype;		/* type of voldb structures */
    uint32_t vol;		/* volume number */
    int ref;			/* refcount */
    struct dp_part *dp;		/* on what partition the volume resides */
    void *data;			/* data blob for volume type */
    volintInfo info;		/* info struct */
    struct {
	unsigned infop:1;	/* if there is valid info in node */
	unsigned voldbp:1;	/* if voldb has been read in */
	unsigned offlinep:1;  /* if volume is offline */
	unsigned salvaged:1;  /* if volume is salvaged */
	unsigned attacherr:1; /* volume had an error when attaching */
	unsigned cleanp:1;	/* volume was cleanly mounted */
    } flags ;
    onode_opaque sino;		/* inode number of volume entry  */
    onode_opaque dino;		/* inode number of db of dirs */
    onode_opaque fino;		/* inode number of db of files */
    struct voldb *db[2];	/* large and small mnode tables */
    Listitem *db_li;		/* position in db_lru */
    Listitem *li;		/* position on the vol_list */
} volume_handle;

struct trans {
    int32_t tid;
    int32_t time; /* last active */
    int32_t creationTime;
    int32_t returnCode;
    volume_handle *volume;
    int32_t volid;
    int32_t partition;
    int16_t refCount;
    int16_t iflags;
    int8_t vflags;
    int8_t tflags;
    int8_t incremental;
};

typedef enum { NODE_REG = 1, NODE_DIR = 2,
	       NODE_META = 3, NODE_VOL = 4 } node_type;

typedef struct volume_operations {
    char *name;
    int  (*open)(struct dp_part *part, int32_t volid, 
		 int flags, void **vol);
#define VOLOP_NOFLAGS 0x0
#define VOLOP_CREATE 0x1
    void (*free)(volume_handle *vol);
    int  (*icreate)(volume_handle *vol, onode_opaque *o, node_type type,
		    struct mnode *node);
    int  (*iopen)(volume_handle *vol, onode_opaque *o, int flags, int *fd);
    int  (*iunlink)(volume_handle *vol, onode_opaque *o); 
    int  (*remove)(volume_handle *vol);
} vol_op;

enum { VOLCREAT_NOOP = 0,		/* noop */
       VOLCREAT_CREATE_ROOT = 1		/* create rootnode */
};
       

int
vld_boot (void);

int
vld_init (void);

void
vld_iter_vol (int (*func)(volume_handle *vol, void *arg), void *arg);

void
vld_free (volume_handle *vol);

void
vld_ref (volume_handle *vol);

int
vld_register_backstore_type (const int32_t backstoretype, vol_op *ops);

int
vld_create_trans(int32_t partition, int32_t volume, int32_t *trans);

int
vld_get_trans(int32_t transid, struct trans **trans);

int
vld_put_trans(struct trans *trans);

int
vld_verify_trans(int32_t trans);

int
vld_check_busy(int32_t volid, int32_t partition);

int
vld_trans_set_iflags(int32_t trans, int32_t iflags);

int
vld_trans_set_vflags(int32_t trans, int32_t vflags);

int
vld_trans_get_vflags(int32_t trans, int32_t *vflags);

int
vld_register_vol_type (const int32_t backstoretype, vol_op *ops);

int
vld_end_trans(int32_t trans, int32_t *rcode);

int
vld_find_vol (const int32_t volid, struct volume_handle **vld);

int
vld_create_volume (struct dp_part *dp, int32_t volid, 
		   const char *name, int32_t backstoretype, int32_t voltype,
		   int flags);

const char *
vld_backstoretype_name (int32_t backstoretype);

int
vld_vld2volsync (const struct volume_handle *vld, AFSVolSync *volsync);

int
vld_open_volume_by_num (struct dp_part *dp, int32_t volid,
			volume_handle **vol);

int
vld_open_volume_by_fd (struct dp_part *dp, int fd,
		       volume_handle **vol);

int
vld_open_inode (volume_handle *vol, onode_opaque *o, int flags, int *fd);

int
vld_info_uptodatep (volume_handle *vol);

int
vld_info_write (volume_handle *vol);

int
vld_db_uptodate (volume_handle *vol);

int
vld_open_vnode (volume_handle *vol, struct mnode *n, struct msec *m);

int
vld_modify_vnode (volume_handle *vol, struct mnode *n, struct msec *m,
		  const AFSStoreStatus *ss, int32_t *len);

int
vld_put_acl (volume_handle *vol, struct mnode *n, struct msec *m);

int
vld_check_rights (volume_handle *vol, struct mnode *n,
		  struct msec *m);

int
vld_adjust_linkcount (volume_handle *vol, struct mnode *n, int adjust);

int
vld_set_onode (volume_handle *vol, int32_t vno, onode_opaque *new,
	       onode_opaque *old);

int
vld_create_entry (volume_handle *vol, struct mnode *parent, AFSFid *child,
		  int type, const AFSStoreStatus *ss, struct mnode **ret_n,
		  struct msec *m);

int
vld_remove_node (volume_handle *vol, struct mnode *n);

int
vld_db_flush (volume_handle *vol);

int
vld_list_volumes(struct dp_part *dp, List **retlist);

int
vld_delete_volume (struct dp_part *dp, int32_t volid, 
		   int32_t backstoretype,
		   int flags);

void
vld_end (void);

int
vld_fvol_create_volume_ondisk (struct dp_part *dp, int32_t volid,
			       const char *path);

int
vld_get_volstats (volume_handle *vol,
		  struct AFSFetchVolumeStatus *volstat,
		  char *volName,
		  char *offLineMsg,
		  char *motd);

int
vld_set_volstats (volume_handle *vol,
		  const struct AFSStoreVolumeStatus *volstat,
		  const char *volName,
		  const char *offLineMsg,
		  const char *motd);

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
	     int32_t *acl);

int
vld_rebuild (struct volume_handle *vol);

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
		 void *arg);

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
		  void *arg);

/*
 * This function is internal to vld and vol modules.
 *
 * Use otherwise and die.
 */

int
volop_iopen (volume_handle *vol, onode_opaque *o, struct mnode *n);

#endif /* MILKO_VLD_H */

