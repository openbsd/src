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

/*
 * Keep our own root.afs
 *
 * uses cell DYNROOTCELL as cell number.
 */

#include <arla_local.h>

RCSID("$KTH: dynroot.c,v 1.13.2.2 2001/05/28 15:19:20 map Exp $");

struct create_entry {
    fbuf *thedir;	/* pointer to the fbuf that contains the dir */
    AFSFid fid;		/* the current fid */
    int len;		/* num of links in the dir */
};

#define DYNROOT_ROOTVOLUME 1
#define DYNROOT_ROOTDIR 1
#define DYNROOT_UNIQUE 1

static int32_t dynrootcell = 0;			/* this is the dynroocell */
static Bool dynroot_enable  = 0;		/* is dynroot enabled ? */
static unsigned long last_celldb_version = 0;	/* last version of celldb */

/*
 * Magic glue wrt afsvnode#
 */

static int32_t
cellnum2afs (int cellno)
{
    return (cellno << 1) + 1;
}

static int
afs2cellnum (int32_t afsvnode)
{
    return (afsvnode - 1) >> 1;
}

/*
 * helper functions for dynroot_create_root that for
 * each `cell' creates a entry in the root directory.
 */

static int
create_entry_func (const cell_entry *cell, void *arg)
{
    struct create_entry *entry = (struct create_entry *) arg;
    int ret;

    if (!cell_dynroot(cell))
	return 0;

    entry->fid.Vnode = cellnum2afs (cell->id);

    ret = fdir_creat (entry->thedir, cell->name, entry->fid);
    if (ret)
	return ret;

    entry->len++;

    return 0;
}

/*
 * create the dynroot root directory in `fbuf', return number
 * of entries in `len'.
 */

static int
dynroot_create_root (fbuf *fbuf, size_t *len)
{
    int ret;
    AFSFid dot = { DYNROOT_ROOTVOLUME,
		   DYNROOT_ROOTDIR,
		   DYNROOT_UNIQUE};
    struct create_entry entry;

    ret = fdir_mkdir (fbuf, dot, dot);
    if (ret)
	return ret;

    entry.thedir	= fbuf;

    entry.fid.Volume	= DYNROOT_ROOTVOLUME;
    entry.fid.Vnode	= DYNROOT_ROOTDIR + 2;
    entry.fid.Unique	= DYNROOT_UNIQUE;
    entry.len = 0;
    
    ret = cell_foreach (create_entry_func, &entry);
    if (ret)
	return ret;

    *len = entry.len;

    return 0;
}

/*
 * for the `vnode' create apropriate symlink in `fbuf'
 */

static int
dynroot_create_symlink (fbuf *fbuf, int32_t vnode)
{
    char name[MAXPATHLEN];
    cell_entry *cell;
    int len, ret;

    cell = cell_get_by_id (afs2cellnum (vnode));
    if (cell == NULL)
	return ENOENT;

    len = snprintf (name, sizeof(name), "#%s:root.cell.", cell->name);
    assert (len > 0 && len <= sizeof (name));

    ret = fbuf_truncate (fbuf, len);
    if (ret)
	return ret;

    memmove (fbuf_buf(fbuf), name, len);
    return 0;
}

/*
 * Return TRUE if the combination `cell' and `volume' is
 * in the dynroot.
 */

Bool
dynroot_isvolumep (int cell, const char *volume)
{
    assert (volume);
    
    if (cell == 0 && strcmp (volume, "1") == 0)
	return TRUE;

    return FALSE;
}

/*
 * Create a dummy nvldbentry in `entry'
 */

int
dynroot_fetch_vldbN (nvldbentry *entry)
{
    memset (entry, 0, sizeof(*entry));

    strlcpy(entry->name, "root.cell", sizeof(entry->name));
    entry->nServers = 0;
    entry->volumeId[RWVOL] = DYNROOT_ROOTVOLUME;
    entry->flags = VLF_RWEXISTS;

    return 0;
}

/*
 * Update `entry' to contain the correct information
 * Note: doesn't update status.Length and status.LinkCount
 */

