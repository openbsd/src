/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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

#include "fsrv_locl.h"

RCSID("$Id");

static int
readbyte(struct rx_call *call, unsigned char *b)
{
    int ret;

    ret = rx_Read (call, b, 1);
    if (ret != 1) {
	mlog_log(MDEBVOLDB, "readbyte: read %d wanted %d", ret, 1);
	return VOLSERFAILEDOP;
    }
    return 0;
}

static int
readint32(struct rx_call *call, uint32_t *d)
{
    int ret;

    ret = rx_Read (call, d, 4);
    if (ret != 4) {
	mlog_log(MDEBVOLDB, "readint32: read %d wanted %d", ret, 4);
	return VOLSERFAILEDOP;
    }
    *d = ntohl(*d);
    return 0;
}

static int
readint16(struct rx_call *call, uint16_t *d)
{
    int ret;

    ret = rx_Read (call, d, 2);
    if (ret != 2) {
	mlog_log(MDEBVOLDB, "readint16: read %d wanted %d", ret, 2);
	return VOLSERFAILEDOP;
    }
    *d = ntohs(*d);
    return 0;
}

static int
readstring(struct rx_call *call, char **ret_string)
{
    int malloc_size = 10;
    int i = 0;
    char *s;
    char *new_s;
    int ret;

    s = malloc(malloc_size);
    if (s == NULL)
	return ENOMEM;
    while (1) {
	if (i >= malloc_size) {
	    malloc_size *= 2;
	    new_s = realloc(s, malloc_size);
	    if (new_s == NULL) {
		free(s);
		return ENOMEM;
	    }
	    s = new_s;
	}
	ret = rx_Read(call, &s[i], sizeof(char));
	if (ret != 1)
	    return VOLSERFAILEDOP;
	if (s[i] == 0) {
	    *ret_string = s;
	    return 0;
	}
	i++;
    }
}

static int
writebyte(struct rx_call *call, unsigned char b)
{
    int ret;

    ret = rx_Write (call, &b, 1);
    if (ret != 1) {
	mlog_log(MDEBVOLDB, "writebyte: write %d wanted %d", ret, 1);
	return VOLSERFAILEDOP;
    }
    return 0;
}

static int
writeint32(struct rx_call *call, uint32_t d)
{
    int ret;

    d = htonl(d);
    ret = rx_Write (call, &d, 4);
    if (ret != 4) {
	mlog_log(MDEBVOLDB, "writeint32: write %d wanted %d", ret, 4);
	return VOLSERFAILEDOP;
    }
    return 0;
}

static int
writeint16(struct rx_call *call, uint16_t d)
{
    int ret;

    d = htons(d);
    ret = rx_Write (call, &d, 2);
    if (ret != 2) {
	mlog_log(MDEBVOLDB, "writeint16: write %d wanted %d", ret, 2);
	return VOLSERFAILEDOP;
    }
    return 0;
}

static int
writestring(struct rx_call *call, char *s)
{
    int len;
    int ret;

    len = strlen(s) + 1;

    ret = rx_Write(call, s, len);
    if (ret != len) {
	mlog_log(MDEBVOLDB, "writestring: write %d wanted %d", ret, len);
	return VOLSERFAILEDOP;
    }
    return 0;
}

static int
writebyte_tag(struct rx_call *call, unsigned char tag, unsigned char b)
{
    int ret;

    ret = writebyte(call, tag);
    if (ret)
	return ret;

    ret = writebyte(call, b);
    if (ret)
	return ret;
}

static int
writeint16_tag(struct rx_call *call, unsigned char tag, uint16_t b)
{
    int ret;

    ret = writebyte(call, tag);
    if (ret)
	return ret;

    ret = writeint16(call, b);
    if (ret)
	return ret;
}

static int
writeint32_tag(struct rx_call *call, unsigned char tag, uint32_t b)
{
    int ret;

    ret = writebyte(call, tag);
    if (ret)
	return ret;

    ret = writeint32(call, b);
    if (ret)
	return ret;
}

static int
writestring_tag(struct rx_call *call, unsigned char tag, char *s)
{
    int ret;

    ret = writebyte(call, tag);
    if (ret)
	return ret;

    ret = writestring(call, s);
    if (ret)
	return ret;
}

