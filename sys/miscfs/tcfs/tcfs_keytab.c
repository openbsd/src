#include<sys/errno.h>
#include<sys/types.h>
#include<sys/systm.h>
#ifndef _TCFS_KEYTAB_H_
#include "tcfs_keytab.h"
#endif
#include<sys/malloc.h>

tcfs_keytab_node *tcfs_keytab_newnode()
{
	tcfs_keytab_node *n;

	n=(tcfs_keytab_node*)malloc(sizeof(tcfs_keytab_node),M_FREE,M_NOWAIT); 
	if(!n)
		return n;

	n->kn_key=(void*)0;
	n->kn_data=(tcfs_grp_data*)0;
	n->kn_type=CLEAN;
	n->kn_n=n->kn_p=NIL;
	return n;
}

tcfs_keytab_node *tcfs_keytab_newgidnode()
{
	tcfs_keytab_node *n;
	tcfs_grp_data *gd;
	int i;

	n=tcfs_keytab_newnode();
	if(!n)
		return n;
	
	gd=(tcfs_grp_data*)malloc(sizeof(tcfs_grp_data),M_FREE,M_NOWAIT);
	if(!gd)
		{
			tcfs_keytab_dispnode(n);
			return NIL;
		}
	gd->gd_n=0;
	gd->gd_k=0;
	for(i=0;i<MAXUSRPERGRP;i++);
		{
		 gd->gd_part[i].gui_flag=GUI_CLEAN;
		 gd->gd_part[i].gui_uid=65535; /* nobody */
		}

	n->kn_data=gd;
	n->kn_type=GID_KEY;
	n->kn_key=(void*)0;

	return n;
}
	
	
	
void tcfs_keytab_dispnode(tcfs_keytab_node *n)
{
	if(n->kn_key)
		free(n->kn_key,M_FREE);


	if(n->kn_data)
		free(n->kn_data,M_FREE);

	if(n)
		free(n,M_FREE);
}
		
	

tcfs_keytab *tcfs_keytab_init()
{
	tcfs_keytab *x;
	int i;

	x=(tcfs_keytab*)malloc(sizeof(tcfs_keytab),M_FREE,M_NOWAIT);
	if (!x)
		return x;

	x->cnt=0;

	for(i=0;i<KEYTABSIZE;i++) /*una bzero o simile magari */
		x->node[i]=NIL;

	return x;
}

void tcfs_keytab_dispose(tcfs_keytab *kt)  
{
	tcfs_keytab_node *p,*q;
	int i=0;
		
	if(kt->cnt)
		 for(i=0;i<KEYTABSIZE;i++)
			{
			 p=kt->node[i];
			 while(p!=NIL)
			 	{
  			 	 q=p->kn_n;
			 	 tcfs_keytab_dispnode(p);
				 p=q;
				}
			}

	free(kt,M_FREE);
}

int tcfs_keytab_check_uid(tcfs_keytab *t, uid_t uid)
{
	int pos;
	tcfs_keytab_node *p;

	pos=tcfs_keytab_hash(uid);
	p=t->node[pos];

	while(p!=NIL)
	{
		if (IS_UID_NODE(p) && p->kn_uid==uid)
			return 1;
		p=p->kn_n;
	}
 	return 0;	
}


tcfs_keytab_node *tcfs_keytab_fetch_uid(tcfs_keytab *t, uid_t uid)
{
	tcfs_keytab_node *p;
	int pos;
	
	pos=tcfs_keytab_hash(uid);
	p=t->node[pos];
	while(p!=NIL)
	{
		if(IS_UID_NODE(p) && p->kn_uid==uid)
			break;
		p=p->kn_n;
	}
	return p;
}

