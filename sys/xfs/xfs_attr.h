/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
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

/* $arla: xfs_attr.h,v 1.12 2002/09/07 10:46:15 lha Exp $ */

#ifndef _NNPFS_ATTR_H
#define _NNPFS_ATTR_H

#define XA_V_NONE       0
#define XA_V_MODE	(1 <<  0)
#define XA_V_NLINK	(1 <<  1)
#define XA_V_SIZE	(1 <<  2)
#define XA_V_UID	(1 <<  3)
#define XA_V_GID	(1 <<  4)
#define XA_V_ATIME	(1 <<  5)
#define XA_V_MTIME	(1 <<  6)
#define XA_V_CTIME	(1 <<  7)
#define XA_V_FILEID	(1 <<  8)
#define XA_V_TYPE       (1 <<  9)

#define NNPFS_FILE_NON 1
#define NNPFS_FILE_REG 2
#define NNPFS_FILE_DIR 3
#define NNPFS_FILE_BLK 4
#define NNPFS_FILE_CHR 5
#define NNPFS_FILE_LNK 6
#define NNPFS_FILE_SOCK 7
#define NNPFS_FILE_FIFO 8
#define NNPFS_FILE_BAD 9

#define XA_CLEAR(xa_p) \
        ((xa_p)->valid = XA_V_NONE)
#define XA_SET_MODE(xa_p, value) \
	(((xa_p)->valid) |= XA_V_MODE, ((xa_p)->xa_mode) = value)
#define XA_SET_NLINK(xa_p, value) \
	(((xa_p)->valid) |= XA_V_NLINK, ((xa_p)->xa_nlink) = value)
#define XA_SET_SIZE(xa_p, value) \
	(((xa_p)->valid) |= XA_V_SIZE, ((xa_p)->xa_size) = value)
#define XA_SET_UID(xa_p, value) \
	(((xa_p)->valid) |= XA_V_UID, ((xa_p)->xa_uid) = value)
#define XA_SET_GID(xa_p, value) \
	(((xa_p)->valid) |= XA_V_GID, ((xa_p)->xa_gid) = value)
#define XA_SET_ATIME(xa_p, value) \
	(((xa_p)->valid) |= XA_V_ATIME, ((xa_p)->xa_atime) = value)
#define XA_SET_MTIME(xa_p, value) \
	(((xa_p)->valid) |= XA_V_MTIME, ((xa_p)->xa_mtime) = value)
#define XA_SET_CTIME(xa_p, value) \
	(((xa_p)->valid) |= XA_V_CTIME, ((xa_p)->xa_ctime) = value)
#define XA_SET_FILEID(xa_p, value) \
	(((xa_p)->valid) |= XA_V_FILEID, ((xa_p)->xa_fileid) = value)
#define XA_SET_TYPE(xa_p, value) \
	(((xa_p)->valid) |= XA_V_TYPE, ((xa_p)->xa_type) = value)


#define XA_VALID_MODE(xa_p) \
	(((xa_p)->valid) & XA_V_MODE)
#define XA_VALID_NLINK(xa_p) \
	(((xa_p)->valid) & XA_V_NLINK)
#define XA_VALID_SIZE(xa_p) \
	(((xa_p)->valid) & XA_V_SIZE)
#define XA_VALID_UID(xa_p) \
	(((xa_p)->valid) & XA_V_UID)
#define XA_VALID_GID(xa_p) \
	(((xa_p)->valid) & XA_V_GID)
#define XA_VALID_ATIME(xa_p) \
	(((xa_p)->valid) & XA_V_ATIME)
#define XA_VALID_MTIME(xa_p) \
	(((xa_p)->valid) & XA_V_MTIME)
#define XA_VALID_CTIME(xa_p) \
	(((xa_p)->valid) & XA_V_CTIME)
#define XA_VALID_FILEID(xa_p) \
	(((xa_p)->valid) & XA_V_FILEID)
#define XA_VALID_TYPE(xa_p) \
	(((xa_p)->valid) & XA_V_TYPE)

struct xfs_attr {
    uint32_t		valid;
    uint32_t		xa_mode;

    uint32_t		xa_nlink;
    uint32_t		xa_size;

    uint32_t		xa_uid;
    uint32_t		xa_gid;

    uint32_t		xa_atime;
    uint32_t		xa_mtime;

    uint32_t		xa_ctime;
    uint32_t		xa_fileid;

    uint32_t           xa_type;
    uint32_t           pad1;
};

#endif /* _NNPFS_ATTR_H */
