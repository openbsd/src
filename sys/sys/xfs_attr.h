/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

/* $Id: xfs_attr.h,v 1.1 1998/08/30 16:49:58 art Exp $ */

#ifndef _XFS_ATTR_H
#define _XFS_ATTR_H

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

enum xfs_file_type { XFS_FILE_NON, XFS_FILE_REG, XFS_FILE_DIR,
		     XFS_FILE_BLK, XFS_FILE_CHR, XFS_FILE_LNK,
		     XFS_FILE_SOCK, XFS_FILE_FIFO, XFS_FILE_BAD };

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

/*
 * Under glibc and Linux, foo_t in the kernel is not the same type as
 * foo_t in user-level.  Therefore we need these defines.
 */

#if !defined(HAVE_LINUX_TYPES_H) && !defined(__KERNEL__)
typedef mode_t __kernel_mode_t;
typedef nlink_t __kernel_nlink_t;
typedef off_t __kernel_off_t;
typedef uid_t __kernel_uid_t;
typedef gid_t __kernel_gid_t;
#endif

struct xfs_attr {
    u_int32_t		valid;
    __kernel_mode_t	xa_mode;
    __kernel_nlink_t	xa_nlink;
    __kernel_off_t	xa_size;
    __kernel_uid_t	xa_uid;
    __kernel_gid_t	xa_gid;
    time_t		xa_atime;
    time_t		xa_mtime;
    time_t		xa_ctime;
    u_int32_t		xa_fileid;
    enum xfs_file_type  xa_type;
};

#endif /* _XFS_ATTR_H */
