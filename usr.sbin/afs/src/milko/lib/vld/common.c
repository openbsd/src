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

#include <sfvol_private.h>

RCSID("$arla: common.c,v 1.3 2000/10/03 00:18:41 lha Exp $");

/*
 * Translate the `opaque' to the `ino'.
 */

static int
local_ino2opaque (ino_t *ino, onode_opaque *opaque)
{
    unsigned char *ptr = opaque->data;

    opaque->size =  sizeof(ino_t) + 1;

    *ptr = 'L';
    ptr++;

    memcpy (ptr, ino, sizeof (*ino));
    return 0;
}

/*
 * The reverse
 */

static int
local_opaque2ino (onode_opaque *opaque, ino_t *ino)
{
    unsigned char *ptr = opaque->data;

    if (*ptr != 'L')
	return EINVAL;

    ptr++;
    
    memcpy (ino, ptr, sizeof (*ino));
    opaque->size = sizeof(ino_t) + 1;
    return 0;
}

/*
 *
 */

Bool
local_opaquep (onode_opaque *opaque)
{
    unsigned char *ptr = opaque->data;

    if (opaque->size < 1) return FALSE;
    if (*ptr == 'L') return TRUE;
    return FALSE;
}

/*
 *
 */

int
local_unlink_file (struct dp_part *dp, onode_opaque *o)
{
    char name[MAXPATHLEN];
    int ret;
    ino_t ino;

    ret = local_opaque2ino (o, &ino);
    if (ret)
	return ret;

    ret = local_create_name (dp, ino, name, sizeof (name));
    if (ret)
	return ret;

    ret = unlink (name);
    if (ret)
	return errno;

    return 0;

}

/*
 *
 */

int
local_open_file (struct dp_part *dp, onode_opaque *o,
		 int flags, int *fd)
{
    char p[MAXPATHLEN];
    ino_t ino;
    int ret;

    ret = local_opaque2ino (o, &ino);
    if (ret) return ret;
	
    ret = local_create_name (dp, ino, p, sizeof(p));
    if (ret) return ret;

    ret = open (p, flags, 0600);
    if (ret < 0) return errno;

    *fd = ret;
    return 0;
}

/*
 *
 */

int
local_create_name (struct dp_part *dp, int32_t num, char *name, size_t sz)
{
    size_t i;
    
    assert (name);

    i = snprintf (name, sz, "%s/%02x/%02x/%02x/%02x",
		  DP_NAME(dp),
		  (unsigned int) (num >> 24) & 0xff,
		  (unsigned int) (num >> 16) & 0xff,
		  (unsigned int) (num >> 8) & 0xff,
		  (unsigned int) num & 0xff);
    
    if (i == -1 || i >= sz)
	return(-1);
    return 0;
}

int
local_create_file (struct dp_part *dp, onode_opaque *o,
		   struct mnode *n)
{
    char name[MAXPATHLEN];
    char nodename[MAXPATHLEN];
    int fd, ret, i;
    struct stat sb;
    
    i = snprintf(nodename, sizeof(nodename), "%s/inodeXXXXXXXXXX",
		 DP_NAME(dp));
    if (i == -1 || i >= sizeof(nodename))
	return(ENOMEM);
    
    fd = mkstemp(nodename);
    if (fd == -1)
	return errno;
    ret = fstat(fd, &sb);
    if (ret == -1) {
	close(fd);
	unlink(nodename);
	return errno;
    }

    if (n) {
	n->fd = fd;
	n->sb = sb;
	n->flags.fdp = TRUE;
	n->flags.sbp = TRUE;
    } else {
	close(fd);
    }
 


    i = snprintf(name, sizeof(name), "%s/%02x", DP_NAME(dp), 
		 (unsigned int) (sb.st_ino >> 24) & 0xff);
    if (i == -1 || i >= sizeof(name)) {
	    ret = ENOMEM
	    goto bad;
    }
    mkdir(name, 0700);
    i = snprintf(name, sizeof(name), "%s/%02x/%02x", DP_NAME(dp), 
	     (unsigned int) (sb.st_ino >> 24) & 0xff, 
	     (unsigned int) (sb.st_ino >> 16) & 0xff);
    if (i == -1 || i >= sizeof(name)) {
	    ret = ENOMEM
	    goto bad;
    }

    mkdir(name, 0700);
    i = snprintf(name, sizeof(name), "%s/%02x/%02x/%02x", DP_NAME(dp), 
	     (unsigned int) (sb.st_ino >> 24) & 0xff, 
	     (unsigned int) (sb.st_ino >> 16) & 0xff,
	     (unsigned int) (sb.st_ino >> 8) & 0xff);
    if (i == -1 || i >= sizeof(name)) {
	    ret = ENOMEM
	    goto bad;
    }
    if (mkdir(name, 0700) == -1) {
	    ret = errno;
	    goto bad;
    }
    if (local_create_name (dp, sb.st_ino, name, sizeof(name)) == -1) {
	    ret = ENOMEM;
	    goto bad;
    }

    ret = rename(nodename, name);
    if (ret < 0) {
	ret = errno;
	goto bad;
    }

    ret = local_ino2opaque (&sb.st_ino, o);
    if (ret) {
	ret = errno;
	unlink (name);
	return ret;
    }
    return 0;
 bad:
    unlink(nodename);
    return(ret);
	    
}

