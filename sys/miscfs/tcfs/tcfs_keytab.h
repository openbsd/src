#ifndef _SYS_TYPES_H_
#include "sys/types.h"
#endif
#ifndef _TCFS_MOUNT_H_
struct tcfs_mount;
#endif

#define _TCFS_KEYTAB_H_
#define	KEYTABSIZE	 20
#define KEYSIZE		8
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

typedef struct 
		{
			 unsigned char gui_flag;
			 uid_t gui_uid;
			 unsigned char gui_tcfskey[KEYPARTSIZE];
		} tcfs_grp_uinfo;

#define	GUI_CLEAN	0
#define GUI_SET		1
#define IS_SET_GUI(gui)	((gui).gui_flag==GUI_SET)

typedef struct _grp_data	{
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
int		_tcfs_keytab_hash(unsigned int);
#define		tcfs_keytab_hash(x)	_tcfs_keytab_hash((unsigned int)(x))
#else
#define		tcfs_keytab_hash(u)	((u)%KEYTABSIZE)
#endif

tcfs_keytab_node  	*tcfs_keytab_newnode(void);
tcfs_keytab_node  	*tcfs_keytab_newgidnode(void);
void			tcfs_keytab_dispnode(tcfs_keytab_node*);
tcfs_keytab 		*tcfs_keytab_init(void);
void			tcfs_keytab_dispose(tcfs_keytab*);
tcfs_keytab_node	*tcfs_keytab_fetch_uid(tcfs_keytab *, uid_t);
int			tcfs_keytab_push_gidpart(struct tcfs_mount *,tcfs_keytab_node *,uid_t, gid_t,int,char*);
tcfs_keytab_node	*tcfs_keytab_fetch_gid(tcfs_keytab *, gid_t);
tcfs_keytab_node	*tcfs_keytab_fetch_pid(tcfs_keytab *, uid_t, pid_t);
int			tcfs_keytab_push_uid(tcfs_keytab*, uid_t, void* );
int			tcfs_keytab_push_pid(tcfs_keytab*, uid_t, pid_t, void* );
int			tcfs_keytab_push_gid(struct tcfs_mount *,tcfs_keytab*, uid_t, gid_t,int,char*);
int			tcfs_keytab_rm_uid(tcfs_keytab*, uid_t);
int			tcfs_keytab_rm_pid(tcfs_keytab*, uid_t, pid_t);
int			tcfs_keytab_rm_gidpart(tcfs_keytab_node *,uid_t, gid_t);
int			tcfs_keytab_rm_gid(tcfs_keytab*, uid_t, gid_t);
int			tcfs_interp(struct tcfs_mount *, tcfs_keytab_node*);
int			tcfs_keytab_check_uid(tcfs_keytab *, uid_t);
int			tcfs_keytab_check_pid(tcfs_keytab *, uid_t, pid_t);
int			tcfs_keytab_check_gid(tcfs_keytab *, gid_t);
