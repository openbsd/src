/*	$OpenBSD: xfs_message.h,v 1.2 1998/08/30 17:35:43 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

/* $Id: xfs_message.h,v 1.2 1998/08/30 17:35:43 art Exp $ */

#ifndef _xmsg_h
#define _xmsg_h

#if !defined(__LINUX__) && !defined(HAVE_GLIBC)
#include <sys/types.h>
#include <sys/param.h>
#else
#include <linux/types.h>
#include <linux/param.h>
#endif

#include <sys/xfs_attr.h>

/* Temporary hack? */
#define MAX_XMSG_SIZE (1024*64)

typedef u_int32_t pag_t;

/*
 * The xfs_cred, if pag == 0, use uid 
 */
struct xfs_cred {
    __kernel_uid_t uid;
    pag_t pag;
};
typedef struct xfs_cred xfs_cred;

#define MAXHANDLE (4*4)
#define MAXRIGHTS 8

#define XFS_ANONYMOUSID 32766

typedef struct xfs_handle {
    u_int a, b, c, d;
} xfs_handle;

#define xfs_handle_eq(p, q) \
((p)->a == (q)->a && (p)->b == (q)->b && (p)->c == (q)->c && (p)->d == (q)->d)

#define CACHEHANDLESIZE 4

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
#define XFS_LOCK_R	0x0100	       /* Data Shared locks? */
#define XFS_LOCK_W	0x0200	       /* Data Exclusive locks? */

#define XFS_ATTR_VALID		XFS_ATTR_R
#define XFS_DATA_VALID		XFS_DATA_W

/* xfs_node.flags */
#define XFS_DATA_DIRTY	0x0001
#define XFS_ATTR_DIRTY	0x0002

/* Are necessary tokens available? */
#define XFS_TOKEN_GOT(xn, tok)		((xn)->tokens & (tok))
#define XFS_TOKEN_SET(xn, tok, mask)	((xn)->tokens |= ((tok) & (mask)))
#define XFS_TOKEN_CLEAR(xn, tok, mask)	((xn)->tokens &= ~((tok) & (mask)))

/* definitions for the rights fields */
#define XFS_RIGHT_R	0x01		/* may read? */
#define XFS_RIGHT_W	0x02		/* may write? */
#define XFS_RIGHT_X	0x04		/* may execute? */

struct xfs_msg_node {
    xfs_handle handle;
    u_int tokens;
    struct xfs_attr attr;
    pag_t id[MAXRIGHTS];
    u_char rights[MAXRIGHTS];
    u_char anonrights;
};

/*
 * Messages passed through the  xfs_dev.
 */
struct xfs_message_header {
  u_int size;
  u_int opcode;
  u_int sequence_num;		/* Private */
};

/*
 * Used by putdata flag
 */

enum { XFS_READ = 1, XFS_WRITE = 2, XFS_NONBLOCK = 4, XFS_APPEND = 8};

/*
 * Flags for inactivenode
 */

enum { XFS_NOREFS = 1, XFS_DELETE = 2 };

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

#define XFS_MSG_COUNT		24

/* XFS_MESSAGE_WAKEUP */
struct xfs_message_wakeup {
  struct xfs_message_header header;
  int sleepers_sequence_num;	/* Where to send wakeup */
  int error;			/* Return value */
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
  char name[256];		/* XXX */
};

/* XFS_MESSAGE_INSTALLNODE */
struct xfs_message_installnode {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[256];		/* XXX */
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
  u_int tokens;
};

/* XFS_MESSAGE_INSTALLDATA */
struct xfs_message_installdata {
  struct xfs_message_header header;
  struct xfs_msg_node node;
  struct xfs_cache_handle cache_handle;
};

/* XFS_MSG_INACTIVENODE */
struct xfs_message_inactivenode {
  struct xfs_message_header header;
  xfs_handle handle;
  u_int flag;
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
  u_int tokens;
};

/* XFS_MSG_PUTDATA */
struct xfs_message_putdata {
  struct xfs_message_header header;
  xfs_handle handle;
  struct xfs_attr attr;		/* XXX ??? */
  struct xfs_cred cred;
  u_int flag;
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
  char name[256];		/* XXX */
  struct xfs_attr attr;
#if 0 /* XXX ??? */
  enum vcexcl exclusive;
#endif
  int mode;
  struct xfs_cred cred;
};

/* XFS_MSG_MKDIR */
struct xfs_message_mkdir {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[256];		/* XXX */
  struct xfs_attr attr;
  struct xfs_cred cred;
};

/* XFS_MSG_LINK */
struct xfs_message_link {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[256];		/* XXX */
  xfs_handle from_handle;
  struct xfs_cred cred;
};

/* XFS_MSG_SYMLINK */
struct xfs_message_symlink {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[256];		/* XXX */
  char contents[2048];		/* XXX */
  struct xfs_attr attr;
  struct xfs_cred cred;
};

/* XFS_MSG_REMOVE */
struct xfs_message_remove {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[256];		/* XXX */
  struct xfs_cred cred;
};

/* XFS_MSG_RMDIR */
struct xfs_message_rmdir {
  struct xfs_message_header header;
  xfs_handle parent_handle;
  char name[256];		/* XXX */
  struct xfs_cred cred;
};

/* XFS_MSG_RENAME */
struct xfs_message_rename {
  struct xfs_message_header header;
  xfs_handle old_parent_handle;
  char old_name[256];		/* XXX */
  xfs_handle new_parent_handle;
  char new_name[256];		/* XXX */
  struct xfs_cred cred;
};

/* XFS_MSG_PIOCTL */
struct xfs_message_pioctl {
  struct xfs_message_header header;
  int opcode ;
  xfs_cred cred;         /* XXX we should also use PAG */
  int insize;
  int outsize;
  char msg[2048] ;    /* XXX */
  xfs_handle handle;
};


/* XFS_MESSAGE_WAKEUP_DATA */
struct xfs_message_wakeup_data {
  struct xfs_message_header header;
  int sleepers_sequence_num;	/* Where to send wakeup */
  int error;			/* Return value */
  int len;
  char msg[2048] ;    /* XXX */
};

#endif /* _xmsg_h */