static int
read_dumpheader(struct rx_call *call, unsigned char *tag, volume_handle *vh)
{
    uint32_t magic;
    uint32_t version;
    char *s;
    uint32_t volid;
    uint16_t count;
    uint32_t fromdate;
    uint32_t todate;
    int ret;

    ret = readint32(call, &magic);
    if (ret)
	return ret;
    
    ret = readint32(call, &version);
    if (ret)
	return ret;

    if (magic != DUMPBEGINMAGIC)
	return VOLSERFAILEDOP;

    if (version != DUMPVERSION)
	return VOLSERFAILEDOP;

    while (1) {
	ret = readbyte(call, tag);
	if (ret)
	    return ret;
	switch (*tag) {
	case DHTAG_VOLNAME:
	    ret = readstring(call, &s);
	    if (ret)
		return ret;
	    printf("volume: \"%s\"\n", s);
	    free(s);
	    break;
	case DHTAG_VOLID:
	    ret = readint32(call, &volid);
	    if (ret)
		return ret;
	    printf("volid: %u\n", volid);
	    break;
	case DHTAG_DUMPTIMES:
	    ret = readint16(call, &count);
	    if (ret)
		return ret;
	    if (count != 2)
		return VOLSERFAILEDOP;
	    ret = readint32(call, &fromdate);
	    if (ret)
		return ret;
	    ret = readint32(call, &todate);
	    if (ret)
		return ret;
	    printf("dumptimes: from %d to %d\n", fromdate, todate);
	    break;
	default:
	    return 0;
	}
    }
}

static int
read_volumeheader(struct rx_call *call, unsigned char *tag, volume_handle *vh)
{
    int ret;
    char *s;
    uint32_t i32;
    unsigned char byte;
    uint16_t count;
    uint32_t *i32_array;
    int i;

    while (1) {
	switch (*tag) {
	case VHTAG_VOLID:
	case VHTAG_VERS:
	case VHTAG_VUNIQ:
	case VHTAG_PARENT:
	case VHTAG_CLONE:
	case VHTAG_MAXQUOTA:
	case VHTAG_MINQUOTA:
	case VHTAG_DISKUSED:
	case VHTAG_FILECNT:
	case VHTAG_ACCOUNT:
	case VHTAG_OWNER:
	case VHTAG_DAYUSE:
	case VHTAG_CREAT:
	case VHTAG_ACCESS:
	case VHTAG_UPDATE:
	case VHTAG_EXPIRE:
	case VHTAG_BACKUP:
	case VHTAG_DUDATE:
	    ret = readint32(call, &i32);
	    break;
	case VHTAG_VOLNAME:
	case VHTAG_OFFLINE:
	case VHTAG_MOTD:
	    ret = readstring(call, &s);
	    break;
	case VHTAG_INSERV:
	case VHTAG_BLESSED:
	case VHTAG_TYPE:
	    ret = readbyte(call, &byte);
	    break;
	case VHTAG_WEEKUSE:
	    ret = readint16(call, &count);
	    if (ret)
		break;
	    i32_array = malloc(4 * count);
	    if (i32_array == NULL)
		return ENOMEM;
	    for (i = 0; i < count; i++) {
		ret = readint32(call, &i32_array[i]);
		if (ret) {
		    free(i32_array);
		    break;
		}
	    }
	    break;
	default:
	    return 0;
	}
	if (ret)
	    return ret;
	switch (*tag) {
	case VHTAG_VOLID:
	    mlog_log(MDEBVOLDB, "volid: %d", i32);
	    break;
	case VHTAG_VERS:
	    mlog_log(MDEBVOLDB, "vers: %d", i32);
	    break;
	case VHTAG_VUNIQ:
	    break;
	case VHTAG_PARENT:
	    vh->info.parentID = i32;
	    break;
	case VHTAG_CLONE:
	    vh->info.cloneID = i32;
	    break;
	case VHTAG_MAXQUOTA:
	    vh->info.maxquota = i32;
	    break;
	case VHTAG_MINQUOTA:
	    break;
	case VHTAG_DISKUSED:
	    break;
	case VHTAG_FILECNT:
	    break;
	case VHTAG_ACCOUNT:
	    break;
	case VHTAG_OWNER:
	    break;
	case VHTAG_DAYUSE:
	    vh->info.dayUse = i32;
	    break;
	case VHTAG_CREAT:
	    vh->info.creationDate = i32;
	    break;
	case VHTAG_ACCESS:
	    vh->info.accessDate = i32;
	    break;
	case VHTAG_UPDATE:
	    vh->info.updateDate = i32;
	    break;
	case VHTAG_EXPIRE:
	    mlog_log(MDEBVOLDB, "expire: %d", i32);
	    break;
	case VHTAG_BACKUP:
	    vh->info.backupDate = i32;
	case VHTAG_DUDATE:
	    mlog_log(MDEBVOLDB, "dudate: %d", i32);
	    break;
	case VHTAG_VOLNAME:
	    strlcpy(vh->info.name, s, VNAMESIZE);
	    free(s);
	    break;
	case VHTAG_OFFLINE:
	    free(s);
	    break;
	case VHTAG_MOTD:
	    free(s);
	    break;
	case VHTAG_INSERV:
	    mlog_log(MDEBVOLDB, "inserv: %u", (unsigned int)byte);
	    break;
	case VHTAG_BLESSED:
	    mlog_log(MDEBVOLDB, "blessed: %u", (unsigned int)byte);
	    break;
	case VHTAG_TYPE:
	    vh->info.type = byte;
	    break;
	case VHTAG_WEEKUSE:
	    free(i32_array);
	    break;
	default:
	    return VOLSERFAILEDOP;
	}

	ret = readbyte(call, tag);
	if (ret)
	    return ret;
    }
}

