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

#include <sfvol_private.h>

RCSID("$arla: fvol.c,v 1.3 2000/10/03 00:18:51 lha Exp $");

/*
 * Description:
 *  fvol is a simple volume to read-only files.
 *  It stores the complete filenames in a flat database.
 */

struct fvol {
    int fd;
    fbuf the_fbuf;
};

static int
getfilename (struct fvol *f, int32_t off, char **name);

static int
opaque2num (onode_opaque *opaque, int32_t *num)
{
    char *c;

    if (opaque->size < sizeof(*num) + 1)
	return EINVAL;

    c = (char *)opaque->data;
    if (*c != 'F')
	return EINVAL;

    c++;
    memcpy (num, c, sizeof (*num));
    *num = ntohl(*num);
    return 0;
}

static int
num2opaque (int32_t num, onode_opaque *opaque)
{
    unsigned char *ptr = opaque->data;
    num = htonl(num);
    
    opaque->size = sizeof (int32_t) + 1;

    *ptr = 'F';
    ptr++;

    memcpy (ptr, &num, sizeof (num));
    return 0;
}

/*
 *
 */


int
fvol_offset2opaque (int32_t offset, onode_opaque *opaque)
{
    return num2opaque (offset, opaque);
}


/*
 *
 */

static int
getfilename (struct fvol *f, int32_t off, char **name)
{
    unsigned char *buf;
    
    assert (f && name);

    buf = (unsigned char *) fbuf_buf (&f->the_fbuf);

    if (off > fbuf_len(&f->the_fbuf) - 1)
	return ENOENT;

    *name = buf + off;
    return 0;
}
/*
 *
 */

int
fvol_addfile (struct fvol *f, const char *name, int32_t *off)
{
    size_t len = strlen(name);
    char *ptr;
    int ret;

    *off = fbuf_len(&f->the_fbuf);
    ptr = ((unsigned char *)fbuf_buf((&f->the_fbuf))) + *off;

    ret = fbuf_truncate (&f->the_fbuf, *off + len + 1);
    assert (ret == 0);

    strcpy (ptr, name);

    return 0;
}

static void
create_fvol_name (struct dp_part *dp, int32_t volid, char *name, size_t len)
{
    snprintf (name, len, "%s/fvol-db-%d",
	      DP_NAME(dp),
	      volid);

}

/*
 * Create a fvol and return it in `vol' that is generic volume.
 * ignore the flags.
 */

static int
fvol_open (struct dp_part *part, int32_t volid, int flags,
	   void **data)
{
    char p[MAXPATHLEN];
    struct fvol *f;
    int ret, openf;
    int32_t magic;
    struct stat sb;
    fbuf_flags fflags;

    *data = NULL;

    create_fvol_name (part, volid, p, sizeof(p));

    f = malloc (sizeof(*f));
    if (f == NULL)
	return EINVAL;

    if ((VOLOP_CREATE & flags) == VOLOP_CREATE) {
	fflags = FBUF_READ|FBUF_WRITE|FBUF_PRIVATE;
	openf = O_RDWR|O_CREAT;
    } else {
	fflags = FBUF_READ|FBUF_WRITE|FBUF_SHARED;
	openf = O_RDWR;
    }

    f->fd = open (p, openf, 0600);
    if (f->fd < 0) {
	ret = errno;
	free (f);
	return ret;
    }

    if ((VOLOP_CREATE & flags) == VOLOP_CREATE) {
	magic = htonl(FVOL_MAGIC1);
	ret = write (f->fd, &magic, sizeof (magic));
	if (ret != sizeof (magic)) {
	    ret = errno;
	    close (f->fd);
	    free (f);
	}
	sb.st_size = sizeof (magic);
    } else {
	ret = read (f->fd, &magic, sizeof (magic));
	if (ret != sizeof (magic)) {
	    ret = errno;
	    close (f->fd);
	    free (f);
	    return ret;
	}
	
	if (htonl(magic) != FVOL_MAGIC1) {
	    close (f->fd);
	    free (f);
	    return EINVAL;
	}

	ret = fstat (f->fd, &sb);
	assert (ret == 0);
    }
    ret = fbuf_create (&f->the_fbuf, f->fd, sb.st_size, fflags);
    assert (ret == 0);

    *data = f;
    return 0;
}

/*
 * free all fvol related to the volume `vol'.
 */

void
fvol_free_i (struct fvol *f)
{
    fbuf_end (&f->the_fbuf);
    close (f->fd);
    free (f);
}

/*
 * create a inode on `vol', return onode_opaque in `o'.
 */

static int
fvol_icreate (volume_handle *vol, onode_opaque *o, node_type type,
	      struct mnode *n)
{
    int ret;

    switch (type) {
    case NODE_VOL:
    case NODE_META:
    case NODE_DIR:
	ret = local_create_file (vol->dp, o, n);
	break;
    case NODE_REG:
	o->size= 0;
	memset (o->data, 0, sizeof (o->data));
	ret = 0;
	break;
    default:
        err(-1, "fvol_icreate: bad type %d\n", type);
    }
    return ret;
}

/*
 * open `o' in `vol' with open(2) `flags', return filedescriptor in `fd'
 */

static int
fvol_iopen (volume_handle *vol, onode_opaque *o, int flags, int *fd)
{
    char *name;
    int32_t num;
    int ret;

    if (local_opaquep (o))
	return local_open_file (vol->dp, o, flags, fd);

    ret = opaque2num (o, &num);
    if (ret) return ret;

    ret = getfilename ((struct fvol *) vol->data, num, &name);
    if (ret) return ret;

    ret = open (name, O_RDONLY, 0600);
    if (ret < 0)
	return errno;

    *fd = ret;
    return 0;
}

static int
fvol_unlink (volume_handle *vol, onode_opaque *o)
{
    return EROFS;
}

/*
 *
 */

static void
fvol_free (volume_handle *vol)
{
    if (vol->data)
	fvol_free_i ((struct fvol *)vol->data);
}

/*
 *
 */

int
fvol_create_volume (struct dp_part *dp, int32_t volid, struct fvol **f)
{
    return fvol_open (dp, volid, VOLOP_CREATE, (void **) f);
}

/*
 *
 */

static int
fvol_remove (volume_handle *vol)
{
    char p[MAXPATHLEN];
    int ret;

    create_fvol_name (vol->dp, vol->vol, p, sizeof(p));
    ret = unlink(p);
    if (ret) return ret;

    if (vol->data)
	fvol_free_i ((struct fvol *)vol->data);
    return 0;
}


/*
 *
 */

vol_op fvol_volume_ops = {
    "fvol",
    fvol_open,
    fvol_free,
    fvol_icreate,
    fvol_iopen,
    fvol_unlink,
    fvol_remove
};    

