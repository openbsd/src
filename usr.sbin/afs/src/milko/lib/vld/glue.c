/*
 * Copyright (c) 1999, 2000 Kungliga Tekniska Högskolan
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
 * This is glue between voldb and some vol-layer parts when there
 * isn't a clean way to create them.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>

#include <assert.h>
#include <dirent.h>

#include <vldb.h>

#include <voldb.h>
#include <vld.h>
#include <vld_ops.h>

#include <vstatus.h>
#include <voldb.h>
#include <fvol.h>

#include <mdir.h>
#include <err.h>
#include <errno.h>
 
RCSID("$arla: glue.c,v 1.4 2000/10/03 00:19:01 lha Exp $");

/*
 *
 */

static int
add_entry (volume_handle *vol, const char *path, int32_t offset, 
	   struct mnode *parent, AFSFid *fid,
	   struct stat *sb, int type, const char *name)
{
    int ret;
    AFSStoreStatus ss;
    struct mnode *child;
    
    memset (&ss, 0, sizeof(ss));
    ss.Mask = SS_MODTIME|SS_OWNER|SS_GROUP|SS_MODEBITS;
    ss.ClientModTime = sb->st_mtime;
    ss.UnixModeBits = sb->st_mode;
    ss.Owner = sb->st_uid;
    ss.Group = sb->st_gid;
    
    ret = vld_create_entry (vol, parent, fid, type, &ss, &child, NULL);
    assert (ret == 0);

    if (type == TYPE_FILE || type == TYPE_LINK) {
	onode_opaque opaque;
	
	fvol_offset2opaque (offset, &opaque);

	ret = vld_set_onode (vol, fid->Vnode, &opaque, NULL);
	assert (ret == 0);
    }

    ret = mdir_creat (parent, name, *fid);
    assert (ret == 0);

    return 0;
}

static int
add_file (volume_handle *vol, const char *path,
	  struct mnode *parent, AFSFid *fid, struct stat *sb,
	  int type, const char *name)
{
    int ret;
    int32_t offset;

    ret = fvol_addfile ((struct fvol *)vol->data, path, &offset);
    if (ret) return ret;

    ret = add_entry (vol, path, offset, parent, fid, sb, type, name);
    if (ret)
	return ret;

    return 0;
} 

/*
 *
 */

#if 0
#ifndef S_ISLNK
#ifdef S_IFLNK
#define S_ISLNK(mode) (((mode) & S_IFLNK) == S_IFLNK)
#endif /* S_IFLNK */
#define S_ISLNK(mode) 0
#endif /* I_ISLNK */
#endif

static int
add_tree (volume_handle *vol, struct mnode *parent, const char *name)
{
    struct voldb_entry e;
    char p[MAXPATHLEN];
    struct dirent *dp;
    struct stat sb;
    int ret, fd;
    AFSFid fid;
    DIR *dir;

    if (parent->flags.fdp == FALSE) {
	ret = voldb_get_entry (vol->db[0],
			       dir_afs2local (parent->fid.Vnode),
			       &e); 
	if (ret) return ret;
	parent->flags.fdp = TRUE;

	ret = VOLOP_IOPEN(vol,&e.u.dir.ino, O_RDWR, &fd);
	if (ret)
	    return ret;
    }

    dir = opendir (name);
    if (dir == NULL) {
	mnode_free (parent, FALSE);
	return errno;
    }

    while ((dp = readdir (dir)) != NULL) {

	if (strcmp (dp->d_name, ".") == 0
	    || strcmp (dp->d_name, "..") == 0)
	    continue;

	snprintf (p, sizeof(p), "%s/%s", name, dp->d_name);

	ret = stat (p, &sb);
	if (ret < 0) {
	    mnode_free (parent, FALSE);
	    return errno;
	}
	
	if (S_ISDIR(sb.st_mode)) {
	    struct mnode *child_m;

	    add_file (vol, p, parent, &fid, &sb, TYPE_DIR,
		      dp->d_name);
	    ret = mnode_find (&fid, &child_m);
	    assert (ret == 0);

	    ret = add_tree (vol, child_m, p);
	    assert (ret == 0);
	} else if (S_ISREG(sb.st_mode)) {
	    ret = add_file (vol, p, parent, &fid, &sb, TYPE_FILE, 
			    dp->d_name);
	    assert (ret == 0);
	} else if (S_ISLNK(sb.st_mode)) {
	    ret = add_file (vol, p, parent, &fid, &sb, TYPE_LINK, 
			    dp->d_name);
	    assert (ret == 0);
	} else {
	    printf ("ignoring %s\n", p);
	}
    }
    closedir (dir);
    mnode_free (parent, FALSE);
    return 0;
}

int
vld_fvol_create_volume_ondisk (struct dp_part *dp, int32_t volid,
			       const char *path)
{
    volume_handle *vol;
    AFSFid fid;
    struct mnode *m;
    int ret;
    
    ret = vld_create_volume (dp, volid, "foo", VLD_FVOL, ROVOL, 0);
    if (ret)
	return ret;

    ret = vld_open_volume_by_num (dp, volid, &vol);
    if (ret)
	return ret;

    ret = vld_db_uptodate (vol);
    assert (ret == 0);

    fid.Volume = volid;
    fid.Vnode = fid.Unique = 1;
    
    ret = mnode_find (&fid, &m);
    assert (ret == 0);
    ret = add_tree (vol, m, path);
    assert (ret == 0);
    
    vld_free (vol);

    return ret;
}
