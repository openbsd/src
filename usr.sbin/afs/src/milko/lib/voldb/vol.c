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

/*
 * the volume data file
 */

#include "voldb_locl.h"

RCSID("$arla: vol.c,v 1.7 2002/02/07 17:59:55 lha Exp $");

/*
 * get the partial name of the volume
 * `num' points out the volume and the name is passed back in the
 * string `str' that has size `sz'.
 */

int
vol_getname (uint32_t num, char *str, size_t sz)
{
    int i;
    i = snprintf (str, sz, "vol%08u", num);
    assert (i > 0);
    return 0;
}

/*
 * get the full name of the volume.
 * `part' and `num' points out the volume, the named of passed back in
 * `str' that has the size of `sz'.
 */

int
vol_getfullname (uint32_t part, uint32_t num, char *str, size_t sz)
{
    int ret;
    char volname[VOLSER_MAXVOLNAME];

    ret = vol_getname (num, volname, sizeof (volname));
    if (ret)
	return ret;

    if (part <= 'z' - 'a')
	ret = snprintf (str, sz, "%s/vicep%c/%s",
			dpart_root, 'a' + part, volname);
#if 0
    else if (part <= ('z' - 'a') * ('z' - 'a'))
	ret = snprintf (str, sz, "%s/vicep%c%c/%s", dpart_root,
		      'a' + part / ('z' - 'a'),
		      'a' + part % ('z' - 'a'),
		      volname);
#endif
    else
	return EINVAL; /* XXX */
	
    assert (ret > 0);
    return 0;
}

/*
 * read header from stable storage
 */

int
vol_read_header (int fd, volintInfo *info)
{
    struct stat sb;
    char *buf, *ptr;
    size_t size;
    int ret;

    assert (info);

    ret = fstat (fd, &sb);
    if (ret)
	return errno; /* XXX */

    if (sb.st_size != VOLINTINFO_SIZE)
	return EINVAL;

    ret = lseek (fd, 0, SEEK_SET);
    if (ret)
	return errno; /* XXX */

    buf = malloc (VOLINTINFO_SIZE);
    if (buf == NULL)
	return ENOMEM; /* XXX */

    ret = read (fd, buf, VOLINTINFO_SIZE);
    if (ret <= 0) {
	free (buf);
	return ret;
    }
    
    size = VOLINTINFO_SIZE;
    ptr = ydr_decode_volintInfo(info, buf, &size);
    if (ptr == NULL) {
	free (buf);
	return errno; /* XXX */ 
    }

    free (buf);

    return 0;
}

/*
 * write header to stable storage
 */

int
vol_write_header (int fd, volintInfo *info)
{
    char *buf, *ptr;
    size_t size;
    int ret;

    assert (info);

    ret = lseek (fd, 0, SEEK_SET);
    if (ret)
	return errno; /* XXX */

    size = VOLINTINFO_SIZE;
    buf = malloc (size);
    if (buf == NULL)
	return ENOMEM; /* XXX */

    ptr = ydr_encode_volintInfo(info, buf, &size);
    if (ptr == NULL) {
	free (buf);
	return errno; /* XXX */ 
    }
    
    size = ptr - buf;
    ret = write (fd, buf, size);
    if (ret != size)
	ret = errno;
    else
	ret = 0;

    free (buf);

    return ret;

}

/*
 * create a partition `part' with number `num', name `name'
 * of type `type' that has `parent' as parent.
 */

int
vol_create (int fd, uint32_t num, const char *name,
	    uint32_t type, uint32_t parent)
{
    int ret;
    struct timeval tv;
    volintInfo info;

    gettimeofday (&tv, NULL);

    strlcpy (info.name, name, sizeof (info.name));
    info.name[sizeof (info.name)-1] = '\0';
    info.type = type;
    info.volid = num;
    info.backupID = 0;
    info.parentID = parent;
    info.cloneID = 0;
    info.status = 0;
    info.copyDate = 0;
    info.inUse = 1;
    info.needsSalvaged = 0;
    info.destroyMe = 'c';
    info.creationDate = tv.tv_sec;
    info.accessDate = tv.tv_sec;
    info.updateDate = tv.tv_sec;
    info.backupDate = tv.tv_sec;
    info.dayUse = 0;
    info.filecount = 1;
    info.maxquota = 5000; /* XXX */
    info.size = 0;
    info.flags = 0;
    info.spare0 = 0;
    info.spare1 = 0;
    info.spare2 = 0;
    info.spare3 = 0;

    ret = vol_write_header (fd, &info);
    close (fd);
    return ret;
}


/*
 * Pretty print the volintInfo structure
 */

void
vol_pretty_print_info (FILE *out, volintInfo *info)
{
    assert (info);

    fprintf (out, "name:\t\t%-32s\n", info->name);
    fprintf (out, "type:\t\t%d\n", info->type);
    fprintf (out, "volid: %d backupID: %d parentID: %d cloneID: %d\n", 
	     info->volid, info->backupID, info->parentID, info->cloneID);
    fprintf (out, "status:\t\t0x%x\n", info->status);
    fprintf (out, "copyDate:\t%d\n", info->copyDate);
    fprintf (out, "inUse:\t\t%d\n", info->inUse);
    fprintf (out, "needsSalvaged:\t%d\n", info->needsSalvaged);
    fprintf (out, "destroyMe:\t%d\n", info->destroyMe);
    
    fprintf (out, " XXX \n");
}

/*
 *
 */

static char *type_array[] = {
    "RW",
    "RO",
    "BACK",
};

const char *
vol_voltype2name (int32_t type)
{
    if (type < 0 || type > 2)
	return "UNKN";
    return type_array[type];
}

