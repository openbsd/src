/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

/* $Id: xfs_message.h,v 1.5 2002/06/07 04:10:32 hin Exp $ */

#ifndef _xmsg_h
#define _xmsg_h

/* bump this for any incompatible changes */

#define XFS_VERSION 17

#if defined(WIN32)
#ifdef i386
#ifndef __CYGWIN__
typedef int int32_t;
typedef unsigned int u_int32_t;
typedef short int16_t;
typedef unsigned char u_char;
#endif
#else
#error not a i386
#endif
#elif !defined(__LINUX__) && !defined(HAVE_GLIBC)
#include <sys/types.h>
#if !defined(__OpenBSD__) && defined(_KERNEL)
#include <atypes.h>
#endif
#include <sys/param.h>
#else
#include <linux/types.h>
#include <linux/param.h>
#endif

#include <xfs/xfs_attr.h>

/* Temporary hack? */
#define MAX_XMSG_SIZE (1024*64)

typedef u_int32_t xfs_pag_t;

/*
 * The xfs_cred, if pag == 0, use uid 
 */
typedef struct xfs_cred {
    u_int32_t uid;
    xfs_pag_t pag;
} xfs_cred;

typedef u_int32_t xfs_locktype_t;
typedef u_int32_t xfs_lockid_t;


#define MAXHANDLE (4*4)
#define MAXRIGHTS 8

#define XFS_ANONYMOUSID 32766

typedef struct xfs_handle {
    u_int32_t a, b, c, d;
} xfs_handle;

#define xfs_handle_eq(p, q) \
((p)->a == (q)->a && (p)->b == (q)->b && (p)->c == (q)->c && (p)->d == (q)->d)

/*
 * This should be the maximum size of any `file handle'
 */

#define CACHEHANDLESIZE 80

typedef struct xfs_cache_handle {
    u_char data[CACHEHANDLESIZE];
} xfs_cache_handle;

/*
 * Tokens that apply to nodes, open modes and attributes. Shared
 * reading might be used for exec and exclusive write for remove.
 */
#define XFS_OPEN_MASK	0x000f
#define XFS_OPEN_NR	0x0001	       /* Normal reading, data might change */
#define XFS_OPEN_SR	0x0002	       /* Shared reading, data won't change */
#define XFS_OPEN_NW	0x0004	       /* Normal writing, multiple writers */
#define XFS_OPEN_EW	0x0008	       /* Exclusive writing (open really) */

#define XFS_ATTR_MASK	0x0030
#define XFS_ATTR_R	0x0010	       /* Attributes valid */
#define XFS_ATTR_W	0x0020	       /* Attributes valid and modifiable */

/*
 * Tokens that apply to node data.
 */
#define XFS_DATA_MASK	0x00c0
#define XFS_DATA_R	0x0040	       /* Data valid */
#define XFS_DATA_W	0x0080	       /* Data valid and modifiable */
#define XFS_LOCK_MASK	0x0300
#define XFS_LOCK_R	0x0100	       /* Data Shared locks */
#define XFS_LOCK_W	0x0200	       /* Data Exclusive locks */

#define XFS_ATTR_VALID		XFS_ATTR_R
#define XFS_DATA_VALID		XFS_DATA_W

/* xfs_node.flags */
#define XFS_DATA_DIRTY	0x0001
#define XFS_ATTR_DIRTY	0x0002
#define XFS_AFSDIR	0x0004
#define XFS_STALE	0x0008
#define XFS_XDELETED	0x0010

/* Are necessary tokens available? */
#define XFS_TOKEN_GOT(xn, tok)		((xn)->tokens & (tok))
#define XFS_TOKEN_SET(xn, tok, mask)	((xn)->tokens |= ((tok) & (mask)))
#define XFS_TOKEN_CLEAR(xn, tok, mask)	((xn)->tokens &= ~((tok) & (mask)))

/* definitions for the rights fields */
#define XFS_RIGHT_R	0x01		/* may read? */
#define XFS_RIGHT_W	0x02		/* may write? */
#define XFS_RIGHT_X	0x04		/* may execute? */

/* Max name length passed in xfs messages */

#define XFS_MAX_NAME 256
#define XFS_MAX_SYMLINK_CONTENT 2048

