OBSOLETE /* COPYRIGHT  (C)  1998
OBSOLETE  * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
OBSOLETE  * ALL RIGHTS RESERVED
OBSOLETE  * 
OBSOLETE  * PERMISSION IS GRANTED TO USE, COPY, CREATE DERIVATIVE WORKS 
OBSOLETE  * AND REDISTRIBUTE THIS SOFTWARE AND SUCH DERIVATIVE WORKS 
OBSOLETE  * FOR ANY PURPOSE, SO LONG AS THE NAME OF THE UNIVERSITY OF 
OBSOLETE  * MICHIGAN IS NOT USED IN ANY ADVERTISING OR PUBLICITY 
OBSOLETE  * PERTAINING TO THE USE OR DISTRIBUTION OF THIS SOFTWARE 
OBSOLETE  * WITHOUT SPECIFIC, WRITTEN PRIOR AUTHORIZATION.  IF THE 
OBSOLETE  * ABOVE COPYRIGHT NOTICE OR ANY OTHER IDENTIFICATION OF THE 
OBSOLETE  * UNIVERSITY OF MICHIGAN IS INCLUDED IN ANY COPY OF ANY 
OBSOLETE  * PORTION OF THIS SOFTWARE, THEN THE DISCLAIMER BELOW MUST 
OBSOLETE  * ALSO BE INCLUDED.
OBSOLETE  * 
OBSOLETE  * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION 
OBSOLETE  * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY 
OBSOLETE  * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF 
OBSOLETE  * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING 
OBSOLETE  * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF 
OBSOLETE  * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE 
OBSOLETE  * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE 
OBSOLETE  * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR 
OBSOLETE  * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING 
OBSOLETE  * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN 
OBSOLETE  * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF 
OBSOLETE  * SUCH DAMAGES.
OBSOLETE  */
OBSOLETE 
OBSOLETE /*
OBSOLETE  * Do merging of the files that changed with we was in disconnected mode
OBSOLETE  */
OBSOLETE 
OBSOLETE #if 0
OBSOLETE 
OBSOLETE #include "arla_local.h"
OBSOLETE 
OBSOLETE RCSID("$arla: reconnect.c,v 1.37 2002/09/07 10:43:27 lha Exp $");
OBSOLETE 
OBSOLETE static int reconnect_nonmute(struct vcache *, int, struct timeval);
OBSOLETE static int reconnect_putattr(struct vcache *, struct nnpfs_attr *);
OBSOLETE static int reconnect_putdata(struct vcache *);    
OBSOLETE static int reconnect_remove(struct vcache *, FCacheEntry *childentry, char *);
OBSOLETE static int reconnect_rename(struct vcache *, struct vcache *, char *, char *);
OBSOLETE static int reconnect_rmdir(struct vcache *vcp, FCacheEntry *childEntry,
OBSOLETE 			   char *name);
OBSOLETE static int reconnect_mkdir(struct vcache *parent, struct vcache *curdir, 
OBSOLETE 			   AFSStoreStatus *store_status, char *name);
OBSOLETE static int reconnect_link(struct vcache *parent, struct vcache *existing,
OBSOLETE 			  char *name);
OBSOLETE static int reconnect_symlink(struct vcache *parent, struct vcache *child,
OBSOLETE 			     AFSStoreStatus *store_attr, char *name, 
OBSOLETE 			     char *contents);
OBSOLETE static int reconnect_create(struct vcache *parent, struct vcache *child,
OBSOLETE 			    char *name);
OBSOLETE static int reconnect_mut_chk(FCacheEntry *fce, CredCacheEntry *ce,
OBSOLETE 			     int version);
OBSOLETE 
OBSOLETE static int check_log_todo(log_ent_t * , VenusFid *, VenusFid *);
OBSOLETE static int is_done_before(int no);
OBSOLETE static void add_done_list(int no);
OBSOLETE static void clear_log_entry(void);
OBSOLETE static void clear_index_list(void);
OBSOLETE 
OBSOLETE typedef struct _fid_trans{
OBSOLETE     VenusFid logged_fid;
OBSOLETE     VenusFid fetched_fid;
OBSOLETE     struct _fid_trans *next, *prev;
OBSOLETE } fid_trans;
OBSOLETE 
OBSOLETE typedef struct _fid_keep{
OBSOLETE     char name[MAX_NAME];
OBSOLETE     AFSFid kept_fid;
OBSOLETE     struct _fid_keep *next;
OBSOLETE } fid_keep;
OBSOLETE 
OBSOLETE typedef struct _index_list{
OBSOLETE     int   index;
OBSOLETE     struct _index_list * next;
OBSOLETE } index_list;
OBSOLETE 
OBSOLETE fid_trans *fid_AD_head, *fid_AD_tail;
OBSOLETE fid_keep  *fid_KP_head;
OBSOLETE index_list * index_head;
OBSOLETE extern log_ent_t  log_head;
OBSOLETE 
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void
OBSOLETE set_fid_value(VenusFid *new, VenusFid *old)
OBSOLETE {
OBSOLETE     if(old==0) {
OBSOLETE 	new->fid.Volume = 0;
OBSOLETE 	new->fid.Vnode = 0;
OBSOLETE 	new->fid.Unique = 0;
OBSOLETE     } else {
OBSOLETE 	new->fid.Volume = old->fid.Volume;
OBSOLETE 	new->fid.Vnode  = old->fid.Vnode;
OBSOLETE         new->fid.Unique = old->fid.Unique;
OBSOLETE     }
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE void 
OBSOLETE do_replay(char *log_file, int log_entries, VenusFid *changed_fid)
OBSOLETE {
OBSOLETE     int fd, len, i;
OBSOLETE     log_ent_t *cur_log;
OBSOLETE     struct vcache vc, vc_new;
OBSOLETE     char *name, *name_new;
OBSOLETE     fid_trans *fid_tmp;
OBSOLETE     fid_keep * fid_KP_tail;
OBSOLETE     VenusFid new_fid; 
OBSOLETE 
OBSOLETE     int count=1; /* Used to record how may actions we have done*/
OBSOLETE 
OBSOLETE     fid_AD_tail = fid_AD_head;
OBSOLETE     cur_log = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     fd = open(log_file, O_RDWR | O_BINARY);
OBSOLETE 
OBSOLETE     set_fid_value(&new_fid , 0);
OBSOLETE 
OBSOLETE     while (read(fd, cur_log, sizeof(int))){
OBSOLETE 
OBSOLETE 	if (cur_log->log_len < sizeof(*cur_log) - sizeof(cur_log->log_data) ||
OBSOLETE             cur_log->log_len > sizeof(log_ent_t)) {
OBSOLETE 	    arla_log(ADEBDISCONN, "do_replay: corrupt log entry, log_len %d",
OBSOLETE 		     cur_log->log_len);
OBSOLETE 	    goto terminate;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	len = cur_log->log_len - sizeof(int);
OBSOLETE 
OBSOLETE 	if (!read(fd, ((char *)cur_log + sizeof(int)), len)){
OBSOLETE 	    arla_log(ADEBDISCONN, "do_replay: read bad log entry...");
OBSOLETE 	    goto terminate;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	arla_log(ADEBDISCONN, 
OBSOLETE 		 "do_replay: read %d bytes of log entry.", 
OBSOLETE 		 cur_log->log_len);
OBSOLETE 
OBSOLETE 	if (is_done_before(cur_log->log_index)==1)
OBSOLETE 	    continue; /* the log entry has been executed */
OBSOLETE 	else {
OBSOLETE 	    if (changed_fid !=0) {
OBSOLETE 		int is_log_todo = 0;
OBSOLETE  
OBSOLETE 		is_log_todo = check_log_todo(cur_log, changed_fid, &new_fid);
OBSOLETE 		if (is_log_todo ==0)
OBSOLETE 		    continue; /* This log should not be  executed */
OBSOLETE 	    }
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	add_done_list(cur_log->log_index);
OBSOLETE 
OBSOLETE 	/* big case/switch statement to switch log_op  */
OBSOLETE 	switch (cur_log->log_op){
OBSOLETE 
OBSOLETE 	case DIS_STORE:
OBSOLETE 	    vc.fid = cur_log->st_fid;
OBSOLETE 	    vc.DataVersion = cur_log->st_origdv-1;
OBSOLETE 	    vc.flag = cur_log->st_flag;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** (putdata) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_putdata(&vc);
OBSOLETE 	    break;
OBSOLETE 	case DIS_SETATTR:
OBSOLETE 	    vc.fid = cur_log->sa_fid;
OBSOLETE 	    vc.DataVersion = cur_log->sa_origdv;
OBSOLETE 	    vc.flag = 0;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** (putattr) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_putattr(&vc, &(cur_log->sa_vattr));
OBSOLETE 	    break;
OBSOLETE 	case DIS_REMOVE: {
OBSOLETE 	    FCacheEntry *childentry;
OBSOLETE 	    vc.fid = cur_log->rm_filefid;
OBSOLETE 	    vc.DataVersion = cur_log->rm_origdv;
OBSOLETE 	    vc.flag = 0;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    childentry = cur_log->rm_chentry;
OBSOLETE 	    name = cur_log->rm_name;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** "
OBSOLETE 		     "(file remove) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_remove(&vc, childentry, name);
OBSOLETE 	    break;
OBSOLETE 	}
OBSOLETE 	case DIS_RMDIR: {
OBSOLETE 	    FCacheEntry *child;
OBSOLETE 	    vc.fid = cur_log->rd_parentfid;
OBSOLETE 	    name = cur_log->rd_name;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    child = cur_log->rd_direntry;
OBSOLETE 	    arla_log (ADEBDISCONN,
OBSOLETE 		      "%d action is to do_replay: **rmdir** "
OBSOLETE 		      "(directory remove) op...",
OBSOLETE 		      count++);
OBSOLETE 	    reconnect_rmdir(&vc, child, name);
OBSOLETE 	    break;
OBSOLETE 	}
OBSOLETE 	case DIS_RENAME:
OBSOLETE 	    vc.fid = cur_log->rn_oparentfid;
OBSOLETE 	    vc.DataVersion = cur_log->rn_origdv;
OBSOLETE 	    vc.flag = 0;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    vc_new.fid = cur_log->rn_nparentfid;
OBSOLETE 	    vc_new.DataVersion = cur_log->rn_overdv;
OBSOLETE 	    vc_new.flag = 0;
OBSOLETE 	    vc_new.cred = cur_log->cred;
OBSOLETE 	    name = cur_log->rn_names;
OBSOLETE 	    for (i=0; *(name+i)!='\0';++i);
OBSOLETE 	    name_new = name+i+1; 
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** (rename) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_rename(&vc, &vc_new, name, name_new);
OBSOLETE 	    break;
OBSOLETE 	case DIS_MKDIR:	{ 
OBSOLETE 	    AFSStoreStatus store_status;
OBSOLETE 	    vc.fid = cur_log->md_parentfid;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    store_status = cur_log->md_vattr;
OBSOLETE 	    vc_new.fid = cur_log->md_dirfid;
OBSOLETE 	    /*Ba Wu: child's data vers*/
OBSOLETE 	    vc_new.DataVersion = cur_log->md_dversion; 
OBSOLETE 	    name = cur_log->md_name;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to DO_Replay: **replay** (mkdir) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_mkdir(&vc, &vc_new, &store_status, name);
OBSOLETE 	    break; 
OBSOLETE 	}
OBSOLETE 	case DIS_LINK:
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    vc.fid = cur_log->ln_parentfid;
OBSOLETE 	    vc_new.fid = cur_log->ln_linkfid;
OBSOLETE 	    name = cur_log->ln_name;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** (link) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_link(&vc, &vc_new, name);
OBSOLETE 	    break;
OBSOLETE 	case DIS_SYMLINK: {
OBSOLETE 	    char *new_name;
OBSOLETE 	    AFSStoreStatus store_attr;
OBSOLETE 	    
OBSOLETE 	    vc.fid = cur_log->sy_parentfid;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    name = cur_log->sy_name;
OBSOLETE 	    new_name = cur_log->sy_content;
OBSOLETE 	    vc_new.fid = cur_log->sy_filefid;
OBSOLETE 	    store_attr = cur_log->sy_attr;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** (symlink) op...",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_symlink(&vc, &vc_new, &store_attr, name, new_name);
OBSOLETE 	    break;
OBSOLETE 	}
OBSOLETE 	case DIS_CREATE:
OBSOLETE 	    vc.fid = cur_log->cr_parentfid;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE  
OBSOLETE 	    vc_new.fid = cur_log->cr_filefid;
OBSOLETE 	    vc_new.DataVersion = cur_log->cr_origdv;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to DO_Replay: **replay** (create) op...",
OBSOLETE 		     count++);
OBSOLETE 	    name = cur_log->cr_name;
OBSOLETE 	    reconnect_create(&vc, &vc_new, name); 
OBSOLETE 	    break; 
OBSOLETE 	case DIS_ACCESS:
OBSOLETE 	    vc.fid = cur_log->nm_fid;
OBSOLETE 	    vc.DataVersion = cur_log->nm_origdv;
OBSOLETE 	    vc.cred = cur_log->cred;
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: **replay** (nonmutating) op",
OBSOLETE 		     count++);
OBSOLETE 	    reconnect_nonmute(&vc, cur_log->log_op, cur_log->log_time);
OBSOLETE 	    break; 
OBSOLETE 
OBSOLETE 	default:
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "%d action is to do_replay: skipping the current op=%d", 
OBSOLETE 		     count++,cur_log->log_op);
OBSOLETE 	}
OBSOLETE     }  
OBSOLETE 
OBSOLETE     if (changed_fid ==0) {
OBSOLETE 	clear_index_list();  /* clean all index when after discon */
OBSOLETE 	clear_log_entry();
OBSOLETE         /* clean up, remove all associative data structures */
OBSOLETE         fid_AD_tail = fid_AD_head;
OBSOLETE   	while(fid_AD_tail)
OBSOLETE   	{
OBSOLETE 	    fid_tmp = fid_AD_tail->next;
OBSOLETE 	    free(fid_AD_tail);
OBSOLETE 	    fid_AD_tail = fid_tmp; 
OBSOLETE   	}   
OBSOLETE 	/* SWW Qiyue 28: We need to reset head to 0*/
OBSOLETE   	fid_AD_head = 0;
OBSOLETE   	fid_KP_tail = fid_KP_head;
OBSOLETE     	while(fid_KP_tail)
OBSOLETE     	{
OBSOLETE 	    fid_keep *fid_tmp;
OBSOLETE 	
OBSOLETE 	    fid_tmp = fid_KP_tail->next;
OBSOLETE 	    free(fid_KP_tail);
OBSOLETE 	    fid_KP_tail = fid_tmp;
OBSOLETE     	}
OBSOLETE    	 
OBSOLETE   	fid_KP_head = 0;
OBSOLETE     }
OBSOLETE     i = ftruncate (fd, 0);
OBSOLETE     assert (i == 0);
OBSOLETE 
OBSOLETE  terminate:
OBSOLETE 
OBSOLETE     arla_warnx (ADEBDISCONN,"We have done total %d replays",count-1);
OBSOLETE     close(fd);  
OBSOLETE     free(cur_log);
OBSOLETE     return;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE check_rm_fid (VenusFid v1, VenusFid v2)
OBSOLETE {
OBSOLETE     if(v1.fid.Vnode == v2.fid.Vnode &&
OBSOLETE        v1.fid.Volume == v2.fid.Volume &&
OBSOLETE        v1.fid.Unique == v2.fid.Unique )
OBSOLETE 	return 1;
OBSOLETE     else
OBSOLETE 	return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE check_log_fid(struct vcache vc, VenusFid *fid)
OBSOLETE {
OBSOLETE     log_ent_t *temp = log_head.next;
OBSOLETE 
OBSOLETE     if (vc.fid.fid.Vnode == fid->fid.Vnode &&
OBSOLETE 	vc.fid.fid.Volume == fid->fid.Volume &&
OBSOLETE 	vc.fid.fid.Unique == fid->fid.Unique)
OBSOLETE 	return 1;
OBSOLETE 
OBSOLETE     while (temp!=0) {
OBSOLETE 	switch(temp->log_op) {
OBSOLETE 	case DIS_RENAME:
OBSOLETE 	    if (check_rm_fid(temp->rn_oparentfid,*fid)==1) 
OBSOLETE 		return 1;
OBSOLETE 	default: 
OBSOLETE 	    temp = temp->next;
OBSOLETE 	    break;
OBSOLETE         }
OBSOLETE     }
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE check_log_todo(log_ent_t * cur_log , VenusFid *fid, VenusFid *newfid)
OBSOLETE {
OBSOLETE     VenusFid *lookfid;
OBSOLETE     struct vcache vc, vc_new;
OBSOLETE     int will_do = 0;
OBSOLETE 
OBSOLETE     if (newfid->fid.Vnode ==0 &&
OBSOLETE 	newfid->fid.Volume == 0 &&
OBSOLETE 	newfid->fid.Unique ==0)
OBSOLETE 	lookfid = fid;
OBSOLETE     else
OBSOLETE 	lookfid = newfid; /* For create and putdata */
OBSOLETE 
OBSOLETE     switch (cur_log->log_op){
OBSOLETE     case DIS_STORE:
OBSOLETE         vc.fid = cur_log->st_fid;
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    set_fid_value(newfid, 0);
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE         break;
OBSOLETE     case DIS_SETATTR:
OBSOLETE 	vc.fid = cur_log->sa_fid;
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    set_fid_value(newfid , &cur_log->sa_fid);
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE         break;
OBSOLETE     case DIS_REMOVE:
OBSOLETE 	vc.fid = cur_log->rm_filefid;
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE         break;
OBSOLETE     case DIS_RMDIR:
OBSOLETE 	vc.fid = cur_log->rd_parentfid;
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE         break;
OBSOLETE     case DIS_RENAME:
OBSOLETE 	vc.fid = cur_log->rn_oparentfid;
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE 	vc_new.fid = cur_log->rn_nparentfid;
OBSOLETE 	will_do = check_log_fid(vc_new, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE         break;
OBSOLETE     case DIS_MKDIR:
OBSOLETE 	vc.fid = cur_log->md_parentfid;
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE 	break;
OBSOLETE     case DIS_LINK:
OBSOLETE         break;
OBSOLETE     case DIS_SYMLINK:
OBSOLETE 	will_do = check_log_fid(vc, lookfid);
OBSOLETE         if (will_do==1) {
OBSOLETE 	    return 1;
OBSOLETE         }
OBSOLETE         break;
OBSOLETE     case DIS_CREATE:
OBSOLETE 	vc.fid = cur_log->cr_parentfid;
OBSOLETE         will_do = check_log_fid(vc, lookfid);
OBSOLETE 	if (will_do==1) {
OBSOLETE 	    set_fid_value(newfid , &cur_log->cr_filefid);
OBSOLETE 	    return 1;
OBSOLETE 	}
OBSOLETE 	break;
OBSOLETE     case DIS_FSYNC:
OBSOLETE     case DIS_ACCESS:
OBSOLETE     case DIS_READDIR:
OBSOLETE     case DIS_READLINK:
OBSOLETE     case DIS_INFO:
OBSOLETE     case DIS_START_OPT:
OBSOLETE     case DIS_END_OPT:
OBSOLETE     case DIS_REPLAYED:
OBSOLETE 	/* A no op */
OBSOLETE 	break;
OBSOLETE     }
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE #if 0
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE void
OBSOLETE keepfid_newrename(char *name,
OBSOLETE 		  AFSFid fid)
OBSOLETE {
OBSOLETE     if (fid_KP_head == 0) {
OBSOLETE         fid_KP_head = (fid_keep *)malloc(sizeof(fid_keep));
OBSOLETE         assert(fid_KP_head);
OBSOLETE 
OBSOLETE         strcpy(fid_KP_head->name, name);
OBSOLETE 	fid_KP_head->kept_fid = fid;
OBSOLETE 	fid_KP_head->next = 0;
OBSOLETE     }
OBSOLETE     else { 
OBSOLETE         fid_keep *temp;
OBSOLETE 
OBSOLETE         temp = (fid_keep *)malloc(sizeof(fid_keep));
OBSOLETE 	assert(temp);
OBSOLETE 
OBSOLETE         strcpy(temp->name, name);
OBSOLETE 	temp->name[strlen(name)] = '\0';
OBSOLETE 	temp->kept_fid = fid;
OBSOLETE 	temp->next = fid_KP_head->next;
OBSOLETE 	fid_KP_head->next = temp;
OBSOLETE     }
OBSOLETE }
OBSOLETE 
OBSOLETE #endif
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE find_venus (char *name, VenusFid *fid)
OBSOLETE {
OBSOLETE     fid_keep *fid_temp;
OBSOLETE 
OBSOLETE     if(fid_KP_head == 0 )
OBSOLETE 	return 1;  /*error */
OBSOLETE   	
OBSOLETE     fid_temp = fid_KP_head;
OBSOLETE 
OBSOLETE     while(fid_temp) {
OBSOLETE 	if (strcmp(fid_temp->name,name) == 0) {
OBSOLETE 	    fid->fid.Volume = fid_temp->kept_fid.Volume;
OBSOLETE 	    fid->fid.Vnode = fid_temp->kept_fid.Vnode;
OBSOLETE 	    fid->fid.Unique = fid_temp->kept_fid.Unique;
OBSOLETE 	    return 0;
OBSOLETE         }
OBSOLETE 	fid_temp = fid_temp->next;
OBSOLETE     }
OBSOLETE     arla_warnx (ADEBDISCONN, "find_venus: *PANIC* not found fid for %s", name);
OBSOLETE     return 1;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE VenusFid *
OBSOLETE fid_translate(VenusFid *fid_in)
OBSOLETE {
OBSOLETE     fid_trans *fid_tmp;
OBSOLETE     VenusFid *fid_ret;
OBSOLETE 
OBSOLETE     if (!fid_AD_head)
OBSOLETE 	return fid_in;
OBSOLETE 
OBSOLETE     fid_tmp = fid_AD_head;
OBSOLETE 
OBSOLETE     while(fid_tmp){
OBSOLETE 
OBSOLETE 	fid_ret=&fid_tmp->logged_fid;
OBSOLETE 
OBSOLETE 	if ((fid_ret->Cell == fid_in->Cell) && 
OBSOLETE 	    (fid_ret->fid.Volume == fid_in->fid.Volume) && 
OBSOLETE 	    (fid_ret->fid.Vnode == fid_in->fid.Vnode) && 
OBSOLETE 	    (fid_ret->fid.Unique == fid_in->fid.Unique))
OBSOLETE 	    return &fid_tmp->fetched_fid;
OBSOLETE 
OBSOLETE 	fid_tmp = fid_tmp->next;	   
OBSOLETE     }
OBSOLETE     return fid_in; 
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void 
OBSOLETE clear_index_list(void)
OBSOLETE {
OBSOLETE     index_list *temp=index_head;
OBSOLETE 
OBSOLETE     while(temp!=0) {
OBSOLETE 	index_list *tmp;
OBSOLETE     
OBSOLETE   	tmp = temp->next;
OBSOLETE         free(temp);
OBSOLETE 	temp = tmp;
OBSOLETE     }
OBSOLETE     index_head = 0;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void 
OBSOLETE clear_log_entry(void)
OBSOLETE {
OBSOLETE     log_ent_t *temp=log_head.next;
OBSOLETE 
OBSOLETE     while(temp!=0) {             
OBSOLETE         log_ent_t *tmp;
OBSOLETE 
OBSOLETE         tmp = temp->next;
OBSOLETE         free(temp);                          
OBSOLETE         temp = tmp;
OBSOLETE     }                        
OBSOLETE     log_head.next = 0;    
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE is_done_before(int no)
OBSOLETE {
OBSOLETE     index_list * temp = index_head;
OBSOLETE 
OBSOLETE     while(temp !=0) {
OBSOLETE 	if (temp->index == no)
OBSOLETE 	    return 1;
OBSOLETE 	else
OBSOLETE 	    temp = temp->next;
OBSOLETE     }
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void 
OBSOLETE add_done_list(int no)
OBSOLETE {
OBSOLETE     if (!index_head) {
OBSOLETE 	index_head = (index_list *)malloc(sizeof(index_list));
OBSOLETE 	assert(index_head);
OBSOLETE 	index_head->index = no;
OBSOLETE 	index_head->next = 0;
OBSOLETE     } else {
OBSOLETE 	index_list * temp;
OBSOLETE 	temp = (index_list *) malloc(sizeof(index_list));
OBSOLETE 	assert(temp);
OBSOLETE 	temp->next = index_head->next;
OBSOLETE 	index_head->next = temp;
OBSOLETE 	temp->index = no;
OBSOLETE     }
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void 
OBSOLETE alloc_fid_trans(VenusFid *logged_fid)
OBSOLETE {
OBSOLETE     if (!fid_AD_head)
OBSOLETE     {
OBSOLETE 	/*SWW Qiyue 28 Make sure we have the memory*/
OBSOLETE 	fid_AD_head = (fid_trans *) malloc(sizeof(fid_trans)); 
OBSOLETE 	assert(fid_AD_head);
OBSOLETE 
OBSOLETE 	fid_AD_head->prev=fid_AD_head->next = 0;
OBSOLETE 	fid_AD_tail = fid_AD_head;
OBSOLETE 	fid_AD_tail->logged_fid = *logged_fid;
OBSOLETE 
OBSOLETE     } else{
OBSOLETE 
OBSOLETE 	/*SWW Qiyue 28 Make sure we have the memory*/
OBSOLETE 	fid_AD_tail->next = (fid_trans *) malloc(sizeof(fid_trans)); 
OBSOLETE 	assert(fid_AD_tail->next);
OBSOLETE 
OBSOLETE 	fid_AD_tail->next->prev = fid_AD_tail;
OBSOLETE 	fid_AD_tail->next->next = 0;
OBSOLETE 	fid_AD_tail = fid_AD_tail->next;  /*Ba ba: move tail ahead */
OBSOLETE 	fid_AD_tail->logged_fid = *logged_fid;
OBSOLETE     }
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void 
OBSOLETE fill_fid_trans (VenusFid *fetched_fid)
OBSOLETE {
OBSOLETE     fid_AD_tail->fetched_fid = *fetched_fid;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE #if 0
OBSOLETE void
OBSOLETE update_entry_flag (FCacheEntry *entry)
OBSOLETE {
OBSOLETE     entry->flags.attrp = FALSE;
OBSOLETE     entry->flags.datap = FALSE;
OBSOLETE }
OBSOLETE #endif
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_nonmute(struct vcache *vcp, int op, struct timeval log_time)
OBSOLETE {
OBSOLETE     FCacheEntry *fce, fce_fetched;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     int error;
OBSOLETE     VenusFid *fid; 
OBSOLETE #if 0
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE #endif
OBSOLETE 
OBSOLETE     arla_warnx (ADEBDISCONN,
OBSOLETE 		"Start of reconnect_nonmute by sww"); /*SWW Qiyue 25*/
OBSOLETE 
OBSOLETE     fid = &(vcp->fid);
OBSOLETE     if (fid->Cell == -1) /* newly created file, skip reconnect */
OBSOLETE 	return 0;
OBSOLETE 
OBSOLETE     error = fcache_find(&fce, *fid);
OBSOLETE     /* assert(fce); */
OBSOLETE     if (error) /* nonmute op on nonexisting data */
OBSOLETE     {     
OBSOLETE 	arla_log(ADEBDISCONN, 
OBSOLETE 		 "reconnect: nonmute op %d performed on cache "
OBSOLETE 		 "entry no longer exist locally!", 
OBSOLETE 		 op); 
OBSOLETE 	return -1;
OBSOLETE     }   
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, 
OBSOLETE 	     "reconnect: DISCONNECTED nonmute "
OBSOLETE 	     "on fid.Cell=0x%x, fid.fid.Volume= 0x%x, fid.fid.Vnode=0x%x, "
OBSOLETE 	     "fid.fid.Unique=0x%x", fid->Cell, 
OBSOLETE 	     fid->fid.Volume,
OBSOLETE 	     fid->fid.Vnode, 
OBSOLETE 	     fid->fid.Unique);  
OBSOLETE 
OBSOLETE     ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE     error = 0;
OBSOLETE   
OBSOLETE     /* setting some stuff so do_read_attr would work */
OBSOLETE     fce->flags.attrp = TRUE;
OBSOLETE     fce->callback.CallBackType = 2;
OBSOLETE     fce_fetched = *fce;
OBSOLETE     /*conn = findconn (fce, ce);*/
OBSOLETE 
OBSOLETE     error = read_attr(&fce_fetched, ce);
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, 
OBSOLETE 	     "reconnect: logged DataVersion=%d, "
OBSOLETE 	     "fetched DataVersion=%d", 
OBSOLETE 	     vcp->DataVersion,
OBSOLETE 	     fce_fetched.status.DataVersion);
OBSOLETE  
OBSOLETE     if (vcp->DataVersion < fce_fetched.status.DataVersion)
OBSOLETE     {
OBSOLETE 	if (log_time.tv_usec >= fce_fetched.status.ServerModTime)
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "Warning: nonmutating operation %d read stale data!", 
OBSOLETE 		     op);
OBSOLETE 	else if (log_time.tv_usec <= fce_fetched.status.ServerModTime && 
OBSOLETE 		 (vcp->DataVersion +1) == fce_fetched.status.DataVersion)
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "Notice: file modified once after nonmutating "
OBSOLETE 		     "operation %d.",
OBSOLETE 		     op);
OBSOLETE 	else
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "Warning: nonmutating operation %d might have read "
OBSOLETE 		     "stale data!", op);
OBSOLETE     } 
OBSOLETE  
OBSOLETE     ReleaseWriteLock(&fce->lock);
OBSOLETE     cred_free(ce);
OBSOLETE     /*conn_free(conn);*/
OBSOLETE 
OBSOLETE     return error;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_remove(struct vcache *vcp, FCacheEntry *childentry, char *name)
OBSOLETE {
OBSOLETE     FCacheEntry *fce;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     int error;
OBSOLETE     VenusFid *fid, tempfid; /* Ba san: to check the deletion of file*/
OBSOLETE     Result res;
OBSOLETE     int    isupdate;
OBSOLETE 
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     AFSFetchStatus status;
OBSOLETE     AFSVolSync volsync; 
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE 
OBSOLETE     fid = &(vcp->fid); /* points to the VenusFid structure */
OBSOLETE     fid = fid_translate(fid);
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: DISCONNECTED remove on "
OBSOLETE 	     "fid.Cell=0x%x, fid.fid.  Volume= 0x%x, fid.fid.Vnode=0x%x, "
OBSOLETE 	     "fid.fid.Unique=0x%x", 
OBSOLETE 	     fid->Cell, fid->fid.Volume, fid->fid.Vnode, fid->fid.Unique);
OBSOLETE 
OBSOLETE     /* ObtainWriteLock called in fcache_find */
OBSOLETE     error = fcache_find(&fce, *fid); 
OBSOLETE     assert (error == 0);
OBSOLETE 
OBSOLETE     ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     if (connected_mode != CONNMODE_PARCONNECTED) {
OBSOLETE 	ObtainWriteLock(&childentry->lock);
OBSOLETE 	isupdate = reconnect_mut_chk(childentry, ce, 
OBSOLETE 				     childentry->status.DataVersion);
OBSOLETE 	ReleaseWriteLock(&childentry->lock);
OBSOLETE   
OBSOLETE 	if (isupdate)
OBSOLETE 	{
OBSOLETE 	    fcache_cache_handle cache_handle;
OBSOLETE 
OBSOLETE 	    arla_log(ADEBDISCONN, 
OBSOLETE 		     "reconnect_remove: can't remove because file modified!");
OBSOLETE 	    cred_free(ce);
OBSOLETE 	    adir_creat (fce, name, childentry->fid.fid);
OBSOLETE 	    childentry->flags.attrp = FALSE;
OBSOLETE 	    ReleaseWriteLock(&fce->lock);
OBSOLETE 	    conv_dir (fce, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 	    ReleaseWriteLock(&fce->lock);
OBSOLETE 	    return -1;
OBSOLETE 	}
OBSOLETE     }  /* Ba ershi: we dont need to do it in parconn */
OBSOLETE 
OBSOLETE     res.res = 0;
OBSOLETE 
OBSOLETE     AssertExclLocked(&fce->lock);
OBSOLETE 
OBSOLETE     conn = find_first_fs (fce, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed", fce->index);
OBSOLETE 	cred_free(ce);
OBSOLETE 	ReleaseWriteLock(&fce->lock);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE /* Ba san: check the file exists 
OBSOLETE 
OBSOLETE    ReleaseWriteLock(&fce->lock);
OBSOLETE    error = adir_lookup(fce->fid, name, &tempfid, NULL, &ce);
OBSOLETE    assert (error == 0);                                             
OBSOLETE    ObtainWriteLock(&fce->lock);  */
OBSOLETE      
OBSOLETE     res.res = RXAFS_RemoveFile (conn->connection,
OBSOLETE 				&fce->fid.fid,
OBSOLETE 				name,
OBSOLETE 				&status,
OBSOLETE 				&volsync);
OBSOLETE     if (res.res) {
OBSOLETE 	arla_log (ADEBDISCONN, "Could not RemoveFile: %s (%d)",
OBSOLETE 		  koerr_gettext(res.res), res.res);
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     arla_warnx (ADEBDISCONN,"In reconnect_remove: Remove the file %s",name);
OBSOLETE 
OBSOLETE /* Ba san: Chcek the deletion of the file */
OBSOLETE     ReleaseWriteLock(&fce->lock);
OBSOLETE     error = adir_lookup(&fce->fid, name, &tempfid, NULL, &ce);
OBSOLETE     ObtainWriteLock(&fce->lock);
OBSOLETE 
OBSOLETE     if (error == 0) {
OBSOLETE 	int result;
OBSOLETE         
OBSOLETE 	arla_warnx (ADEBDISCONN,
OBSOLETE 		    "In reconnect_remove: file %s needs to be deleted",
OBSOLETE 		    name);
OBSOLETE 	result = adir_remove (fce,name);
OBSOLETE 	assert ( result == 0);
OBSOLETE     } /* This is for the file produced during disconnect mode,
OBSOLETE 	 if error==ENOENT then the file is created during connect mode*/
OBSOLETE   
OBSOLETE 
OBSOLETE     fce->status  = status;
OBSOLETE     fce->volsync = volsync;
OBSOLETE     childentry->host = 0;  /* Ba shiliu dont get callback */
OBSOLETE 
OBSOLETE     volcache_update_volsync (fce->volume, fce->volsync);
OBSOLETE     conv_dir (fce, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     cred_free(ce);
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     ReleaseWriteLock(&fce->lock); 
OBSOLETE     return error;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int 
OBSOLETE reconnect_rmdir(struct vcache *vcp, FCacheEntry *childEntry, char *name)
OBSOLETE {
OBSOLETE     FCacheEntry *fce;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     int error;               
OBSOLETE     VenusFid *fid, tempfid; /* Ba san: to check the deletion of file*/
OBSOLETE     Result res;
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE     int ret = 0; 
OBSOLETE     Result tempres; 
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE   
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     AFSFetchStatus status;
OBSOLETE     AFSVolSync volsync;
OBSOLETE 
OBSOLETE     fid = &(vcp->fid); /* points to the VenusFid structure */
OBSOLETE     fid = fid_translate(fid);
OBSOLETE 
OBSOLETE     ret = fcache_find(&fce, *fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     AssertExclLocked(&fce->lock);
OBSOLETE 
OBSOLETE     conn = find_first_fs (fce, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed", fce->index);
OBSOLETE 	cred_free(ce);
OBSOLETE 	ReleaseWriteLock(&fce->lock);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     ret = RXAFS_RemoveDir (conn->connection,
OBSOLETE 			   &fce->fid.fid,
OBSOLETE 			   name,
OBSOLETE 			   &status,
OBSOLETE 			   &volsync);
OBSOLETE     if (ret) {
OBSOLETE 	arla_log (ADEBDISCONN, 
OBSOLETE 		  "Could not RemoveDir : %s (%d)", 
OBSOLETE 		  koerr_gettext(res.res),res.res);
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE /* Ba san: Chcek the deletion of the file */
OBSOLETE     ReleaseWriteLock(&fce->lock);
OBSOLETE     error = adir_lookup(&fce->fid, name, &tempfid, NULL, &ce);
OBSOLETE     ObtainWriteLock(&fce->lock);
OBSOLETE 
OBSOLETE     if (error == 0) {
OBSOLETE         int result;
OBSOLETE 
OBSOLETE         arla_warnx (ADEBDISCONN,
OBSOLETE 		    "In reconnect_rmdir: file %s needs to be deleted",name);
OBSOLETE         result = adir_remove (fce,name);
OBSOLETE         assert ( result == 0);
OBSOLETE     } /* This is for the file produced during disconnect mode,
OBSOLETE 	 if error==ENOENT then the file is created during connect mode*/
OBSOLETE 
OBSOLETE     fce->status  = status;
OBSOLETE     fce->volsync = volsync;
OBSOLETE 
OBSOLETE     volcache_update_volsync (fce->volume, fce->volsync);
OBSOLETE 
OBSOLETE     tempres = conv_dir(fce, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE     childEntry->host = 0; /*Ba shiqi: no callback for this entry*/
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     cred_free(ce);
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     ReleaseWriteLock(&fce->lock);
OBSOLETE     return error;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE reconnect_mut_chk(FCacheEntry *fce, CredCacheEntry *ce, int version)
OBSOLETE {
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     FCacheEntry fetched = *fce;
OBSOLETE     int ret;
OBSOLETE 
OBSOLETE     AFSFetchStatus status;                                                   
OBSOLETE     AFSCallBack callback;                                                    
OBSOLETE     AFSVolSync volsync;      
OBSOLETE 
OBSOLETE     AssertExclLocked(&fetched.lock);
OBSOLETE 
OBSOLETE /*SWW Aug 01: >= is changed into > */
OBSOLETE     conn = find_first_fs (&fetched, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed", fce->index);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE  
OBSOLETE     ret = RXAFS_FetchStatus (conn->connection,
OBSOLETE                              &fce->fid.fid,
OBSOLETE                              &status,
OBSOLETE                              &callback,
OBSOLETE                              &volsync);
OBSOLETE     if (ret) {
OBSOLETE         if (ret == -1)
OBSOLETE             ret = ENETDOWN;
OBSOLETE         free_fs_server_context(&context);
OBSOLETE         arla_warn (ADEBFCACHE, ret, "fetch-status");
OBSOLETE         return ret;
OBSOLETE     }
OBSOLETE 
OBSOLETE     if (status.DataVersion > version)
OBSOLETE     {
OBSOLETE 	arla_log(ADEBDISCONN, "reconnect_mut_chk: concurrent writes detected!");
OBSOLETE 	return 1;
OBSOLETE     }
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void
OBSOLETE fcache_backfile_name(char *name, size_t len)
OBSOLETE {
OBSOLETE     static int no = 1;
OBSOLETE 
OBSOLETE     snprintf (name, len, "%04X",no++);
OBSOLETE     strcat (name, "bak");
OBSOLETE     name[strlen(name)+1] = '\0';
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void
OBSOLETE copy_cached_file(int from, int to)
OBSOLETE {
OBSOLETE     char name_from[2 * sizeof(int) + 1], name_to[2 * sizeof(int) + 1];
OBSOLETE     int fd_from, n, fd_to;
OBSOLETE     char buf[BUFSIZE];
OBSOLETE 
OBSOLETE     snprintf (name_from, sizeof(name_from), "%04X", from);
OBSOLETE     snprintf (name_to,   sizeof(name_to),   "%04X", to);
OBSOLETE 
OBSOLETE     fd_from = open(name_from,O_RDONLY | O_BINARY, 0);  
OBSOLETE     fd_to   = open(name_to,  O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, 0600);
OBSOLETE 
OBSOLETE     while((n = read(fd_from, buf, BUFSIZE)) > 0)
OBSOLETE 	write(fd_to, buf, n);
OBSOLETE 
OBSOLETE #if 0
OBSOLETE     if(fstat(fd_to, &statinfo)<0) {
OBSOLETE 	arla_warnx(ADEBDISCONN,"ERROR");
OBSOLETE     }
OBSOLETE #endif   
OBSOLETE 
OBSOLETE     close(fd_from);
OBSOLETE     close(fd_to);
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static void
OBSOLETE reconnect_update_fid (FCacheEntry *entry, VenusFid oldfid)
OBSOLETE {
OBSOLETE     if (entry->flags.kernelp)
OBSOLETE 	update_fid (oldfid, NULL, entry->fid, entry);
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int
OBSOLETE reconnect_mut_newfile(FCacheEntry **fcep, nnpfs_pag_t cred,VenusFid *new_fid)
OBSOLETE {
OBSOLETE 
OBSOLETE     FCacheEntry *parent_fce;
OBSOLETE     u_long host;
OBSOLETE     char name[2 * sizeof(int) + 3 + 1], tmp[2 * sizeof(int) + 2];
OBSOLETE     AFSStoreStatus store_attr;
OBSOLETE     AFSFetchStatus fetch_attr;
OBSOLETE     CredCacheEntry *ce; 
OBSOLETE     AccessEntry *ae;
OBSOLETE     VenusFid newfid;
OBSOLETE     int ret;
OBSOLETE     int from, to;
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE 
OBSOLETE     ret = fcache_find (&parent_fce, (*fcep)->parent);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     host = (*fcep)->host;
OBSOLETE 
OBSOLETE     ce = cred_get((*fcep)->parent.Cell, cred, CRED_ANY);
OBSOLETE 
OBSOLETE     fcache_backfile_name (name, sizeof(name));
OBSOLETE 
OBSOLETE     store_attr.Mask = 8;
OBSOLETE     store_attr.ClientModTime = 430144;
OBSOLETE     store_attr.Owner = 1957724;
OBSOLETE     store_attr.Group = 21516;
OBSOLETE     store_attr.UnixModeBits = 420;
OBSOLETE     store_attr.SegSize = 0;
OBSOLETE 
OBSOLETE     create_file(parent_fce, name, &store_attr, &newfid, &fetch_attr, ce);
OBSOLETE 
OBSOLETE     (*fcep)->flags.datap = FALSE; /* Ba shiqi: we need to get the old from FS*/
OBSOLETE     *new_fid = newfid;
OBSOLETE     from = (*fcep)->index;
OBSOLETE     ret = fcache_find(fcep, newfid);
OBSOLETE     assert (ret == 0);
OBSOLETE     to   = (*fcep)->index;
OBSOLETE     (*fcep)->host = host;
OBSOLETE     (*fcep)->flags.attrp = TRUE;
OBSOLETE     (*fcep)->flags.datap = TRUE;
OBSOLETE     findaccess(ce->cred, (*fcep)->acccache, &ae); /*Ba shijiu obtain access */
OBSOLETE     ae->cred   = ce->cred;
OBSOLETE     ae->access = (*fcep)->status.CallerAccess;
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&(*fcep)->lock);
OBSOLETE 
OBSOLETE     copy_cached_file(from, to);
OBSOLETE     ret = adir_creat (parent_fce, name, newfid.fid);
OBSOLETE     assert (ret ==0);
OBSOLETE     conv_dir (parent_fce, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE     ReleaseWriteLock(&parent_fce->lock);
OBSOLETE 
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE    
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_putattr(struct vcache *vcp, struct nnpfs_attr *xap)
OBSOLETE {
OBSOLETE 
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     struct rx_call *call;
OBSOLETE     VenusFid *fid;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     FCacheEntry *fce, *tempce;
OBSOLETE     AFSFetchStatus status;
OBSOLETE     Result res;
OBSOLETE     uint32_t sizefs;
OBSOLETE     AFSStoreStatus storestatus;
OBSOLETE     AFSVolSync volsync;
OBSOLETE     int ret;
OBSOLETE 
OBSOLETE     fid = &(vcp->fid); /* points to the VenusFid structure */
OBSOLETE     fid = fid_translate(fid);
OBSOLETE 
OBSOLETE #if 0
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: DISCONNECTED write on fid.Cell=0x%x, "
OBSOLETE 	     "fid.fid.Volume= 0x%x, fid.fid.Vnode=0x%x, fid.fid.Unique=0x%x",
OBSOLETE 	     fid->Cell, 
OBSOLETE 	     fid->fid.Volume, 
OBSOLETE 	     fid->fid.Vnode, 
OBSOLETE 	     fid->fid.Unique);
OBSOLETE #endif
OBSOLETE 
OBSOLETE     ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE     res.res = 0;
OBSOLETE 
OBSOLETE #if 0
OBSOLETE /* Ba shier: should we send the file back to server?  */
OBSOLETE     if (XA_VALID_SIZE(xap)){
OBSOLETE 	AFSStoreStatus storestatus;
OBSOLETE 	memset(&storestatus, 0, sizeof(storestatus));
OBSOLETE 	storestatus.Mask = 0;
OBSOLETE 
OBSOLETE 	res = cm_ftruncate (*fid, xap->xa_size, &storestatus, ce);
OBSOLETE     }  
OBSOLETE #endif
OBSOLETE 
OBSOLETE     ret = fcache_find(&fce, *fid);
OBSOLETE     assert (ret == 0);
OBSOLETE     tempce = fce;
OBSOLETE 
OBSOLETE     sizefs=fce->status.Length;
OBSOLETE 
OBSOLETE #if 0 /* XXX */
OBSOLETE     /* some people have written to the file while we are disconnected */
OBSOLETE     /* we have to give it a different name on the server  */
OBSOLETE     if (reconnect_mut_chk(fce, ce, vcp->DataVersion))
OBSOLETE     {
OBSOLETE 	VenusFid new_fid;
OBSOLETE 
OBSOLETE 	alloc_fid_trans(fid);
OBSOLETE 	reconnect_mut_newfile(&fce,vcp->cred.pag,&new_fid);  
OBSOLETE 	fce->status.Length = sizefs;
OBSOLETE 	fce->length = sizefs;
OBSOLETE 	ReleaseWriteLock(&tempce->lock);
OBSOLETE 	fill_fid_trans(&new_fid);
OBSOLETE 	tempce->flags.attrp = FALSE;
OBSOLETE 	tempce->flags.kernelp = FALSE;
OBSOLETE     }   
OBSOLETE #endif
OBSOLETE 
OBSOLETE     /* code from truncate file XXX join */
OBSOLETE     conn = find_first_fs (fce, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed.");
OBSOLETE 	ReleaseWriteLock(&fce->lock);
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     if (fce->status.FileType != TYPE_DIR) {
OBSOLETE 
OBSOLETE 	call = rx_NewCall (conn->connection);
OBSOLETE 	if (call == NULL) {
OBSOLETE 	    arla_log (ADEBDISCONN, "Cannot call");
OBSOLETE 	    res.res = ENOMEM;
OBSOLETE 	    goto out;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	storestatus.Mask = 0;
OBSOLETE 	res.res = StartRXAFS_StoreData (call,
OBSOLETE 					&(fce->fid.fid),
OBSOLETE 					&storestatus,
OBSOLETE 					0, 0, fce->status.Length);
OBSOLETE 	if(res.res) {
OBSOLETE 	    arla_log (ADEBDISCONN, "Could not start store, %s (%d)",
OBSOLETE 		      koerr_gettext(res.res), res.res);
OBSOLETE 	    rx_EndCall(call, 0);
OBSOLETE 	    goto out;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	sizefs = htonl (sizefs);
OBSOLETE 	if (rx_Write (call, &sizefs, sizeof(sizefs)) != sizeof(sizefs)) {
OBSOLETE 	    res.res = conv_to_arla_errno(rx_GetCallError(call));
OBSOLETE 	    arla_log (ADEBDISCONN, "Error writing length: %d", res.res);
OBSOLETE 	    rx_EndCall(call, 0);
OBSOLETE 	    goto out;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	if (rx_Write (call, 0, 0) != 0) {
OBSOLETE 	    res.res = conv_to_arla_errno(rx_GetCallError(call));
OBSOLETE 	    arla_log (ADEBDISCONN, "Error writing: %d", res.res);
OBSOLETE 	    rx_EndCall(call, 0);
OBSOLETE 	    goto out;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	res.res = rx_EndCall (call, EndRXAFS_StoreData (call,
OBSOLETE 							&status,
OBSOLETE 							&volsync));
OBSOLETE 	if (res.res) {
OBSOLETE 	    arla_log (ADEBDISCONN, "Error rx_EndCall: %s (%d)",
OBSOLETE 		      koerr_gettext(res.res), res.res);
OBSOLETE 	    goto out;
OBSOLETE 	}
OBSOLETE 
OBSOLETE 	fce->status   = status;
OBSOLETE 	fce->volsync  = volsync;
OBSOLETE 
OBSOLETE 	volcache_update_volsync (fce->volume, fce->volsync);
OBSOLETE 
OBSOLETE     }
OBSOLETE     /* code from write_attr XXX join */
OBSOLETE     nnpfs_attr2afsstorestatus(xap, &storestatus);
OBSOLETE 
OBSOLETE     res.res = RXAFS_StoreStatus (conn->connection,
OBSOLETE 				 &fce->fid.fid,
OBSOLETE 				 &storestatus,
OBSOLETE 				 &status,
OBSOLETE 				 &volsync);
OBSOLETE     if (res.res) {
OBSOLETE         arla_log (ADEBDISCONN, "Could not make store-status call, %s (%d)",
OBSOLETE 		  koerr_gettext(res.res), res.res);
OBSOLETE         goto out;
OBSOLETE     }
OBSOLETE     arla_log(ADEBDISCONN, 
OBSOLETE 	     "write_attr: status.Length = %d", status.Length);
OBSOLETE     fce->status  = status;
OBSOLETE     fce->volsync = volsync;
OBSOLETE 
OBSOLETE     volcache_update_volsync (fce->volume, fce->volsync);
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     ReleaseWriteLock(&fce->lock);
OBSOLETE     cred_free(ce);
OBSOLETE     return res.res;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE static int 
OBSOLETE reconnect_putdata(struct vcache *vcp)
OBSOLETE {
OBSOLETE     VenusFid *fid;
OBSOLETE     FCacheEntry *fce;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     Result res;
OBSOLETE 
OBSOLETE     uint32_t sizefs;
OBSOLETE     int fd = -1;
OBSOLETE     struct rx_call *call;
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     struct stat statinfo;
OBSOLETE     AFSStoreStatus storestatus;
OBSOLETE     AFSFetchStatus status;
OBSOLETE     AFSVolSync volsync;
OBSOLETE     int ret;
OBSOLETE 
OBSOLETE     fid = &(vcp->fid); /* points to the VenusFid structure */
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: putdata before fid_translate, "
OBSOLETE 	     "fid->Cell=0x%x, fid->fid.Volume=0x%x, fid->fid.Vnode=0x%x, "
OBSOLETE 	     "fid->fid.Unique=0x%x", 
OBSOLETE 	     fid->Cell, 
OBSOLETE 	     fid->fid.Volume, 
OBSOLETE 	     fid->fid.Vnode, 
OBSOLETE 	     fid->fid.Unique);
OBSOLETE 
OBSOLETE     fid = fid_translate(fid);
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: putdata after fid_translate, "
OBSOLETE 	     "fid->Cell=0x%x, fid->fid.Volume=0x%x, fid->fid.Vnode=0x%x, "
OBSOLETE 	     "fid->fid.Unique=0x%x", 
OBSOLETE 	     fid->Cell, 
OBSOLETE 	     fid->fid.Volume, 
OBSOLETE 	     fid->fid.Vnode, 
OBSOLETE 	     fid->fid.Unique);
OBSOLETE 
OBSOLETE 
OBSOLETE     ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     ret = fcache_find (&fce, *fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE #if 0
OBSOLETE     isupdate = reconnect_mut_chk(fce, ce, vcp->DataVersion);
OBSOLETE     if (isupdate)
OBSOLETE     {
OBSOLETE 	arla_log(ADEBDISCONN, 
OBSOLETE 		 "reconnect_putdata: send data back because "
OBSOLETE 		 "the file was modified!");
OBSOLETE 	cred_free(ce);
OBSOLETE 	ReleaseWriteLock(&fce->lock);
OBSOLETE 	reconnect_mut_newfile(&fce, vcp->cred.pag);  
OBSOLETE 	return -1;
OBSOLETE     }
OBSOLETE 
OBSOLETE     if (reconnect_mut_chk (fce, ce)) {
OBSOLETE 	arla_log (ADEBDISCONN, "Reconnect_putdata: can not send the file"
OBSOLETE 		  "to FS becausethis file has been modified!");
OBSOLETE 	ReleaseWriteLock(&fce->lock);
OBSOLETE 	return -1;
OBSOLETE     } 
OBSOLETE #endif
OBSOLETE 
OBSOLETE     /* code taken from write_data XXX join */ 
OBSOLETE     AssertExclLocked(&fce->lock);
OBSOLETE 
OBSOLETE     conn = find_first_fs (fce, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed");
OBSOLETE 	ReleaseWriteLock(&fce->lock); 
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     fd = fcache_open_file (fce, O_RDONLY);
OBSOLETE     if (fd < 0) {
OBSOLETE 	arla_log (ADEBDISCONN, "open %u failed", fce->index);
OBSOLETE 	res.res = errno;
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     if (fstat (fd, &statinfo) < 0) {
OBSOLETE 	arla_log (ADEBDISCONN, "Cannot stat file %u", fce->index);
OBSOLETE 	res.res = errno;
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     sizefs = statinfo.st_size;
OBSOLETE 
OBSOLETE     call = rx_NewCall (conn->connection);
OBSOLETE     if (call == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "Cannot call");
OBSOLETE 	res.res = ENOMEM;
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     storestatus.Mask = 0; /* Dont save anything */
OBSOLETE     res.res = StartRXAFS_StoreData (call, &fce->fid.fid,
OBSOLETE 				    &storestatus,
OBSOLETE 				    0,
OBSOLETE 				    sizefs,
OBSOLETE 				    sizefs);
OBSOLETE     if (res.res) {
OBSOLETE 	arla_log (ADEBDISCONN, "Could not start store, %s (%d)",
OBSOLETE 		  koerr_gettext(res.res), res.res);
OBSOLETE 	rx_EndCall(call, 0);
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     res.res = copyfd2rx (fd, call, 0, sizefs);
OBSOLETE     if (res.res) {
OBSOLETE 	rx_EndCall(call, res.res);
OBSOLETE 	arla_log (ADEBDISCONN, "copyfd2rx failed: %d", res.res);
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE      
OBSOLETE     res.res = rx_EndCall (call, EndRXAFS_StoreData (call,
OBSOLETE 						    &status,
OBSOLETE 						    &volsync));
OBSOLETE     if (res.res) {
OBSOLETE 	arla_log (ADEBDISCONN, "Error rx_EndCall: %s (%d)", 
OBSOLETE 		  koerr_gettext(res.res), res.res);
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE     if (status.DataVersion > fce->status.DataVersion)
OBSOLETE 	arla_log(ADEBDISCONN, 
OBSOLETE 		 "reconnect: putdata, server incremented DataVersion!");
OBSOLETE 
OBSOLETE     fce->status   = status;
OBSOLETE     fce->volsync  = volsync;
OBSOLETE 
OBSOLETE     volcache_update_volsync (fce->volume, fce->volsync);
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&fce->lock); 
OBSOLETE     if (fd != -1)
OBSOLETE 	close (fd);
OBSOLETE     free_fs_server_context (&context);
OBSOLETE   
OBSOLETE     cred_free(ce);
OBSOLETE     return res.res;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_rename(struct vcache *vcp_old, struct vcache *vcp_new, 
OBSOLETE 		     char *name_old, char *name_new)
OBSOLETE {
OBSOLETE 
OBSOLETE     FCacheEntry *fce_old, *fce_new;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     VenusFid *fid_old, *fid_new,foo_fid,*tempnew_fid;
OBSOLETE     int error;
OBSOLETE 
OBSOLETE     int ret = 0;
OBSOLETE     Result res;
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE     int isnewpar = 0;
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     AFSFetchStatus orig_status, new_status;
OBSOLETE     AFSVolSync volsync;
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE 
OBSOLETE     fid_old = &vcp_old->fid;
OBSOLETE     fid_old = fid_translate(fid_old);
OBSOLETE  
OBSOLETE     ret = fcache_find (&fce_old, *fid_old);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     /* ReleaseWriteLock(&fce_old->lock);  SWW Qiyue 28 Maybe we dont need it*/
OBSOLETE     assert(fce_old);
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: old rename on Cell=0x%x, "
OBSOLETE 	     "fid.Volume= 0x%x, fid.Vnode=0x%x, fid.Unique=0x%x", 
OBSOLETE 	     fce_old->fid.Cell,
OBSOLETE 	     fce_old->fid.fid.Volume,
OBSOLETE 	     fce_old->fid.fid.Vnode, 
OBSOLETE 	     fce_old->fid.fid.Unique);
OBSOLETE 
OBSOLETE     fid_new = tempnew_fid = &vcp_new->fid;
OBSOLETE     fid_new = fid_translate(fid_new);
OBSOLETE     
OBSOLETE     if (tempnew_fid->fid.Volume != fid_new->fid.Volume ||
OBSOLETE 	tempnew_fid->fid.Vnode != fid_new->fid.Vnode ||
OBSOLETE 	tempnew_fid->fid.Unique != fid_new->fid.Unique)
OBSOLETE         isnewpar = 1; 
OBSOLETE 
OBSOLETE /*Ba ba: the parent dir was created during disconnected */
OBSOLETE 
OBSOLETE     if (fid_old->fid.Volume == fid_new->fid.Volume &&
OBSOLETE 	fid_old->fid.Vnode == fid_new->fid.Vnode   &&
OBSOLETE 	fid_old->fid.Unique == fid_new->fid.Unique )
OBSOLETE         ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/
OBSOLETE 
OBSOLETE     ret = fcache_find (&fce_new, *fid_new);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: new rename on Cell=0x%x, "
OBSOLETE 	     "fid.Volume= 0x%x, fid.Vnode=0x%x, fid.Unique=0x%x", 
OBSOLETE 	     fce_new->fid.Cell, 
OBSOLETE 	     fce_new->fid.fid.Volume, 
OBSOLETE 	     fce_new->fid.fid.Vnode, 
OBSOLETE 	     fce_new->fid.fid.Unique);
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, 
OBSOLETE 	     "reconnect_rename: fce_old = 0x%x, fce_new = 0x%x",
OBSOLETE 	     fce_old, fce_new);
OBSOLETE 
OBSOLETE     ce = cred_get (vcp_old->fid.Cell, vcp_old->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     AssertExclLocked(&fce_old->lock);
OBSOLETE     AssertExclLocked(&fce_new->lock);
OBSOLETE 
OBSOLETE     conn = find_first_fs (fce_old, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed");
OBSOLETE 	ReleaseWriteLock(&fce_new->lock);
OBSOLETE 	
OBSOLETE 	if (fid_old->fid.Volume != fid_new->fid.Volume ||
OBSOLETE 	    fid_old->fid.Vnode != fid_new->fid.Vnode   ||
OBSOLETE 	    fid_old->fid.Unique != fid_new->fid.Unique )
OBSOLETE 	    ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/
OBSOLETE 	
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     error = RXAFS_Rename (conn->connection,
OBSOLETE 			  &fce_old->fid.fid,
OBSOLETE 			  name_old,
OBSOLETE 			  &fce_new->fid.fid,
OBSOLETE 			  name_new,
OBSOLETE 			  &orig_status,
OBSOLETE 			  &new_status,
OBSOLETE 			  &volsync);
OBSOLETE 
OBSOLETE     if (error) {
OBSOLETE 	arla_log (ADEBDISCONN, "Could not Rename: %s (%d)", koerr_gettext(error), error); 
OBSOLETE 	goto out; }
OBSOLETE 
OBSOLETE     fce_old->status = orig_status;
OBSOLETE     fce_new->status = new_status;
OBSOLETE 
OBSOLETE     fce_old->volsync = fce_new->volsync = volsync;
OBSOLETE 
OBSOLETE     volcache_update_volsync (fce_old->volume, fce_old->volsync);
OBSOLETE 
OBSOLETE 
OBSOLETE /*SWW Aug 01 */
OBSOLETE     arla_warnx (ADEBDISCONN,
OBSOLETE 		"reconnect_rename: we delete the old one %s volumn=0x%x, "
OBSOLETE 		"vnode=0x%x,unique=0x%x",
OBSOLETE 		name_old,fce_old->fid.fid.Volume,
OBSOLETE 		fce_old->fid.fid.Vnode,
OBSOLETE 		fce_old->fid.fid.Unique);                      
OBSOLETE 
OBSOLETE /*Ba Yi: get the VenuseFid for new file */
OBSOLETE   #if 0
OBSOLETE     if (fid_old->fid.Volume == fid_new->fid.Volume &&
OBSOLETE 	fid_old->fid.Vnode == fid_new->fid.Vnode   &&
OBSOLETE 	fid_old->fid.Unique == fid_new->fid.Unique ) ;
OBSOLETE #endif
OBSOLETE     ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/
OBSOLETE     
OBSOLETE     error = adir_lookup (&fce_old->fid, name_old, &foo_fid, NULL, &ce);
OBSOLETE     
OBSOLETE #if 0
OBSOLETE     if (fid_old->fid.Volume == fid_new->fid.Volume &&
OBSOLETE        fid_old->fid.Vnode == fid_new->fid.Vnode   &&
OBSOLETE        fid_old->fid.Unique == fid_new->fid.Unique );
OBSOLETE #endif
OBSOLETE     ObtainWriteLock (&fce_old->lock);
OBSOLETE 
OBSOLETE /*Ba San: delete the old which was created during dis */
OBSOLETE     if (error == 0) {
OBSOLETE 	arla_warnx (ADEBDISCONN,"reconnect_rename: we delete the old one %s "
OBSOLETE 		    "volumn=0x%x,vnode=0x%x,unique=0x%x",
OBSOLETE 		    name_old,
OBSOLETE 		    foo_fid.fid.Volume,
OBSOLETE 		    foo_fid.fid.Vnode,
OBSOLETE 		    foo_fid.fid.Unique);
OBSOLETE 
OBSOLETE 	adir_remove(fce_old,name_old);
OBSOLETE 	adir_remove(fce_new,name_new);
OBSOLETE 
OBSOLETE 	res = conv_dir (fce_old, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE     } else {
OBSOLETE 	/* if found delete it */
OBSOLETE /*Ba San: try to find the previous VenuseFid for old name */
OBSOLETE 	if (error == ENOENT) {
OBSOLETE #if 0
OBSOLETE 	    if (fid_old->fid.Volume == fid_new->fid.Volume &&
OBSOLETE 		fid_old->fid.Vnode == fid_new->fid.Vnode   &&
OBSOLETE 		fid_old->fid.Unique == fid_new->fid.Unique );
OBSOLETE #endif
OBSOLETE 	    ReleaseWriteLock(&fce_new->lock);
OBSOLETE 	    
OBSOLETE 	    error = adir_lookup (&fce_new->fid, name_new, &foo_fid, NULL, &ce);
OBSOLETE 	    
OBSOLETE #if 0
OBSOLETE 	    if (fid_old->fid.Volume == fid_new->fid.Volume &&
OBSOLETE                fid_old->fid.Vnode == fid_new->fid.Vnode   &&
OBSOLETE                fid_old->fid.Unique == fid_new->fid.Unique );
OBSOLETE #endif
OBSOLETE 	    ObtainWriteLock (&fce_new->lock);
OBSOLETE 	    if (error == 0) /*Ba Si: We need delete the faked new */
OBSOLETE 		adir_remove(fce_new,name_new);
OBSOLETE 	    else if (error == ENOENT) {
OBSOLETE                 int venusret;
OBSOLETE 		
OBSOLETE                 venusret = find_venus (name_new,&foo_fid);
OBSOLETE                 assert (venusret==0);
OBSOLETE                 arla_warnx (ADEBDISCONN,"I MUST WRITE A PROGRAM HERE");
OBSOLETE                 if (isnewpar == 1) {
OBSOLETE 
OBSOLETE 		    arla_warnx(ADEBDISCONN,"In reconnect_rename: "
OBSOLETE 			       "new Volume=0x%x,Vnode=0x%x,Unique=0x%x",
OBSOLETE 			       fce_new->fid.fid.Volume,
OBSOLETE 			       fce_new->fid.fid.Vnode,
OBSOLETE 			       fce_new->fid.fid.Unique);
OBSOLETE #if 0
OBSOLETE 		    error = adir_creat(fce_new, name_new, foo_fid.fid);
OBSOLETE #endif
OBSOLETE 		}
OBSOLETE 	    }
OBSOLETE 	}
OBSOLETE     }
OBSOLETE 
OBSOLETE     arla_warnx (ADEBDISCONN,"reconnect_rename: we add the new one %s "
OBSOLETE 		"volumn=0x%x,vnode=0x%x,unique=0x%x",
OBSOLETE 		name_new,
OBSOLETE 		foo_fid.fid.Volume,
OBSOLETE 		foo_fid.fid.Vnode,
OBSOLETE 		foo_fid.fid.Unique);
OBSOLETE     error = adir_creat (fce_new, name_new, foo_fid.fid);
OBSOLETE     res = conv_dir (fce_new, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE /* Aug 1 */
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     free_fs_server_context (&context);
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&fce_new->lock);
OBSOLETE 
OBSOLETE     if (fid_old->fid.Volume != fid_new->fid.Volume ||
OBSOLETE 	fid_old->fid.Vnode != fid_new->fid.Vnode   ||
OBSOLETE 	fid_old->fid.Unique != fid_new->fid.Unique )
OBSOLETE         ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/
OBSOLETE 
OBSOLETE     cred_free(ce);
OBSOLETE     return error;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_create(struct vcache *parent, struct vcache *child, char *name)
OBSOLETE {
OBSOLETE 
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     VenusFid *parent_fid;
OBSOLETE     VenusFid *child_fid;
OBSOLETE     VenusFid fakeFid; 
OBSOLETE 
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     FCacheEntry *parentEntry;
OBSOLETE     FCacheEntry *childEntry;
OBSOLETE 
OBSOLETE     AFSFetchStatus fetch_attr;
OBSOLETE     AFSStoreStatus store_attr; 
OBSOLETE 
OBSOLETE     AFSFetchStatus status;
OBSOLETE     AFSCallBack callback;
OBSOLETE     AFSVolSync volsync;
OBSOLETE     int ret; 
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE     int32_t type;
OBSOLETE 
OBSOLETE     parent_fid = &(parent->fid); /* points to the VenusFid structure */
OBSOLETE     child_fid = &(child->fid);
OBSOLETE     fakeFid = *child_fid;
OBSOLETE 
OBSOLETE     /*Ba Liu: the parent dir may be created during dison mode*/
OBSOLETE     parent_fid = fid_translate(parent_fid);
OBSOLETE 
OBSOLETE     ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     ret = fcache_find (&parentEntry, *parent_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE #if 0
OBSOLETE     is_change = reconnect_mut_chk(parentEntry, 
OBSOLETE 				  ce, 
OBSOLETE 				  parentEntry->status.DataVersion);
OBSOLETE #endif
OBSOLETE 
OBSOLETE /*SWW Qiyue 30 delete the old file entry in dir  */
OBSOLETE     arla_warnx (ADEBDISCONN,
OBSOLETE 		"reconnect_rename: we delete the old one volumn=0x%x, "
OBSOLETE 		"vnode=0x%x,unique=0x%x",
OBSOLETE 		parentEntry->fid.fid.Volume,
OBSOLETE 		parentEntry->fid.fid.Vnode,
OBSOLETE 		parentEntry->fid.fid.Unique);
OBSOLETE 
OBSOLETE     adir_remove(parentEntry,name);  
OBSOLETE 
OBSOLETE     conn = find_first_fs (parentEntry, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN, "find_first_fs failed");
OBSOLETE 	ReleaseWriteLock(&parentEntry->lock);
OBSOLETE 	ReleaseWriteLock(&childEntry->lock);
OBSOLETE 	free_fs_server_context(&context);
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     ret = fcache_find (&childEntry, *child_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     recon_hashtabdel(childEntry);
OBSOLETE 
OBSOLETE #if 0
OBSOLETE     fetch_attr = &childEntry->status;
OBSOLETE #endif
OBSOLETE 
OBSOLETE     store_attr.Mask 	   =    8;
OBSOLETE     store_attr.ClientModTime =    childEntry->status.ClientModTime;
OBSOLETE     store_attr.Owner = 	 	childEntry->status.Owner;
OBSOLETE     store_attr.Group = 		childEntry->status.Group;
OBSOLETE     store_attr.UnixModeBits = 	childEntry->status.UnixModeBits;
OBSOLETE     store_attr.SegSize = 		childEntry->status.SegSize;
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, 
OBSOLETE 	     "reconnect: create before RXAFS_CreateFile, "
OBSOLETE 	     "Cell=0x%x, fid.Volume= 0x%x, fid.Vnode=0x%x, fid.Unique=0x%x", 
OBSOLETE 	     childEntry->fid.Cell, 
OBSOLETE 	     childEntry->fid.fid.Volume, 
OBSOLETE 	     childEntry->fid.fid.Vnode, 
OBSOLETE 	     childEntry->fid.fid.Unique);
OBSOLETE 
OBSOLETE     alloc_fid_trans(&childEntry->fid);
OBSOLETE   
OBSOLETE 
OBSOLETE     ret = RXAFS_CreateFile (conn->connection,
OBSOLETE                             &(parentEntry->fid.fid),
OBSOLETE                             name, &store_attr,
OBSOLETE                             &(childEntry->fid.fid), &fetch_attr,
OBSOLETE                             &status,
OBSOLETE                             &callback,
OBSOLETE                             &volsync);
OBSOLETE  
OBSOLETE     if (ret) {
OBSOLETE 	if (ret == 17) {
OBSOLETE 	    ReleaseWriteLock(&parentEntry->lock);
OBSOLETE 	    reconnect_mut_newfile(&childEntry, 
OBSOLETE 				  parent->cred.pag, 
OBSOLETE 				  &childEntry->fid);
OBSOLETE 	    ObtainWriteLock(&parentEntry->lock);
OBSOLETE 	    fill_fid_trans(&childEntry->fid);
OBSOLETE 	    recon_hashtabadd(childEntry);
OBSOLETE 	    childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
OBSOLETE 	    reconnect_update_fid (childEntry, fakeFid);
OBSOLETE 	} else {
OBSOLETE 	    arla_log (ADEBDISCONN, "Could not CreateFile: %s (%d)",
OBSOLETE 		      koerr_gettext(ret), ret);
OBSOLETE 	}
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE    
OBSOLETE     parentEntry->status   = status;   
OBSOLETE     parentEntry->callback = callback;
OBSOLETE     parentEntry->volsync  = volsync;
OBSOLETE 
OBSOLETE     childEntry->fid.Cell = parentEntry->fid.Cell;
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, "reconnect: create after RXAFS_CreateFile, "
OBSOLETE 	     "Cell=0x%x, fid.Volume= 0x%x, fid .Vnode=0x%x, fid.Unique=0x%x", 
OBSOLETE 	     childEntry->fid.Cell,
OBSOLETE 	     childEntry->fid.fid.Volume, 
OBSOLETE 	     childEntry->fid.fid.Vnode, 
OBSOLETE 	     childEntry->fid.fid.Unique); 
OBSOLETE 
OBSOLETE     fill_fid_trans(&childEntry->fid);
OBSOLETE 
OBSOLETE #if 0
OBSOLETE     ReleaseWriteLock(&childEntry->lock);
OBSOLETE #endif
OBSOLETE 
OBSOLETE 
OBSOLETE     ret = volcache_getbyid (childEntry->fid.fid.Volume,
OBSOLETE 			    childEntry->fid.Cell,
OBSOLETE 			    ce,
OBSOLETE 			    &childEntry->volume,
OBSOLETE 			    &type);
OBSOLETE 
OBSOLETE     recon_hashtabadd(childEntry); 
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, 
OBSOLETE 	     "reconnect: create after volcache_getbyid, Cell=0x%x, "
OBSOLETE 	     "fid.Volume= 0x%x, fid .Vnode=0x%x, fid.Unique=0x%x",
OBSOLETE 	     childEntry->fid.Cell, 
OBSOLETE 	     childEntry->fid.fid.Volume, 
OBSOLETE 	     childEntry->fid.fid.Vnode, 
OBSOLETE 	     childEntry->fid.fid.Unique); 
OBSOLETE 
OBSOLETE /* SWW Qiyue 30: add the new file entry in dir */
OBSOLETE     arla_warnx (ADEBDISCONN,"reconnect_rename: we add the new one "
OBSOLETE 		"volumn=0x%x,vnode=0x%x,unique=0x%x",
OBSOLETE 		parentEntry->fid.fid.Volume,
OBSOLETE 		parentEntry->fid.fid.Vnode,
OBSOLETE 		parentEntry->fid.fid.Unique);
OBSOLETE 
OBSOLETE     adir_creat (parentEntry, name, childEntry->fid.fid);  
OBSOLETE 
OBSOLETE     childEntry->status = fetch_attr;
OBSOLETE   
OBSOLETE     childEntry->flags.attrp = TRUE;
OBSOLETE     childEntry->flags.kernelp = TRUE;
OBSOLETE 
OBSOLETE     childEntry->flags.datap = TRUE;
OBSOLETE     childEntry->tokens |= NNPFS_ATTR_R | NNPFS_DATA_R | NNPFS_DATA_W;
OBSOLETE 
OBSOLETE     if (parentEntry->volume == NULL)
OBSOLETE 	ret = volcache_getbyid (parentEntry->fid.fid.Volume,
OBSOLETE 				parentEntry->fid.Cell,
OBSOLETE 				ce,
OBSOLETE 				&parentEntry->volume,
OBSOLETE 				&type);
OBSOLETE 
OBSOLETE     volcache_update_volsync (parentEntry->volume, parentEntry->volsync);
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE /*SWW Qiyue 28: Set the host for child entry */
OBSOLETE 
OBSOLETE     childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
OBSOLETE     assert(childEntry->host);
OBSOLETE 
OBSOLETE /*SWW Qiyue 29:  */
OBSOLETE     arla_warnx (ADEBDISCONN,
OBSOLETE 		"Replace fid.Volume=0x%x,Vnode=0x%x,Unique=0x%x with "
OBSOLETE 		"Volume=0x%x,Vnode=0x%x,Unqiue=0x%x",
OBSOLETE 		fakeFid.fid.Volume,
OBSOLETE 		fakeFid.fid.Vnode,
OBSOLETE 		fakeFid.fid.Unique,
OBSOLETE 		childEntry->fid.fid.Volume,
OBSOLETE 		childEntry->fid.fid.Vnode,
OBSOLETE 		childEntry->fid.fid.Unique);
OBSOLETE 
OBSOLETE     reconnect_update_fid (childEntry, fakeFid);
OBSOLETE 
OBSOLETE     conv_dir(parentEntry, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&childEntry->lock);
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&parentEntry->lock);
OBSOLETE     ReleaseWriteLock(&childEntry->lock);
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     cred_free(ce);
OBSOLETE     return ret;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_mkdir(struct vcache *parent, struct vcache *curdir, 
OBSOLETE                     AFSStoreStatus *store_status, char *name)
OBSOLETE {
OBSOLETE     ConnCacheEntry *conn; 
OBSOLETE     fs_server_context context;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     VenusFid *parent_fid;
OBSOLETE     VenusFid *child_fid;
OBSOLETE     VenusFid fakeFid;
OBSOLETE 
OBSOLETE     FCacheEntry *parentEntry, *childEntry, *tempEntry, *tempparEntry;
OBSOLETE     Result tempres;
OBSOLETE     int    ret = 0;
OBSOLETE     int    tempret = 0;
OBSOLETE     struct timeval tv;
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE 
OBSOLETE     AFSFid  Outfid;   /* Ba Wu: These are used to get the info from FS*/
OBSOLETE     AFSFetchStatus fetch_attr;
OBSOLETE     AFSFetchStatus status;
OBSOLETE     AFSCallBack  callback;
OBSOLETE     AFSVolSync   volsync;
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE     int32_t type;
OBSOLETE 
OBSOLETE     parent_fid = &(parent->fid); /* points to the VenusFid structure */
OBSOLETE     child_fid = &(curdir->fid);
OBSOLETE     fakeFid = *child_fid;
OBSOLETE 
OBSOLETE     parent_fid = fid_translate(parent_fid);
OBSOLETE 
OBSOLETE     ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     ret = fcache_find (&parentEntry, *parent_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     tempparEntry = parentEntry;
OBSOLETE 
OBSOLETE /*Ba ba: used to check whether name can be find  Deleted !!!
OBSOLETE   ReleaseWriteLock(&parentEntry->lock);
OBSOLETE   tempret = adir_lookup (parentEntry->fid , name , &foo_fid , NULL, ce);  */
OBSOLETE /*Ba ba: used to check whether name can be find  Deleted !!! */
OBSOLETE 
OBSOLETE     /*Ba Wu Remove the dir name from itsparent dir */
OBSOLETE     tempret = adir_remove(parentEntry,name);  
OBSOLETE     conn = find_first_fs (parentEntry, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN,"Cannot make this connection");
OBSOLETE 	ReleaseWriteLock(&parentEntry->lock);
OBSOLETE 	ReleaseWriteLock(&childEntry->lock);
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     ret = fcache_find(&childEntry, *child_fid);/*Ba Wu: remove the newly created dir */
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     recon_hashtabdel(childEntry);
OBSOLETE 
OBSOLETE     alloc_fid_trans(&childEntry->fid);
OBSOLETE 
OBSOLETE     gettimeofday(&tv, NULL);
OBSOLETE 
OBSOLETE     ret = RXAFS_MakeDir (conn->connection,
OBSOLETE 			 &parentEntry->fid.fid,
OBSOLETE 			 name,
OBSOLETE 			 store_status, 
OBSOLETE 			 &Outfid,
OBSOLETE 			 &fetch_attr,
OBSOLETE 			 &status,
OBSOLETE 			 &callback,
OBSOLETE 			 &volsync);
OBSOLETE 
OBSOLETE     if (ret) {
OBSOLETE 	arla_log (ADEBDISCONN, "Could not CreateFile: %s (%d)",
OBSOLETE 		  koerr_gettext(ret), ret);
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     parentEntry->status   = status;
OBSOLETE     parentEntry->callback = callback;
OBSOLETE     parentEntry->callback.ExpirationTime += tv.tv_sec;
OBSOLETE     parentEntry->volsync  = volsync;
OBSOLETE 
OBSOLETE     childEntry->fid.Cell = parentEntry->fid.Cell;
OBSOLETE     childEntry->fid.fid = Outfid;
OBSOLETE     childEntry->status = fetch_attr;
OBSOLETE     childEntry->flags.attrp = TRUE;
OBSOLETE     childEntry->flags.kernelp = TRUE;
OBSOLETE     childEntry->flags.datap = TRUE;
OBSOLETE     childEntry->tokens |= NNPFS_ATTR_R | NNPFS_DATA_R | NNPFS_DATA_W;
OBSOLETE 
OBSOLETE     fill_fid_trans(&childEntry->fid);
OBSOLETE 
OBSOLETE     ret = volcache_getbyid (childEntry->fid.fid.Volume,
OBSOLETE 			    childEntry->fid.Cell,
OBSOLETE 			    ce,
OBSOLETE 			    &childEntry->volume,
OBSOLETE 			    &type);
OBSOLETE 
OBSOLETE     recon_hashtabadd(childEntry);
OBSOLETE 
OBSOLETE /*Ba ba: Need to change later!!! */
OBSOLETE #if 0
OBSOLETE     ReleaseWriteLock(&tempparEntry->lock);
OBSOLETE     tempret = adir_changefid (tempparEntry->fid ,name, &Outfid,  ce);
OBSOLETE     ReleaseWriteLock(&tempparEntry->lock);
OBSOLETE     tempret = adir_lookup (tempparEntry->fid ,name, &foo_fid, NULL, ce);
OBSOLETE #endif
OBSOLETE 
OBSOLETE     tempret = adir_creat (parentEntry, name, childEntry->fid.fid); 
OBSOLETE 
OBSOLETE     childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
OBSOLETE     assert(childEntry->host);
OBSOLETE 
OBSOLETE     reconnect_update_fid(childEntry, fakeFid);
OBSOLETE      
OBSOLETE     tempres = conv_dir(parentEntry, ce, 0, &cache_handle,
OBSOLETE 		       tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&childEntry->lock);
OBSOLETE 
OBSOLETE     /*SWW Qiyue 29: This should be deleted later */                      
OBSOLETE     ret = fcache_find (&tempEntry, childEntry->fid);  
OBSOLETE 
OBSOLETE     assert (ret == 0);
OBSOLETE     ReleaseWriteLock(&tempEntry->lock);
OBSOLETE 
OBSOLETE  out:
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&parentEntry->lock);
OBSOLETE     ReleaseWriteLock(&childEntry->lock);
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     cred_free(ce);
OBSOLETE     return ret;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_link(struct vcache *parent, struct vcache *existing,
OBSOLETE 		   char *name)
OBSOLETE {
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     VenusFid *parent_fid;  
OBSOLETE     VenusFid *existing_fid;
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE     int ret = 0;
OBSOLETE     FCacheEntry *dir_entry,*existing_entry;
OBSOLETE     Result res;
OBSOLETE 
OBSOLETE     AFSFetchStatus new_status;
OBSOLETE     AFSFetchStatus status;
OBSOLETE     AFSVolSync volsync;
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE 
OBSOLETE 
OBSOLETE     parent_fid = &(parent->fid);
OBSOLETE     existing_fid = &(existing->fid);
OBSOLETE 
OBSOLETE     parent_fid = fid_translate(parent_fid);
OBSOLETE 
OBSOLETE     ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     ret = fcache_find (&dir_entry, *parent_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     ret = fcache_find (&existing_entry, *existing_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     conn = find_first_fs (dir_entry, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN,"Cannot make this connection");
OBSOLETE 	ReleaseWriteLock(&dir_entry->lock);
OBSOLETE 	ReleaseWriteLock(&existing_entry->lock);
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENETDOWN;
OBSOLETE     }
OBSOLETE 
OBSOLETE     ret = RXAFS_Link (conn->connection,
OBSOLETE 		      &dir_entry->fid.fid,
OBSOLETE 		      name,
OBSOLETE 		      &existing_entry->fid.fid,
OBSOLETE 		      &new_status,
OBSOLETE 		      &status,
OBSOLETE 		      &volsync);
OBSOLETE     if (ret) {
OBSOLETE 	arla_warn (ADEBFCACHE, ret, "Link");
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     dir_entry->status  = status;
OBSOLETE     dir_entry->volsync = volsync;
OBSOLETE 
OBSOLETE     existing_entry->status = new_status;
OBSOLETE     
OBSOLETE     volcache_update_volsync (dir_entry->volume, dir_entry->volsync);
OBSOLETE 
OBSOLETE     res = conv_dir (dir_entry, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 	
OBSOLETE  out:
OBSOLETE     ReleaseWriteLock(&dir_entry->lock);
OBSOLETE     ReleaseWriteLock(&existing_entry->lock);
OBSOLETE     cred_free(ce);
OBSOLETE     free_fs_server_context (&context);
OBSOLETE     return ret;
OBSOLETE }
OBSOLETE 
OBSOLETE /*
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE int reconnect_symlink(struct vcache *parent, struct vcache *child,
OBSOLETE 		      AFSStoreStatus *store_attr, char *name, 
OBSOLETE 		      char *contents)
OBSOLETE {
OBSOLETE     ConnCacheEntry *conn;
OBSOLETE     fs_server_context context;
OBSOLETE     CredCacheEntry *ce;
OBSOLETE     VenusFid *parent_fid, *child_fid, fakeFid;
OBSOLETE     char tmp[2 * sizeof(int) + 2];
OBSOLETE     int ret = 0;
OBSOLETE     FCacheEntry *dir_entry, *childEntry;
OBSOLETE     Result res;
OBSOLETE 
OBSOLETE     AFSFetchStatus fetch_attr, new_status;
OBSOLETE     AFSVolSync volsync;
OBSOLETE     fcache_cache_handle cache_handle;
OBSOLETE     int32_t type;
OBSOLETE 
OBSOLETE     parent_fid = &(parent->fid);
OBSOLETE     child_fid  = &(child->fid);
OBSOLETE     fakeFid    = *child_fid;
OBSOLETE     parent_fid = fid_translate(parent_fid);
OBSOLETE 
OBSOLETE     ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
OBSOLETE     assert (ce != NULL);
OBSOLETE 
OBSOLETE     ret = fcache_find (&dir_entry, *parent_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     adir_remove(dir_entry,name);
OBSOLETE 
OBSOLETE     ret = fcache_find (&childEntry, *child_fid);
OBSOLETE     assert (ret == 0);
OBSOLETE 
OBSOLETE     recon_hashtabdel(childEntry);
OBSOLETE 
OBSOLETE     assert(ret==0);
OBSOLETE     conn = find_first_fs (dir_entry, ce, &context);
OBSOLETE     if (conn == NULL) {
OBSOLETE 	arla_log (ADEBDISCONN,"Cannot make this connection");
OBSOLETE 	ReleaseWriteLock(&dir_entry->lock);
OBSOLETE 	ReleaseWriteLock(&childEntry->lock);
OBSOLETE 	cred_free(ce);
OBSOLETE 	return ENOMEM;
OBSOLETE     }
OBSOLETE   
OBSOLETE     alloc_fid_trans(&childEntry->fid);
OBSOLETE 
OBSOLETE     ret = RXAFS_Symlink (conn->connection,
OBSOLETE 			 &dir_entry->fid.fid,
OBSOLETE 			 name,
OBSOLETE 			 contents,
OBSOLETE 			 store_attr,
OBSOLETE 			 &(childEntry->fid.fid),
OBSOLETE 			 &fetch_attr,
OBSOLETE 			 &new_status,
OBSOLETE 			 &volsync);
OBSOLETE     if (ret) {
OBSOLETE 	arla_warn (ADEBFCACHE, ret, "Symlink");
OBSOLETE 	goto out;
OBSOLETE     }
OBSOLETE 
OBSOLETE     child_fid->Cell = dir_entry->fid.Cell;
OBSOLETE 
OBSOLETE     fill_fid_trans (&childEntry->fid);
OBSOLETE     ret = volcache_getbyid (childEntry->fid.fid.Volume,
OBSOLETE 			    childEntry->fid.Cell,
OBSOLETE 			    ce,
OBSOLETE 			    &childEntry->volume,
OBSOLETE 			    &type);
OBSOLETE 
OBSOLETE     recon_hashtabadd (childEntry);
OBSOLETE 
OBSOLETE     adir_creat(dir_entry, name, childEntry->fid.fid);
OBSOLETE 
OBSOLETE     childEntry->status = fetch_attr;
OBSOLETE     childEntry->flags.attrp = TRUE;
OBSOLETE     childEntry->flags.kernelp = TRUE;
OBSOLETE     childEntry->tokens |= NNPFS_ATTR_R;
OBSOLETE     volcache_update_volsync (dir_entry->volume, dir_entry->volsync);
OBSOLETE 
OBSOLETE     childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
OBSOLETE     assert(childEntry->host);
OBSOLETE 
OBSOLETE     reconnect_update_fid(childEntry, fakeFid);
OBSOLETE     res = conv_dir (dir_entry, ce, 0, &cache_handle, tmp, sizeof(tmp));
OBSOLETE 
OBSOLETE  out: 
OBSOLETE     ReleaseWriteLock(&dir_entry->lock);
OBSOLETE     ReleaseWriteLock(&childEntry->lock);
OBSOLETE     free_fs_server_context(&context);
OBSOLETE     cred_free(ce);
OBSOLETE     return ret;
OBSOLETE }
OBSOLETE 
OBSOLETE #endif
