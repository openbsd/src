/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

/* $arla: nnpfs_message.h,v 1.55 2002/09/27 09:43:21 lha Exp $ */

#ifndef _xmsg_h
#define _xmsg_h

/* bump this for any incompatible changes */

#define NNPFS_VERSION 18

#include <nnpfs/nnpfs_attr.h>

/* Temporary hack? */
#define MAX_XMSG_SIZE (1024*64)

typedef uint32_t nnpfs_pag_t;

/*
 * The nnpfs_cred, if pag == 0, use uid 
 */
typedef struct nnpfs_cred {
    uint32_t uid;
    nnpfs_pag_t pag;
} nnpfs_cred;

typedef uint32_t nnpfs_locktype_t;
typedef uint32_t nnpfs_lockid_t;


#define MAXHANDLE (4*4)
#define MAXRIGHTS 8

#define NNPFS_ANONYMOUSID 32766

typedef struct nnpfs_handle {
    uint32_t a, b, c, d;
} nnpfs_handle;

#define nnpfs_handle_eq(p, q) \
((p)->a == (q)->a && (p)->b == (q)->b && (p)->c == (q)->c && (p)->d == (q)->d)

/*
 * This should be the maximum size of any `file handle'
 */

#define CACHEHANDLESIZE 80

typedef struct nnpfs_cache_handle {
    u_char data[CACHEHANDLESIZE];
} nnpfs_cache_handle;

/*
 * Tokens that apply to nodes, open modes and attributes. Shared
 * reading might be used for exec and exclusive write for remove.
 */
#define NNPFS_OPEN_MASK	0x000f
#define NNPFS_OPEN_NR	0x0001	       /* Normal reading, data might change */
#define NNPFS_OPEN_SR	0x0002	       /* Shared reading, data won't change */
#define NNPFS_OPEN_NW	0x0004	       /* Normal writing, multiple writers */
#define NNPFS_OPEN_EW	0x0008	       /* Exclusive writing (open really) */

#define NNPFS_ATTR_MASK	0x0030
#define NNPFS_ATTR_R	0x0010	       /* Attributes valid */
#define NNPFS_ATTR_W	0x0020	       /* Attributes valid and modifiable */

/*
 * Tokens that apply to node data.
 */
#define NNPFS_DATA_MASK	0x00c0
#define NNPFS_DATA_R	0x0040	       /* Data valid */
#define NNPFS_DATA_W	0x0080	       /* Data valid and modifiable */
#define NNPFS_LOCK_MASK	0x0300
#define NNPFS_LOCK_R	0x0100	       /* Data Shared locks */
#define NNPFS_LOCK_W	0x0200	       /* Data Exclusive locks */

#define NNPFS_ATTR_VALID		NNPFS_ATTR_R
#define NNPFS_DATA_VALID		NNPFS_DATA_W

/* nnpfs_node.flags
 * The lower 16 bit flags are reserved for common nnpfs flags
 * The upper 16 bit flags are reserved for operting system dependant
 * flags.
 */

#define NNPFS_DATA_DIRTY	0x0001
#define NNPFS_ATTR_DIRTY	0x0002
#define NNPFS_AFSDIR		0x0004
#define NNPFS_STALE		0x0008
#define NNPFS_XDELETED		0x0010
#define NNPFS_VMOPEN		0x0020

/*
 * Token match macros, NNPFS_TOKEN_GOT is depricated and
 * NNPFS_TOKEN_GOT_* should be used instead.
 */

/* Are necessary tokens available? */
#define NNPFS_TOKEN_GOT(xn, tok)      ((xn)->tokens & (tok))          /* deprecated */
#define NNPFS_TOKEN_GOT_ANY(xn, tok)  ((xn)->tokens & (tok))          /* at least one must match */
#define NNPFS_TOKEN_GOT_ALL(xn, tok)  (((xn)->tokens & (tok)) == (tok)) /* all tokens must match */
#define NNPFS_TOKEN_SET(xn, tok, mask)	((xn)->tokens |= ((tok) & (mask)))
#define NNPFS_TOKEN_CLEAR(xn, tok, mask)	((xn)->tokens &= ~((tok) & (mask)))

/* definitions for the rights fields */
#define NNPFS_RIGHT_R	0x01		/* may read? */
#define NNPFS_RIGHT_W	0x02		/* may write? */
#define NNPFS_RIGHT_X	0x04		/* may execute? */

/* Max name length passed in nnpfs messages */

#define NNPFS_MAX_NAME 256
#define NNPFS_MAX_SYMLINK_CONTENT 2048