struct xfs_msg_node {
    xfs_handle handle;
    u_int32_t tokens;
    u_int32_t pad1;
    struct xfs_attr attr;
    xfs_pag_t id[MAXRIGHTS];
    u_char rights[MAXRIGHTS];
    u_char anonrights;
    u_int16_t pad2;
    u_int32_t pad3;
};

/*
 * Messages passed through the  xfs_dev.
 */
struct xfs_message_header {
  u_int32_t size;
  u_int32_t opcode;
  u_int32_t sequence_num;		/* Private */
  u_int32_t pad1;
};

/*
 * Used by putdata flag
 */
enum { XFS_READ     = 0x01,
       XFS_WRITE    = 0x02,
       XFS_NONBLOCK = 0x04,
       XFS_APPEND   = 0x08,
       XFS_FSYNC    = 0x10};

/*
 * Flags for inactivenode
 */
enum { XFS_NOREFS = 1, XFS_DELETE = 2 };

/*
 * Flags for installdata
 */

enum { XFS_ID_INVALID_DNLC = 0x01, XFS_ID_AFSDIR = 0x02,
       XFS_ID_HANDLE_VALID = 0x04 };

/*
 * Defined message types and their opcodes.
 */
#define XFS_MSG_VERSION		0
#define XFS_MSG_WAKEUP		1

#define XFS_MSG_GETROOT		2
#define XFS_MSG_INSTALLROOT	3

#define XFS_MSG_GETNODE		4
#define XFS_MSG_INSTALLNODE	5

#define XFS_MSG_GETATTR		6
#define XFS_MSG_INSTALLATTR	7

#define XFS_MSG_GETDATA		8
#define XFS_MSG_INSTALLDATA	9

#define XFS_MSG_INACTIVENODE	10
#define XFS_MSG_INVALIDNODE	11
		/* XXX Must handle dropped/revoked tokens better */

#define XFS_MSG_OPEN		12

#define XFS_MSG_PUTDATA		13
#define XFS_MSG_PUTATTR		14

/* Directory manipulating messages. */
#define XFS_MSG_CREATE		15
#define XFS_MSG_MKDIR		16
#define XFS_MSG_LINK		17
#define XFS_MSG_SYMLINK		18

#define XFS_MSG_REMOVE		19
#define XFS_MSG_RMDIR		20

#define XFS_MSG_RENAME		21

#define XFS_MSG_PIOCTL		22
#define XFS_MSG_WAKEUP_DATA	23

#define XFS_MSG_UPDATEFID	24

#define XFS_MSG_ADVLOCK		25

#define XFS_MSG_GC_NODES	26

#define XFS_MSG_COUNT		27

/* XFS_MESSAGE_VERSION */
struct xfs_message_version {
  struct xfs_message_header header;
  u_int32_t ret;
};

/* XFS_MESSAGE_WAKEUP */
struct xfs_message_wakeup {
  struct xfs_message_header header;
  u_int32_t sleepers_sequence_num;	/* Where to send wakeup */
  u_int32_t error;			/* Return value */
};

/* XFS_MESSAGE_GETROOT */
struct xfs_message_getroot {
  struct xfs_message_header header;
  struct xfs_cred cred;
};

/* XFS_MESSAGE_INSTALLROOT */
struct xfs_message_installroot {
  struct xfs_message_header header;
  struct xfs_msg_node node;
};

/* XFS_MESSAGE_GETNODE */
struct xfs_message_getnode {
  struct xfs_message_header header;
  struct xfs_cred cred;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
};

/* XFS_MESSAGE_INSTALLNODE */
struct xfs_message_installnode {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  struct xfs_msg_node node;
};

/* XFS_MESSAGE_GETATTR */
struct xfs_message_getattr {
  struct xfs_message_header header;
  struct xfs_cred cred;
  xfs_handle handle;
};

/* XFS_MESSAGE_INSTALLATTR */
struct xfs_message_installattr {
  struct xfs_message_header header;
  struct xfs_msg_node node;
};

/* XFS_MESSAGE_GETDATA */
struct xfs_message_getdata {
  struct xfs_message_header header;
  struct xfs_cred cred;
  xfs_handle handle;
  u_int32_t tokens;
  u_int32_t pad1;
};

/* XFS_MESSAGE_INSTALLDATA */
struct xfs_message_installdata {
  struct xfs_message_header header;
  struct xfs_msg_node node;
  char cache_name[XFS_MAX_NAME];
  struct xfs_cache_handle cache_handle;
  u_int32_t flag;
  u_int32_t pad1;
};