int tcfs_keytab_check_gid(tcfs_keytab *t, gid_t gid)
{
	tcfs_keytab_node *p;
	int pos;
	
	pos=tcfs_keytab_hash(gid);
	p=t->node[pos];

	while (p!=NIL)
	{
		if(IS_GID_NODE(p) && p->kn_gid==gid && IS_READY_GD(p->kn_data))
			return 1;
		p=p->kn_n;
	}
	return 0;
}

		
tcfs_keytab_node *tcfs_keytab_fetch_gid(tcfs_keytab *t, gid_t gid)
{
	tcfs_keytab_node *p;
	int pos;
	
	pos=tcfs_keytab_hash(gid);
	p=t->node[pos];
	while(p!=NIL)
	{
		if(IS_GID_NODE(p) && p->kn_gid==gid)
			break;
		p=p->kn_n;
	}
	return p;
}
	
	

int tcfs_keytab_check_pid(tcfs_keytab *t, uid_t uid, pid_t pid)
{
	int pos;
	tcfs_keytab_node *p;

	pos=tcfs_keytab_hash(pid);
	p=t->node[pos];

	while(p!=NIL)
	{
		if (IS_PID_NODE(p) && p->kn_pid==pid && p->kn_uid==uid)
			return 1;
		p=p->kn_n;
	}
 	return 0;	
}

tcfs_keytab_node *tcfs_keytab_fetch_pid(tcfs_keytab *t, uid_t uid, pid_t pid)
{
	tcfs_keytab_node *p;
	int pos;
	
	pos=tcfs_keytab_hash(pid);
	p=t->node[pos];

	while(p!=NIL)
	{
		if(IS_PID_NODE(p) && p->kn_pid==pid && p->kn_uid==uid)
			break;
		p=p->kn_n;
	}
	return p;
}

int tcfs_keytab_push_pid(tcfs_keytab *t, uid_t uid, pid_t pid, void *ks) 
{
	int pos=0;
	tcfs_keytab_node *p,*q;
	
	if(tcfs_keytab_fetch_pid(t,uid,pid)!=NIL)
		return EINVAL;
	
	q=tcfs_keytab_newnode();
	if(!q)
		return ENOMEM;

	pos=tcfs_keytab_hash(pid);

	p=t->node[pos];
	q->kn_n=p;
	if(p!=NIL)
		p->kn_p=q;
	t->node[pos]=q;
	t->cnt++;

	q->kn_uid=uid;
	q->kn_pid=pid;
	q->kn_key=ks;
	q->kn_type=PID_KEY;

	return TCFS_OK;
}

int tcfs_keytab_push_uid(tcfs_keytab *t, uid_t uid, void *key)
{
	int pos=0;
	tcfs_keytab_node *p,*q;

	
	if(tcfs_keytab_fetch_uid(t,uid)!=NIL)
		return EINVAL;
	
	q=tcfs_keytab_newnode();
	if(!q)
		return ENOMEM;

	pos=tcfs_keytab_hash(uid);

	p=t->node[pos];
	q->kn_n=p;
	if(p!=NIL)
		p->kn_p=q;
	t->node[pos]=q;
	t->cnt++;

	q->kn_uid=uid;
	q->kn_pid=0;
	q->kn_key=key;
	q->kn_type=UID_KEY;

	return TCFS_OK;
}

int tcfs_keytab_push_gidpart(struct tcfs_mount *mp,tcfs_keytab_node *kn,uid_t uid, gid_t gid, int k,char *key)
{
	int i=0;
	int first=-1;
	tcfs_grp_data *p;

	p=kn->kn_data;
	
	if(IS_FULL_GD(p))
		return EINVAL;

	for (i=0;i<MAXUSRPERGRP;i++)
	{
		if(first<0 && !IS_SET_GUI(p->gd_part[i]))
			{
			 first=i;
			 continue;
			}
		if(IS_SET_GUI(p->gd_part[i]) && 
				p->gd_part[i].gui_uid==uid)
			return EINVAL;
	}
	
	p->gd_part[first].gui_uid=uid;
	p->gd_part[first].gui_flag=GUI_SET;
	memcpy(p->gd_part[first].gui_tcfskey,key,KEYPARTSIZE);
 	if(IS_CLEAN_GD(p))
		p->gd_k=k;	

	p->gd_n++;

	if(IS_READY_GD(p)&&(!kn->kn_key))
		tcfs_interp(mp,kn);
		
	return TCFS_OK;
}