struct nnpfs_msg_node {
    nnpfs_handle handle;
    uint32_t tokens;
    uint32_t pad1;
    struct nnpfs_attr attr;
    nnpfs_pag_t id[MAXRIGHTS];
    u_char rights[MAXRIGHTS];
    u_char anonrights;
    uint16_t pad2;
    uint32_t pad3;
};

/*
 * Messages passed through the  nnpfs_dev.
 */
struct nnpfs_message_header {
  uint32_t size;
  uint32_t opcode;
  uint32_t sequence_num;		/* Private */
  uint32_t pad1;
};

/*
 * Used by putdata flag
 */
enum { NNPFS_READ     = 0x01,
       NNPFS_WRITE    = 0x02,
       NNPFS_NONBLOCK = 0x04,
       NNPFS_APPEND   = 0x08,
       NNPFS_FSYNC    = 0x10};

/*
 * Flags for inactivenode
 */
enum { NNPFS_NOREFS = 1, NNPFS_DELETE = 2 };

/*
 * Flags for installdata
 */

enum { NNPFS_ID_INVALID_DNLC = 0x01, NNPFS_ID_AFSDIR = 0x02,
       NNPFS_ID_HANDLE_VALID = 0x04 };

/*
 * Defined message types and their opcodes.
 */
#define NNPFS_MSG_VERSION		0
#define NNPFS_MSG_WAKEUP		1

#define NNPFS_MSG_GETROOT		2
#define NNPFS_MSG_INSTALLROOT	3

#define NNPFS_MSG_GETNODE		4
#define NNPFS_MSG_INSTALLNODE	5

#define NNPFS_MSG_GETATTR		6
#define NNPFS_MSG_INSTALLATTR	7

#define NNPFS_MSG_GETDATA		8
#define NNPFS_MSG_INSTALLDATA	9

#define NNPFS_MSG_INACTIVENODE	10
#define NNPFS_MSG_INVALIDNODE	11
		/* XXX Must handle dropped/revoked tokens better */

#define NNPFS_MSG_OPEN		12

#define NNPFS_MSG_PUTDATA		13
#define NNPFS_MSG_PUTATTR		14

/* Directory manipulating messages. */
#define NNPFS_MSG_CREATE		15
#define NNPFS_MSG_MKDIR		16
#define NNPFS_MSG_LINK		17
#define NNPFS_MSG_SYMLINK		18

#define NNPFS_MSG_REMOVE		19
#define NNPFS_MSG_RMDIR		20

#define NNPFS_MSG_RENAME		21

#define NNPFS_MSG_PIOCTL		22
#define NNPFS_MSG_WAKEUP_DATA	23

#define NNPFS_MSG_UPDATEFID	24

#define NNPFS_MSG_ADVLOCK		25

#define NNPFS_MSG_GC_NODES	26

#define NNPFS_MSG_COUNT		27

/* NNPFS_MESSAGE_VERSION */
struct nnpfs_message_version {
  struct nnpfs_message_header header;
  uint32_t ret;
};

/* NNPFS_MESSAGE_WAKEUP */
struct nnpfs_message_wakeup {
  struct nnpfs_message_header header;
  uint32_t sleepers_sequence_num;	/* Where to send wakeup */
  uint32_t error;			/* Return value */
};

/* NNPFS_MESSAGE_GETROOT */
struct nnpfs_message_getroot {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
};

/* NNPFS_MESSAGE_INSTALLROOT */
struct nnpfs_message_installroot {
  struct nnpfs_message_header header;
  struct nnpfs_msg_node node;
};

/* NNPFS_MESSAGE_GETNODE */
struct nnpfs_message_getnode {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
};

/* NNPFS_MESSAGE_INSTALLNODE */
struct nnpfs_message_installnode {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_msg_node node;
};

/* NNPFS_MESSAGE_GETATTR */
struct nnpfs_message_getattr {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle handle;
};

/* NNPFS_MESSAGE_INSTALLATTR */
struct nnpfs_message_installattr {
  struct nnpfs_message_header header;
  struct nnpfs_msg_node node;
};

/* NNPFS_MESSAGE_GETDATA */
struct nnpfs_message_getdata {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle handle;
  uint32_t tokens;
  uint32_t pad1;
  uint32_t offset;
  uint32_t pad2;
};