/* XFS_MSG_INACTIVENODE */
struct xfs_message_inactivenode {
  struct xfs_message_header header;
  xfs_handle handle;
  u_int32_t flag;
  u_int32_t pad1;
};

/* XFS_MSG_INVALIDNODE */
struct xfs_message_invalidnode {
  struct xfs_message_header header;
  xfs_handle handle;
};

/* XFS_MSG_OPEN */
struct xfs_message_open {
  struct xfs_message_header header;
  struct xfs_cred cred;
  xfs_handle handle;
  u_int32_t tokens;
  u_int32_t pad1;
};

/* XFS_MSG_PUTDATA */
struct xfs_message_putdata {
  struct xfs_message_header header;
  xfs_handle handle;
  struct xfs_attr attr;		/* XXX ??? */
  struct xfs_cred cred;
  u_int32_t flag;
  u_int32_t pad1;
};

/* XFS_MSG_PUTATTR */
struct xfs_message_putattr {
  struct xfs_message_header header;
  xfs_handle handle;
  struct xfs_attr attr;
  struct xfs_cred cred;
};

/* XFS_MSG_CREATE */
struct xfs_message_create {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  struct xfs_attr attr;
  u_int32_t mode;
  u_int32_t pad1;
  struct xfs_cred cred;
};

/* XFS_MSG_MKDIR */
struct xfs_message_mkdir {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  struct xfs_attr attr;
  struct xfs_cred cred;
};

/* XFS_MSG_LINK */
struct xfs_message_link {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  xfs_handle from_handle;
  struct xfs_cred cred;
};

/* XFS_MSG_SYMLINK */
struct xfs_message_symlink {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  char contents[XFS_MAX_SYMLINK_CONTENT];
  struct xfs_attr attr;
  struct xfs_cred cred;
};

/* XFS_MSG_REMOVE */
struct xfs_message_remove {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  struct xfs_cred cred;
};

/* XFS_MSG_RMDIR */
struct xfs_message_rmdir {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[XFS_MAX_NAME];
  struct xfs_cred cred;
};

/* XFS_MSG_RENAME */
struct xfs_message_rename {
  struct xfs_message_header header;
  xfs_handle old_parent_handle;
  char old_name[XFS_MAX_NAME];
  xfs_handle new_parent_handle;
  char new_name[XFS_MAX_NAME];
  struct xfs_cred cred;
};

/* XFS_MSG_PIOCTL */
struct xfs_message_pioctl {
  struct xfs_message_header header;
  u_int32_t opcode ;
  u_int32_t pad1;
  xfs_cred cred;
  u_int32_t insize;
  u_int32_t outsize;
  char msg[2048] ;    /* XXX */
  xfs_handle handle;
};


/* XFS_MESSAGE_WAKEUP_DATA */
struct xfs_message_wakeup_data {
  struct xfs_message_header header;
  u_int32_t sleepers_sequence_num;	/* Where to send wakeup */
  u_int32_t error;			/* Return value */
  u_int32_t len;
  u_int32_t pad1;
  char msg[2048] ;    /* XXX */
};

/* XFS_MESSAGE_UPDATEFID */
struct xfs_message_updatefid {
  struct xfs_message_header header;
  xfs_handle old_handle;
  xfs_handle new_handle;
};

/* XFS_MESSAGE_ADVLOCK */
struct xfs_message_advlock {
  struct xfs_message_header header;
  xfs_handle handle;
  struct xfs_cred cred;
  xfs_locktype_t locktype;
#define XFS_WR_LOCK 1 /* Write lock */
#define XFS_RD_LOCK 2 /* Read lock */
#define XFS_UN_LOCK 3 /* Unlock */
#define XFS_BR_LOCK 4 /* Break lock (inform that we don't want the lock) */
  xfs_lockid_t lockid;
};

/* XFS_MESSAGE_GC_NODES */
struct xfs_message_gc_nodes {
  struct xfs_message_header header;
#define XFS_GC_NODES_MAX_HANDLE 50
  u_int32_t len;
  u_int32_t pad1;
  xfs_handle handle[XFS_GC_NODES_MAX_HANDLE];
};
#endif /* _xmsg_h */