static int
read_vnodeheader(struct rx_call *call, unsigned char *tag, volume_handle *vh)
{
    int ret;
    uint32_t i32;
    uint16_t i16;
    unsigned char byte;
    uint32_t vnode;
    uint32_t uniq;
    char *acl = NULL;
    uint32_t length;

    uint32_t dataversion = 0;
    uint32_t author = 0;
    uint32_t owner = 0;
    uint32_t group = 0;
    uint32_t parent = 0;
    uint32_t client_date = 0;
    uint32_t server_date = 0;
    uint16_t nlinks = 0;
    uint16_t mode = 0;
    uint8_t type = 0;

    int got_tags = 0;

    ret = readint32(call, &vnode);
    if (ret)
	return ret;
    
    ret = readint32(call, &uniq);
    if (ret)
	return ret;

    ret = readbyte(call, tag);
    if (ret)
	return ret;

    while (1) {
	switch (*tag) {
	case VTAG_DVERS:
	case VTAG_AUTHOR:
	case VTAG_OWNER:
	case VTAG_GROUP:
	case VTAG_PARENT:
	case VTAG_CLIENT_DATE:
	case VTAG_SERVER_DATE:
	    ret = readint32(call, &i32);
	    break;
	case VTAG_NLINKS:
	case VTAG_MODE:
	    ret = readint16(call, &i16);
	    break;
	case VTAG_TYPE:
	    ret = readbyte(call, &byte);
	    break;
	case VTAG_ACL:
	    acl = malloc(SIZEOF_ACL);
	    ret = rx_Read(call, acl, SIZEOF_ACL);
	    if (ret != SIZEOF_ACL)
		return VOLSERFAILEDOP;
	    ret = 0;
	    break;
	case VTAG_DATA:
	    ret = readint32(call, &length);
	    break;
	default:
	    return 0;
	}
	if (ret)
	    return ret;
	switch (*tag) {
	case VTAG_DVERS:
	    dataversion = i32;
	    got_tags |= GOT_VTAG_DVERS;
	    break;
	case VTAG_AUTHOR:
	    author = i32;
	    got_tags |= GOT_VTAG_AUTHOR;
	    break;
	case VTAG_OWNER:
	    owner = i32;
	    got_tags |= GOT_VTAG_OWNER;
	    break;
	case VTAG_GROUP:
	    group = i32;
	    got_tags |= GOT_VTAG_GROUP;
	    break;
	case VTAG_PARENT:
	    parent = i32;
	    got_tags |= GOT_VTAG_PARENT;
	    break;
	case VTAG_CLIENT_DATE:
	    client_date = i32;
	    got_tags |= GOT_VTAG_CLIENT_DATE;
	    break;
	case VTAG_SERVER_DATE:
	    server_date = i32;
	    got_tags |= GOT_VTAG_SERVER_DATE;
	    break;
	case VTAG_NLINKS:
	    nlinks = i16;
	    got_tags |= GOT_VTAG_NLINKS;
	    break;
	case VTAG_MODE:
	    mode = i16;
	    got_tags |= GOT_VTAG_MODE;
	    break;
	case VTAG_TYPE:
	    type = byte;
	    got_tags |= GOT_VTAG_TYPE;
	    break;
	case VTAG_ACL:
	    got_tags |= GOT_VTAG_ACL;
	    break;
	case VTAG_DATA:
	    if ((got_tags & GOT_ALL_FILE) == GOT_ALL_FILE) {
		ret = restore_file(call, vnode, uniq, length,
				   dataversion, author,
				   owner, group, parent,
				   client_date, server_date,
				   nlinks, mode, type, vh, (int32_t *)acl);
		free(acl);
		if (ret)
		    return ret;
		ret = readbyte(call, tag);
		if (ret)
		    return ret;
		return ret;
	    } else {
		free(acl);
		return VOLSERFAILEDOP;
	    }
	    printf("data\n");
	    break;
	default:
	    return VOLSERFAILEDOP;
	}

	ret = readbyte(call, tag);
	if (ret)
	    return ret;
    }
}


