/*	$OpenBSD: tcfs_keytab.h,v 1.6 2002/03/14 01:27:08 millert Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _TCFS_KEYTAB_H_
#define _TCFS_KEYTAB_H_

#include <sys/types.h>
struct tcfs_mount;

#define	KEYTABSIZE	20
#define KEYSIZE		32
#define KEYPARTSIZE	(KEYSIZE+KEYSIZE/8)

#define CLEAN		0x00
#define	PID_BIT		0x01
#define UID_KEY		0x02
#define GID_KEY		0x04
#define PID_KEY		(PID_BIT|UID_KEY)

#define IS_GID_NODE(np)	(((np)->kn_type)&GID_KEY)
#define IS_UID_NODE(np)	(((np)->kn_type)&UID_KEY)
#define IS_PID_NODE(np)	((((np)->kn_type)&PID_BIT)&&(IS_UID_NODE((np))))

#ifndef TCFS_OK
#define TCFS_OK 0
#endif

#define MAXUSRPERGRP	10

typedef struct {
	unsigned char gui_flag;
	uid_t gui_uid;
	unsigned char gui_tcfskey[KEYPARTSIZE];
} tcfs_grp_uinfo;

#define	GUI_CLEAN	0
#define GUI_SET		1
#define IS_SET_GUI(gui)	((gui).gui_flag==GUI_SET)

typedef struct _grp_data {
	int gd_flag;
	int gd_n;
	int gd_k;
	tcfs_grp_uinfo gd_part[MAXUSRPERGRP];
} tcfs_grp_data;

#define IS_CLEAN_GD(gd)	((gd)->gd_n==0)
#define IS_FULL_GD(gd)	((gd)->gd_n==MAXUSRPERGRP)
#define IS_READY_GD(gd)	(((gd)->gd_n)>=((gd)->gd_k))

typedef struct _kn {
	pid_t 		 kn_pid;
	uid_t 		 kn_uid;
	gid_t		 kn_gid;
	unsigned int	 kn_type;

	void *kn_key;
#undef kn_data
	tcfs_grp_data *kn_data;

	struct _kn *kn_n;
	struct _kn *kn_p;
}	tcfs_keytab_node;

typedef struct 	_kt {
	unsigned int cnt;
	tcfs_keytab_node* node[KEYTABSIZE]; 
} tcfs_keytab;

#define	NIL	((tcfs_keytab_node*)0)

#ifdef _HAVE_HASH_
int		_tcfs_keytab_hash __P(unsigned int));
#define		tcfs_keytab_hash(x)	_tcfs_keytab_hash((unsigned int)(x))
#else
#define		tcfs_keytab_hash(u)	((u)%KEYTABSIZE)
#endif

tcfs_keytab_node	*tcfs_keytab_newnode(void);
tcfs_keytab_node	*tcfs_keytab_newgidnode(void);
void			 tcfs_keytab_dispnode(tcfs_keytab_node*);
tcfs_keytab		*tcfs_keytab_init(void);
void			 tcfs_keytab_dispose(tcfs_keytab*);
tcfs_keytab_node	*tcfs_keytab_fetch_uid(tcfs_keytab *, uid_t);
int			 tcfs_keytab_push_gidpart(struct tcfs_mount *,tcfs_keytab_node *,uid_t, gid_t,int,char*);
tcfs_keytab_node	*tcfs_keytab_fetch_gid(tcfs_keytab *, gid_t);
tcfs_keytab_node	*tcfs_keytab_fetch_pid(tcfs_keytab *, uid_t, pid_t);
int			 tcfs_keytab_push_uid(tcfs_keytab*, uid_t, void* );
int			 tcfs_keytab_push_pid(tcfs_keytab*, uid_t, pid_t, void *);
int			 tcfs_keytab_push_gid(struct tcfs_mount *,tcfs_keytab *, uid_t, gid_t, int, char *);
int			 tcfs_keytab_rm_uid(tcfs_keytab *, uid_t);
int			 tcfs_keytab_rm_pid(tcfs_keytab *, uid_t, pid_t);
int			 tcfs_keytab_rm_gidpart(tcfs_keytab_node *,uid_t, gid_t);
int			 tcfs_keytab_rm_gid(tcfs_keytab*, uid_t, gid_t);
int			 tcfs_interp(struct tcfs_mount *, tcfs_keytab_node*);
int			 tcfs_keytab_check_uid(tcfs_keytab *, uid_t);
int			 tcfs_keytab_check_pid(tcfs_keytab *, uid_t, pid_t);
int			 tcfs_keytab_check_gid(tcfs_keytab *, gid_t);
#endif /* _TCFS_KEYTAB_H_ */