static void
dynroot_update_entry (FCacheEntry *entry, int32_t filetype,
		      xfs_pag_t cred)
{
    struct timeval tv;
    AccessEntry *ae;

    assert (entry);
    entry->status.InterfaceVersion = 1;
    entry->status.FileType	= filetype;
    entry->status.DataVersion	= 1;
    entry->status.Author	= 0;
    entry->status.Owner		= 0;
    entry->status.CallerAccess 	= ALIST | AREAD;
    entry->status.AnonymousAccess = ALIST | AREAD;
    switch (filetype) {
    case TYPE_DIR: 
	entry->status.UnixModeBits = 0755;
	break;
    case TYPE_LINK:
	entry->status.UnixModeBits = 0644;
	break;
    default:
	abort();
    }
    entry->status.ParentVnode	= DYNROOT_ROOTDIR;
    entry->status.ParentUnique	= DYNROOT_UNIQUE;
    entry->status.SegSize	= 64*1024;
    entry->status.ClientModTime	= 0;
    entry->status.ServerModTime	= 0;
    entry->status.Group		= 0;
    entry->status.SyncCount	= 0;
    entry->status.spare1	= 0;
    entry->status.spare2	= 0;
    entry->status.spare3	= 0;
    entry->status.spare4	= 0;

    gettimeofday (&tv, NULL);

    memset (&entry->volsync, 0, sizeof (entry->volsync));

    entry->callback.CallBackVersion = 1;
    entry->callback.ExpirationTime = tv.tv_sec + 3600 * 24 * 7;
    entry->callback.CallBackType = CBSHARED;

    entry->anonaccess = entry->status.AnonymousAccess;

    findaccess(cred, entry->acccache, &ae);
    ae->cred = cred;
    ae->access = entry->status.CallerAccess;
}

/*
 * Fetch data and attr for `entry'
 */

static int
dynroot_get_node (FCacheEntry *entry, CredCacheEntry *ce)
{
    int ret, fd, rootnode;
    size_t len;
    fbuf dir;

    rootnode = entry->fid.fid.Vnode == DYNROOT_ROOTDIR ? 1 : 0;

    if (entry->flags.attrp &&
	entry->flags.datap &&
	(!rootnode || last_celldb_version == cell_get_version()))
	return 0;

    fd = fcache_open_file (entry, O_RDWR);
    if (fd < 0)
	return errno; 

    ret = fbuf_create (&dir, fd, 0, FBUF_READ | FBUF_WRITE | FBUF_SHARED);
    if (ret) {
	close (fd);
	return ret;
    }    

    if (rootnode) {
	ret = dynroot_create_root (&dir, &len);
	entry->status.LinkCount = len;
    } else {
	ret = dynroot_create_symlink (&dir, entry->fid.fid.Vnode);
	entry->status.LinkCount = 1;
	fcache_mark_as_mountpoint (entry);
    }

    if (ret) {
	fbuf_end (&dir);
	close(fd);
	return ret;
    }

    entry->status.Length 	= dir.len;
    entry->length	 	= dir.len;

    ret = fbuf_end (&dir);
    close(fd);
    if (ret)
	return ret;

    dynroot_update_entry (entry, rootnode ? TYPE_DIR : TYPE_LINK,
			  ce->cred);

    entry->flags.attrp = TRUE;
    entry->flags.datap = TRUE;

    entry->tokens |= XFS_ATTR_R|XFS_DATA_R;

    return 0;
}

/*
 * Fetch attr for `entry'
 */

int
dynroot_get_attr (FCacheEntry *entry, CredCacheEntry *ce)
{
    return dynroot_get_node (entry, ce);
}


/*
 * Fetch data for `entry'
 */

int
dynroot_get_data (FCacheEntry *entry, CredCacheEntry *ce)
{
    return dynroot_get_node (entry, ce);
}

/*
 * returns TRUE if `entry' is a dynroot entry.
 */

Bool
dynroot_is_dynrootp (FCacheEntry *entry)
{
    assert (entry);

    if (dynroot_enable &&
	entry->fid.Cell == dynrootcell &&
	entry->fid.fid.Volume == DYNROOT_ROOTVOLUME)
	return TRUE;

    return FALSE;
}

/*
 * Return what status the dynroot is in.
 */

Bool
dynroot_enablep (void)
{
    return dynroot_enable;
}

/*
 * Enable/Disable the dynroot depending on `enable', returns previous state.
 */

Bool
dynroot_setenable (Bool enable)
{
    Bool was = dynroot_enable;
    dynroot_enable = enable;
    return was;
}

/*
 * Returns the dynroot_cellid.
 */

int32_t dynroot_cellid (void)
{
    return dynrootcell;
}

/*
 * Return the dynroot volumeid.
 */

int32_t dynroot_volumeid (void)
{
    return DYNROOT_ROOTVOLUME;
}