int
parse_dump(struct rx_call *call, volume_handle *vh)
{
    unsigned char tag;
    int ret;
    uint32_t dumpendmagic;
    char buffer[2048];

    ret = readbyte(call, &tag);
    if (ret)
	return ret;
    
    while (1) {
	switch (tag) {
	case TAG_DUMPHEADER:
	    mlog_log(MDEBVOLDB, "parse_dump: dumpheader");
	    ret = read_dumpheader(call, &tag, vh);
	    if (ret)
		return ret;
	    break;
	case TAG_VOLHEADER:
	    mlog_log(MDEBVOLDB, "parse_dump: volheader");
	    ret = readbyte(call, &tag);
	    if (ret)
		return ret;
	    ret = read_volumeheader(call, &tag, vh);
	    if (ret)
		return ret;
	    break;
	case TAG_VNODE:
	    mlog_log(MDEBVOLDB, "parse_dump: vnode");
	    ret = read_vnodeheader(call, &tag, vh);
	    if (ret)
		return ret;
	    break;
	case TAG_DUMPEND:
	    mlog_log(MDEBVOLDB, "parse_dump: dumpend");
	    ret = readint32(call, &dumpendmagic);
	    if (ret)
		return ret;
	    if (dumpendmagic != DUMPENDMAGIC) {
		mlog_log(MDEBVOLDB, "parse_dump: dumpend: wrong magic: %u",
			 dumpendmagic);
		return VOLSERFAILEDOP;
	    }
	    return 0;
	    break;
	default:
	    mlog_log(MDEBVOLDB, "parse_dump: unknown tag %d", (int) tag);
	    rx_Read(call, buffer, sizeof(buffer));
	    return VOLSERFAILEDOP;
	}
    }
}

static int
write_dumpheader(struct rx_call *call, volume_handle *vh)
{
    int ret;

    ret = writebyte(call, TAG_DUMPHEADER);
    if (ret)
	return ret;
    ret = writeint32(call, DUMPBEGINMAGIC);
    if (ret)
	return ret;
    ret = writeint32(call, DUMPVERSION);
    if (ret)
	return ret;

    ret = writestring_tag(call, DHTAG_VOLNAME, vh->info.name);
    if (ret)
	return ret;

    ret = writeint32_tag(call, DHTAG_VOLID, vh->info.volid);
    if (ret)
	return ret;

    ret = writebyte(call, DHTAG_DUMPTIMES);
    if (ret)
	return ret;
    ret = writeint16(call, 2);
    if (ret)
	return ret;
    ret = writeint32(call, 0); /* fromdate */
    if (ret)
	return ret;
    ret = writeint32(call, 0); /* todate */
    if (ret)
	return ret;

    return 0;
}

