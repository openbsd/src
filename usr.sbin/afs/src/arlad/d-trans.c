/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "arla_local.h"
#ifdef RCSID
RCSID("$arla: d-trans.c,v 1.2 2002/09/07 10:43:01 lha Exp $") ;
#endif


struct transform_fid {
    VenusFid dfid; /* disconnected fid */
    VenusFid cfid; /* connecteted fid */
};

/*
 *
 */

static Hashtab *transform_htab;

/*
 * Transaction fid related functions
 */

/*
 * Find disconnected fid `dfid' in the transaction database.
 */

static struct transform_fid *
find_fid_transform(VenusFid *dfid)
{
    struct transform_fid key;
    key.dfid = *dfid;
    return hashtabsearch(transform_htab, &key);
}

/*
 * Transform the `fid` to a connected fid, return the pointer to `fid'
 * so it can be used in a paramater to a function call.
 */

static VenusFid *
transform_fid(VenusFid *fid)
{
    struct transform_fid *t = find_fid_transform(fid);
    if (t)
	*fid = t->cfid;
    return fid;
}

/*
 * Add a mapping between the disconnected fid `dfid' and the connected
 * fid `cfid' in the transaction database.
 */

static void
add_fid_transform(VenusFid *dfid, VenusFid *cfid)
{
    struct transform_fid *f = find_fid_transform(dfid);
    if (f)
	abort();

    f = malloc(sizeof(*f));
    if (f == NULL)
	abort();
    
    f->cfid = *cfid;
    f->dfid = *dfid;
    
    hashtabadd(transform_htab, f);
}

/*
 * Help function for hashtable, compare `p1' and `p2'.
 */

static int
tfid_cmp(void *p1, void *p2)
{
    struct transform_fid *t1 = (struct transform_fid *)p1;
    struct transform_fid *t2 = (struct transform_fid *)p2;
    return VenusFid_cmp(&t1->dfid, &t2->dfid);
}

/*
 * Help function for hashtable, calculate hash for `p'.
 */

static unsigned
tfid_hash(void *p)
{
    struct transform_fid *t = (struct transform_fid *)p;
    return t->dfid.Cell + t->dfid.fid.Volume + t->dfid.fid.Vnode 
	+ t->dfid.fid.Unique;
}

static Bool
tfid_free(void *p, void *arg)
{
    struct transform_fid *t = (struct transform_fid *)p;
    memset(t, 0, sizeof(t));
    free(t);
    return TRUE;
}


/*
 * Reintegrate the log
 */

int
disco_reintegrate(nnpfs_pag_t pag)
{
    struct disco_play_context *c;
    char buf[DISCO_MAX_BUF_SZ];
    CredCacheEntry *ce;
    FCacheEntry *dir_entry, *child_entry;
#if 0
    VenusFid dir_fid;
#endif 
    VenusFid child_fid;
    AFSStoreStatus store_status;
    AFSFetchStatus fetch_status;
    int ret, store_data;
    int old_conn_mode = connected_mode;

    transform_htab = hashtabnewf(0, tfid_cmp, tfid_hash, HASHTAB_GROW);
    if (transform_htab == NULL)
	return ENOMEM;

#if 0
    modified_log = 0; /* XXX */
#endif

    disco_openlog();

    connected_mode = CONNECTED;

    disco_init_context(&c);

    while(disco_next_entry(c, buf, sizeof(buf)) == 0) {
	struct disco_header *h = (struct disco_header *)buf;

	if (h->opcode >= DISCO_OP_MAX_OPCODE)
	    abort();

	dir_entry = NULL;
	child_entry = NULL;
	ce = NULL;

	if (h->flags & DISCO_HEADER_NOP)
	    goto next_entry;

	switch(h->opcode) {
	case DISCO_OP_UNLINK:
	    break;
	case DISCO_OP_REMOVE_DIR:
	    break;
	case DISCO_OP_CREATE_FILE: {
	    struct disco_create_file *f = (struct disco_create_file *)buf;

	    store_data = 0;

	    transform_fid(&f->parentfid);

	    ce = cred_get(f->parentfid.Cell, pag, CRED_ANY);
	    assert(ce);

	    ret = fcache_find(&dir_entry, f->parentfid);
	    if (ret != 0) {
		arla_warnx(ADEBERROR, "create failed: (parent)"
			   "parent fid %d.%d.%d.%d name %s",
			   f->parentfid.Cell, f->parentfid.fid.Volume,
			   f->parentfid.fid.Vnode, f->parentfid.fid.Unique,
			   f->name);
		goto next_entry;
	    }

	    transform_fid(&f->fid);

	    ret = fcache_find(&child_entry, f->fid);
	    if (ret != 0) {
		arla_warnx(ADEBERROR, "create failed (child): "
			   "parent fid %d.%d.%d.%d name %s",
			   f->parentfid.Cell, f->parentfid.fid.Volume,
			   f->parentfid.fid.Vnode, f->parentfid.fid.Unique,
			   f->name);
		goto next_entry;
		
	    }

	    /* XXX make summery of chain */
	    store_status = f->storestatus; /* XXX */

	    ret = create_file(dir_entry, f->name, &store_status, &child_fid,
			      &fetch_status, ce);

	    if (ret == 0) {
		if (VenusFid_cmp(&f->fid, &child_fid) != 0) {
		    add_fid_transform(&f->fid, &child_fid);

		    recon_hashtabdel(child_entry);
		    child_entry->fid = child_fid;
		    update_fid (f->fid, NULL, child_fid, child_entry);
		    recon_hashtabadd(child_entry);

		    adir_changefid(&dir_entry, f->name, &child_fid, &ce);
		}

		if (store_data)
		    store_data = 1;

	    } else {
		/* XXX invalidate name cache */
		arla_warn(ADEBWARN, ret, "create failed (create_file): "
			  "parent fid %d.%d.%d.%d name %s",
			  f->parentfid.Cell, f->parentfid.fid.Volume,
			  f->parentfid.fid.Vnode, f->parentfid.fid.Unique,
			  f->name);
		
	    }
	    /* XXX invalidate the parent's name cache */
	    break_callback(dir_entry);

	    break;
	}
	case DISCO_OP_CREATE_SYMLINK:
	case DISCO_OP_CREATE_LINK:
	    break;
	case DISCO_OP_STOREDATA:
	    break;
	}

    next_entry:

	if (dir_entry)
	    fcache_release(dir_entry);
	if (child_entry)
	    fcache_release(child_entry);
	if (ce)
	    cred_free(ce);

	arla_warnx(ADEBWARN, "next reintegrate entry");
    }

    disco_close_context(c);

    disco_closelog();


    hashtabcleantab(transform_htab, tfid_free, NULL);
    hashtabrelease(transform_htab);
    transform_htab = NULL;

    connected_mode = old_conn_mode;

    return 0;
}