/* NNPFS_MESSAGE_INSTALLDATA */
struct nnpfs_message_installdata {
  struct nnpfs_message_header header;
  struct nnpfs_msg_node node;
  char cache_name[NNPFS_MAX_NAME];
  struct nnpfs_cache_handle cache_handle;
  uint32_t flag;
  uint32_t pad1;
  uint32_t offset;
  uint32_t pad2;
};

/* NNPFS_MSG_INACTIVENODE */
struct nnpfs_message_inactivenode {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  uint32_t flag;
  uint32_t pad1;
};

/* NNPFS_MSG_INVALIDNODE */
struct nnpfs_message_invalidnode {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
};

/* NNPFS_MSG_OPEN */
struct nnpfs_message_open {
  struct nnpfs_message_header header;
  struct nnpfs_cred cred;
  nnpfs_handle handle;
  uint32_t tokens;
  uint32_t pad1;
};

/* NNPFS_MSG_PUTDATA */
struct nnpfs_message_putdata {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  struct nnpfs_attr attr;		/* XXX ??? */
  struct nnpfs_cred cred;
  uint32_t flag;
  uint32_t pad1;
};

/* NNPFS_MSG_PUTATTR */
struct nnpfs_message_putattr {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  struct nnpfs_attr attr;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_CREATE */
struct nnpfs_message_create {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_attr attr;
  uint32_t mode;
  uint32_t pad1;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_MKDIR */
struct nnpfs_message_mkdir {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_attr attr;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_LINK */
struct nnpfs_message_link {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  nnpfs_handle from_handle;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_SYMLINK */
struct nnpfs_message_symlink {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  char contents[NNPFS_MAX_SYMLINK_CONTENT];
  struct nnpfs_attr attr;
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_REMOVE */
struct nnpfs_message_remove {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_RMDIR */
struct nnpfs_message_rmdir {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  char name[NNPFS_MAX_NAME];
  struct nnpfs_cred cred;
};

/* NNPFS_MSG_RENAME */
struct nnpfs_message_rename {
  struct nnpfs_message_header header;
  nnpfs_handle old_parent_handle;
  char old_name[NNPFS_MAX_NAME];
  nnpfs_handle new_parent_handle;
  char new_name[NNPFS_MAX_NAME];
  struct nnpfs_cred cred;
};

#define NNPFS_MSG_MAX_DATASIZE	2048

/* NNPFS_MSG_PIOCTL */
struct nnpfs_message_pioctl {
  struct nnpfs_message_header header;
  uint32_t opcode ;
  uint32_t pad1;
  nnpfs_cred cred;
  uint32_t insize;
  uint32_t outsize;
  char msg[NNPFS_MSG_MAX_DATASIZE];
  nnpfs_handle handle;
};


/* NNPFS_MESSAGE_WAKEUP_DATA */
struct nnpfs_message_wakeup_data {
  struct nnpfs_message_header header;
  uint32_t sleepers_sequence_num;	/* Where to send wakeup */
  uint32_t error;			/* Return value */
  uint32_t len;
  uint32_t pad1;
  char msg[NNPFS_MSG_MAX_DATASIZE];
};

/* NNPFS_MESSAGE_UPDATEFID */
struct nnpfs_message_updatefid {
  struct nnpfs_message_header header;
  nnpfs_handle old_handle;
  nnpfs_handle new_handle;
};

/* NNPFS_MESSAGE_ADVLOCK */
struct nnpfs_message_advlock {
  struct nnpfs_message_header header;
  nnpfs_handle handle;
  struct nnpfs_cred cred;
  nnpfs_locktype_t locktype;
#define NNPFS_WR_LOCK 1 /* Write lock */
#define NNPFS_RD_LOCK 2 /* Read lock */
#define NNPFS_UN_LOCK 3 /* Unlock */
#define NNPFS_BR_LOCK 4 /* Break lock (inform that we don't want the lock) */
  nnpfs_lockid_t lockid;
};

/* NNPFS_MESSAGE_GC_NODES */
struct nnpfs_message_gc_nodes {
  struct nnpfs_message_header header;
#define NNPFS_GC_NODES_MAX_HANDLE 50
  uint32_t len;
  uint32_t pad1;
  nnpfs_handle handle[NNPFS_GC_NODES_MAX_HANDLE];
};

#if 0 
struct nnpfs_name {
    u_int16_t name;
    char name[1];
};
#endif

struct nnpfs_message_bulkgetnode {
  struct nnpfs_message_header header;
  nnpfs_handle parent_handle;
  uint32_t flags;
#define NNPFS_BGN_LAZY		0x1
  uint32_t numnodes;
  struct nnpfs_handle handles[1];
};

#endif /* _xmsg_h */