static int
write_volheader(struct rx_call *call, volume_handle *vh)
{
    int ret;

    ret = writebyte(call, TAG_VOLHEADER);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_VOLID, vh->info.volid);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_VERS, 1);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_VUNIQ, 0); /* XXX */
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_PARENT, vh->info.parentID);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_CLONE, vh->info.cloneID);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_MAXQUOTA, vh->info.maxquota);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_MINQUOTA, 0); /* XXX */
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_DISKUSED, vh->info.size);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_FILECNT, 0); /* XXX */
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_ACCOUNT, 0);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_OWNER, 0);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_DAYUSE, vh->info.dayUse);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_CREAT, vh->info.creationDate);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_ACCESS, vh->info.accessDate);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_UPDATE, vh->info.updateDate);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_EXPIRE, 0); /* XXX */
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_BACKUP, vh->info.backupDate);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VHTAG_DUDATE, 0); /* XXX */
    if (ret)
	return ret;

    ret = writestring_tag(call, VHTAG_VOLNAME, vh->info.name);
    if (ret)
	return ret;

    ret = writestring_tag(call, VHTAG_OFFLINE, "");
    if (ret)
	return ret;

    ret = writestring_tag(call, VHTAG_MOTD, "");
    if (ret)
	return ret;

    ret = writebyte_tag(call, VHTAG_INSERV, 0); /* XXX */
    if (ret)
	return ret;

    ret = writebyte_tag(call, VHTAG_BLESSED, 0); /* XXX */
    if (ret)
	return ret;

    ret = writebyte_tag(call, VHTAG_TYPE, vh->info.type);
    if (ret)
	return ret;

    ret = writeint16_tag(call, VHTAG_WEEKUSE, 7);
    if (ret)
	return ret;

    {
	int i;

	for (i = 0; i < 7; i++) {
	    ret = writeint32(call, 0);
	}
    }

    return 0;
}

static int
write_vnode(int fd,
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
	    void *arg)
{
    struct rx_call *call = (struct rx_call *) arg;
    int ret;
    struct mnode n;

    ret = writebyte(call, TAG_VNODE);
    if (ret)
	return ret;

    ret = writeint32(call, vnode);
    if (ret)
	return ret;

    ret = writeint32(call, uniq);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_DVERS, dataversion);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_AUTHOR, author);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_OWNER, owner);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_GROUP, group);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_PARENT, parent);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_CLIENT_DATE, client_date);
    if (ret)
	return ret;

    ret = writeint32_tag(call, VTAG_SERVER_DATE, server_date);
    if (ret)
	return ret;

    ret = writeint16_tag(call, VTAG_NLINKS, nlinks);
    if (ret)
	return ret;

    ret = writeint16_tag(call, VTAG_MODE, mode);
    if (ret)
	return ret;

    ret = writebyte_tag(call, VTAG_TYPE, type);
    if (ret)
	return ret;

    if (acl) {
	ret = writebyte(call, VTAG_ACL);
	if (ret)
	    return ret;
	ret = rx_Write(call, acl, SIZEOF_ACL);
	if (ret != SIZEOF_ACL) {
	    mlog_log(MDEBVOLDB, "write_vnode: read %d wanted %d",
		     ret, SIZEOF_ACL);
	    return VOLSERFAILEDOP;
	}
    }

    ret = writeint32_tag(call, VTAG_DATA, length);
    if (ret)
	return ret;

    ret = copyfd2rx(fd, call, 0, length);
    if (ret)
	return ret;

    return 0;
}

int
generate_dump(struct rx_call *call, volume_handle *vh)
{
    int ret;

    ret = write_dumpheader(call, vh);
    if (ret)
	return ret;

    ret = write_volheader(call, vh);
    if (ret)
	return ret;

    ret = vld_foreach_dir(vh, write_vnode, call);
    if (ret)
	return ret;

    ret = vld_foreach_file(vh, write_vnode, call);
    if (ret)
	return ret;

    ret = writebyte(call, TAG_DUMPEND);
    if (ret)
	return ret;
    ret = writeint32(call, DUMPENDMAGIC);
    if (ret)
	return ret;

    return 0;
}