int tcfs_keytab_rm_gidpart(tcfs_keytab_node *kn, uid_t uid, gid_t gid)
{
	int i=0;
	tcfs_grp_data *p;

	p=kn->kn_data;
	
	if(IS_CLEAN_GD(p))
		return EINVAL;

	for (i=0;i<MAXUSRPERGRP;i++)
		if(IS_SET_GUI(p->gd_part[i]))
			if(p->gd_part[i].gui_uid==uid)
				{
				 p->gd_part[i].gui_flag=GUI_CLEAN;
				 break;
				}
				
	if (i==MAXUSRPERGRP)
		return EINVAL;

	p->gd_n--;

	if(!IS_READY_GD(p))
		 if(kn->kn_key)
			{
		 	 free(kn->kn_key,M_FREE);
		 	 kn->kn_key=(void*)0;
			}
		
	return TCFS_OK;
}
	
int tcfs_keytab_push_gid(struct tcfs_mount *mp,tcfs_keytab *t, uid_t uid, gid_t gid, int k, char* key)
{
	int pos=0;
	tcfs_keytab_node *p,*q,*r;
	
	q=r=tcfs_keytab_fetch_gid(t,gid);

	if(r==NIL)
	{
		q=tcfs_keytab_newgidnode();
		if(!q)
			return ENOMEM;

		pos=tcfs_keytab_hash(gid);

		p=t->node[pos];
		q->kn_n=p;
		if(p!=NIL)
			p->kn_p=q;
		t->node[pos]=q;
		t->cnt++;

		q->kn_gid=gid;
		q->kn_pid=0;
		q->kn_uid=0;
	}
	
	return tcfs_keytab_push_gidpart(mp,q,uid,gid,k,key);
}

int tcfs_keytab_rm_uid(tcfs_keytab *t, uid_t uid)
{
	int pos=0;
	tcfs_keytab_node *p;

	p=tcfs_keytab_fetch_uid(t,uid);
	if(p==NIL)
		return EINVAL;

	if(p->kn_p==NIL)
		{
		 pos=tcfs_keytab_hash(uid);
		 t->node[pos]=p->kn_n;
		}
	else
		 p->kn_p->kn_n=p->kn_n;

	if(p->kn_n!=NIL)
		p->kn_n->kn_p=p->kn_p;	 
	
	t->cnt--;
	tcfs_keytab_dispnode(p);

	return TCFS_OK;
}
	
int tcfs_keytab_rm_pid(tcfs_keytab *t, uid_t uid, pid_t pid)
{
	int pos=0;
	tcfs_keytab_node *p;

	p=tcfs_keytab_fetch_pid(t,uid,pid);
	if(p==NIL)
		return EINVAL;

	if(p->kn_p==NIL)
		{
		 pos=tcfs_keytab_hash(pid);
		 t->node[pos]=p->kn_n;
		}
	else
		 p->kn_p->kn_n=p->kn_n;

	if(p->kn_n!=NIL)
		p->kn_n->kn_p=p->kn_p;	 
	
	t->cnt--;
	tcfs_keytab_dispnode(p);

	return TCFS_OK;
}

int tcfs_keytab_rm_gid(tcfs_keytab *t, uid_t uid, gid_t gid)
{
	int pos=0,ret=0;
	tcfs_keytab_node *p;

	p=tcfs_keytab_fetch_gid(t,gid);
	if(p==NIL)
		return EINVAL;

	ret=tcfs_keytab_rm_gidpart(p,uid,gid);

	if (ret)
		return ret; 	

	if(!IS_CLEAN_GD(p->kn_data))
		return TCFS_OK;

	if(p->kn_p==NIL)
		{
		 pos=tcfs_keytab_hash(gid);
		 t->node[pos]=p->kn_n;
		}
	else
		 p->kn_p->kn_n=p->kn_n;

	if(p->kn_n!=NIL)
		p->kn_n->kn_p=p->kn_p;	 
	
	t->cnt--;
	tcfs_keytab_dispnode(p);

	return TCFS_OK;
}

