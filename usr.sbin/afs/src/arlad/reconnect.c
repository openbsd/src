/* COPYRIGHT  (C)  1998
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 * 
 * PERMISSION IS GRANTED TO USE, COPY, CREATE DERIVATIVE WORKS 
 * AND REDISTRIBUTE THIS SOFTWARE AND SUCH DERIVATIVE WORKS 
 * FOR ANY PURPOSE, SO LONG AS THE NAME OF THE UNIVERSITY OF 
 * MICHIGAN IS NOT USED IN ANY ADVERTISING OR PUBLICITY 
 * PERTAINING TO THE USE OR DISTRIBUTION OF THIS SOFTWARE 
 * WITHOUT SPECIFIC, WRITTEN PRIOR AUTHORIZATION.  IF THE 
 * ABOVE COPYRIGHT NOTICE OR ANY OTHER IDENTIFICATION OF THE 
 * UNIVERSITY OF MICHIGAN IS INCLUDED IN ANY COPY OF ANY 
 * PORTION OF THIS SOFTWARE, THEN THE DISCLAIMER BELOW MUST 
 * ALSO BE INCLUDED.
 * 
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION 
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY 
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF 
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING 
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE 
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE 
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR 
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING 
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN 
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGES.
 */

/*
 * Do merging of the files that changed with we was in disconnected mode
 */

#if 0

#include "arla_local.h"

RCSID("$KTH: reconnect.c,v 1.30.2.1 2001/06/04 22:16:39 ahltorp Exp $");

static int reconnect_nonmute(struct vcache *, int, struct timeval);
static int reconnect_putattr(struct vcache *, struct xfs_attr *);
static int reconnect_putdata(struct vcache *);    
static int reconnect_remove(struct vcache *, FCacheEntry *childentry, char *);
static int reconnect_rename(struct vcache *, struct vcache *, char *, char *);
static int reconnect_rmdir(struct vcache *vcp, FCacheEntry *childEntry,
			   char *name);
static int reconnect_mkdir(struct vcache *parent, struct vcache *curdir, 
			   AFSStoreStatus *store_status, char *name);
static int reconnect_link(struct vcache *parent, struct vcache *existing,
			  char *name);
static int reconnect_symlink(struct vcache *parent, struct vcache *child,
			     AFSStoreStatus *store_attr, char *name, 
			     char *contents);
static int reconnect_create(struct vcache *parent, struct vcache *child,
			    char *name);
static int reconnect_mut_chk(FCacheEntry *fce, CredCacheEntry *ce,
			     int version);

static int check_log_todo(log_ent_t * , VenusFid *, VenusFid *);
static int is_done_before(int no);
static void add_done_list(int no);
static void clear_log_entry(void);
static void clear_index_list(void);

typedef struct _fid_trans{
    VenusFid logged_fid;
    VenusFid fetched_fid;
    struct _fid_trans *next, *prev;
} fid_trans;

typedef struct _fid_keep{
    char name[MAX_NAME];
    AFSFid kept_fid;
    struct _fid_keep *next;
} fid_keep;

typedef struct _index_list{
    int   index;
    struct _index_list * next;
} index_list;

fid_trans *fid_AD_head, *fid_AD_tail;
fid_keep  *fid_KP_head;
index_list * index_head;
extern log_ent_t  log_head;


/*
 *
 */

static void
set_fid_value(VenusFid *new, VenusFid *old)
{
    if(old==0) {
	new->fid.Volume = 0;
	new->fid.Vnode = 0;
	new->fid.Unique = 0;
    } else {
	new->fid.Volume = old->fid.Volume;
	new->fid.Vnode  = old->fid.Vnode;
        new->fid.Unique = old->fid.Unique;
    }
}

/*
 *
 */

void 
do_replay(char *log_file, int log_entries, VenusFid *changed_fid)
{
    int fd, len, i;
    log_ent_t *cur_log;
    struct vcache vc, vc_new;
    char *name, *name_new;
    fid_trans *fid_tmp;
    fid_keep * fid_KP_tail;
    VenusFid new_fid; 

    int count=1; /* Used to record how may actions we have done*/

    fid_AD_tail = fid_AD_head;
    cur_log = (log_ent_t *) malloc(sizeof(log_ent_t));
    fd = open(log_file, O_RDWR | O_BINARY);

    set_fid_value(&new_fid , 0);

    while (read(fd, cur_log, sizeof(int))){

	if (cur_log->log_len < sizeof(*cur_log) - sizeof(cur_log->log_data) ||
            cur_log->log_len > sizeof(log_ent_t)) {
	    arla_log(ADEBDISCONN, "do_replay: corrupt log entry, log_len %d",
		     cur_log->log_len);
	    goto terminate;
	}

	len = cur_log->log_len - sizeof(int);

	if (!read(fd, ((char *)cur_log + sizeof(int)), len)){
	    arla_log(ADEBDISCONN, "do_replay: read bad log entry...");
	    goto terminate;
	}

	arla_log(ADEBDISCONN, 
		 "do_replay: read %d bytes of log entry.", 
		 cur_log->log_len);

	if (is_done_before(cur_log->log_index)==1)
	    continue; /* the log entry has been executed */
	else {
	    if (changed_fid !=0) {
		int is_log_todo = 0;
 
		is_log_todo = check_log_todo(cur_log, changed_fid, &new_fid);
		if (is_log_todo ==0)
		    continue; /* This log should not be  executed */
	    }
	}

	add_done_list(cur_log->log_index);

	/* big case/switch statement to switch log_op  */
	switch (cur_log->log_op){

	case DIS_STORE:
	    vc.fid = cur_log->st_fid;
	    vc.DataVersion = cur_log->st_origdv-1;
	    vc.flag = cur_log->st_flag;
	    vc.cred = cur_log->cred;
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** (putdata) op...",
		     count++);
	    reconnect_putdata(&vc);
	    break;
	case DIS_SETATTR:
	    vc.fid = cur_log->sa_fid;
	    vc.DataVersion = cur_log->sa_origdv;
	    vc.flag = 0;
	    vc.cred = cur_log->cred;
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** (putattr) op...",
		     count++);
	    reconnect_putattr(&vc, &(cur_log->sa_vattr));
	    break;
	case DIS_REMOVE: {
	    FCacheEntry *childentry;
	    vc.fid = cur_log->rm_filefid;
	    vc.DataVersion = cur_log->rm_origdv;
	    vc.flag = 0;
	    vc.cred = cur_log->cred;
	    childentry = cur_log->rm_chentry;
	    name = cur_log->rm_name;
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** "
		     "(file remove) op...",
		     count++);
	    reconnect_remove(&vc, childentry, name);
	    break;
	}
	case DIS_RMDIR: {
	    FCacheEntry *child;
	    vc.fid = cur_log->rd_parentfid;
	    name = cur_log->rd_name;
	    vc.cred = cur_log->cred;
	    child = cur_log->rd_direntry;
	    arla_log (ADEBDISCONN,
		      "%d action is to do_replay: **rmdir** "
		      "(directory remove) op...",
		      count++);
	    reconnect_rmdir(&vc, child, name);
	    break;
	}
	case DIS_RENAME:
	    vc.fid = cur_log->rn_oparentfid;
	    vc.DataVersion = cur_log->rn_origdv;
	    vc.flag = 0;
	    vc.cred = cur_log->cred;
	    vc_new.fid = cur_log->rn_nparentfid;
	    vc_new.DataVersion = cur_log->rn_overdv;
	    vc_new.flag = 0;
	    vc_new.cred = cur_log->cred;
	    name = cur_log->rn_names;
	    for (i=0; *(name+i)!='\0';++i);
	    name_new = name+i+1; 
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** (rename) op...",
		     count++);
	    reconnect_rename(&vc, &vc_new, name, name_new);
	    break;
	case DIS_MKDIR:	{ 
	    AFSStoreStatus store_status;
	    vc.fid = cur_log->md_parentfid;
	    vc.cred = cur_log->cred;
	    store_status = cur_log->md_vattr;
	    vc_new.fid = cur_log->md_dirfid;
	    /*Ba Wu: child's data vers*/
	    vc_new.DataVersion = cur_log->md_dversion; 
	    name = cur_log->md_name;
	    arla_log(ADEBDISCONN, 
		     "%d action is to DO_Replay: **replay** (mkdir) op...",
		     count++);
	    reconnect_mkdir(&vc, &vc_new, &store_status, name);
	    break; 
	}
	case DIS_LINK:
	    vc.cred = cur_log->cred;
	    vc.fid = cur_log->ln_parentfid;
	    vc_new.fid = cur_log->ln_linkfid;
	    name = cur_log->ln_name;
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** (link) op...",
		     count++);
	    reconnect_link(&vc, &vc_new, name);
	    break;
	case DIS_SYMLINK: {
	    char *new_name;
	    AFSStoreStatus store_attr;
	    
	    vc.fid = cur_log->sy_parentfid;
	    vc.cred = cur_log->cred;
	    name = cur_log->sy_name;
	    new_name = cur_log->sy_content;
	    vc_new.fid = cur_log->sy_filefid;
	    store_attr = cur_log->sy_attr;
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** (symlink) op...",
		     count++);
	    reconnect_symlink(&vc, &vc_new, &store_attr, name, new_name);
	    break;
	}
	case DIS_CREATE:
	    vc.fid = cur_log->cr_parentfid;
	    vc.cred = cur_log->cred;
 
	    vc_new.fid = cur_log->cr_filefid;
	    vc_new.DataVersion = cur_log->cr_origdv;
	    arla_log(ADEBDISCONN, 
		     "%d action is to DO_Replay: **replay** (create) op...",
		     count++);
	    name = cur_log->cr_name;
	    reconnect_create(&vc, &vc_new, name); 
	    break; 
	case DIS_ACCESS:
	    vc.fid = cur_log->nm_fid;
	    vc.DataVersion = cur_log->nm_origdv;
	    vc.cred = cur_log->cred;
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: **replay** (nonmutating) op",
		     count++);
	    reconnect_nonmute(&vc, cur_log->log_op, cur_log->log_time);
	    break; 

	default:
	    arla_log(ADEBDISCONN, 
		     "%d action is to do_replay: skipping the current op=%d", 
		     count++,cur_log->log_op);
	}
    }  

    if (changed_fid ==0) {
	clear_index_list();  /* clean all index when after discon */
	clear_log_entry();
        /* clean up, remove all associative data structures */
        fid_AD_tail = fid_AD_head;
  	while(fid_AD_tail)
  	{
	    fid_tmp = fid_AD_tail->next;
	    free(fid_AD_tail);
	    fid_AD_tail = fid_tmp; 
  	}   
	/* SWW Qiyue 28: We need to reset head to 0*/
  	fid_AD_head = 0;
  	fid_KP_tail = fid_KP_head;
    	while(fid_KP_tail)
    	{
	    fid_keep *fid_tmp;
	
	    fid_tmp = fid_KP_tail->next;
	    free(fid_KP_tail);
	    fid_KP_tail = fid_tmp;
    	}
   	 
  	fid_KP_head = 0;
    }
    i = ftruncate (fd, 0);
    assert (i == 0);

 terminate:

    arla_warnx (ADEBDISCONN,"We have done total %d replays",count-1);
    close(fd);  
    free(cur_log);
    return;
}

/*
 *
 */

static int 
check_rm_fid (VenusFid v1, VenusFid v2)
{
    if(v1.fid.Vnode == v2.fid.Vnode &&
       v1.fid.Volume == v2.fid.Volume &&
       v1.fid.Unique == v2.fid.Unique )
	return 1;
    else
	return 0;
}

/*
 *
 */

static int 
check_log_fid(struct vcache vc, VenusFid *fid)
{
    log_ent_t *temp = log_head.next;

    if (vc.fid.fid.Vnode == fid->fid.Vnode &&
	vc.fid.fid.Volume == fid->fid.Volume &&
	vc.fid.fid.Unique == fid->fid.Unique)
	return 1;

    while (temp!=0) {
	switch(temp->log_op) {
	case DIS_RENAME:
	    if (check_rm_fid(temp->rn_oparentfid,*fid)==1) 
		return 1;
	default: 
	    temp = temp->next;
	    break;
        }
    }
    return 0;
}

/*
 *
 */

static int 
check_log_todo(log_ent_t * cur_log , VenusFid *fid, VenusFid *newfid)
{
    VenusFid *lookfid;
    struct vcache vc, vc_new;
    int will_do = 0;

    if (newfid->fid.Vnode ==0 &&
	newfid->fid.Volume == 0 &&
	newfid->fid.Unique ==0)
	lookfid = fid;
    else
	lookfid = newfid; /* For create and putdata */

    switch (cur_log->log_op){
    case DIS_STORE:
        vc.fid = cur_log->st_fid;
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    set_fid_value(newfid, 0);
	    return 1;
        }
        break;
    case DIS_SETATTR:
	vc.fid = cur_log->sa_fid;
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    set_fid_value(newfid , &cur_log->sa_fid);
	    return 1;
        }
        break;
    case DIS_REMOVE:
	vc.fid = cur_log->rm_filefid;
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    return 1;
        }
        break;
    case DIS_RMDIR:
	vc.fid = cur_log->rd_parentfid;
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    return 1;
        }
        break;
    case DIS_RENAME:
	vc.fid = cur_log->rn_oparentfid;
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    return 1;
        }
	vc_new.fid = cur_log->rn_nparentfid;
	will_do = check_log_fid(vc_new, lookfid);
        if (will_do==1) {
	    return 1;
        }
        break;
    case DIS_MKDIR:
	vc.fid = cur_log->md_parentfid;
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    return 1;
        }
	break;
    case DIS_LINK:
        break;
    case DIS_SYMLINK:
	will_do = check_log_fid(vc, lookfid);
        if (will_do==1) {
	    return 1;
        }
        break;
    case DIS_CREATE:
	vc.fid = cur_log->cr_parentfid;
        will_do = check_log_fid(vc, lookfid);
	if (will_do==1) {
	    set_fid_value(newfid , &cur_log->cr_filefid);
	    return 1;
	}
	break;
    case DIS_FSYNC:
    case DIS_ACCESS:
    case DIS_READDIR:
    case DIS_READLINK:
    case DIS_INFO:
    case DIS_START_OPT:
    case DIS_END_OPT:
    case DIS_REPLAYED:
	/* A no op */
	break;
    }
    return 0;
}


#if 0

/*
 *
 */

void
keepfid_newrename(char *name,
		  AFSFid fid)
{
    if (fid_KP_head == 0) {
        fid_KP_head = (fid_keep *)malloc(sizeof(fid_keep));
        assert(fid_KP_head);

        strlcpy(fid_KP_head->name, name, sizeof(fid_KP_head->name));
	fid_KP_head->kept_fid = fid;
	fid_KP_head->next = 0;
    }
    else { 
        fid_keep *temp;

        temp = (fid_keep *)malloc(sizeof(fid_keep));
	assert(temp);

        strlcpy(temp->name, name, sizeof(temp->name));
	temp->kept_fid = fid;
	temp->next = fid_KP_head->next;
	fid_KP_head->next = temp;
    }
}

#endif

/*
 *
 */

static int 
find_venus (char *name, VenusFid *fid)
{
    fid_keep *fid_temp;

    if(fid_KP_head == 0 )
	return 1;  /*error */
  	
    fid_temp = fid_KP_head;

    while(fid_temp) {
	if (strcmp(fid_temp->name,name) == 0) {
	    fid->fid.Volume = fid_temp->kept_fid.Volume;
	    fid->fid.Vnode = fid_temp->kept_fid.Vnode;
	    fid->fid.Unique = fid_temp->kept_fid.Unique;
	    return 0;
        }
	fid_temp = fid_temp->next;
    }
    arla_warnx (ADEBDISCONN, "find_venus: *PANIC* not found fid for %s", name);
    return 1;
}


/*
 *
 */

VenusFid *
fid_translate(VenusFid *fid_in)
{
    fid_trans *fid_tmp;
    VenusFid *fid_ret;

    if (!fid_AD_head)
	return fid_in;

    fid_tmp = fid_AD_head;

    while(fid_tmp){

	fid_ret=&fid_tmp->logged_fid;

	if ((fid_ret->Cell == fid_in->Cell) && 
	    (fid_ret->fid.Volume == fid_in->fid.Volume) && 
	    (fid_ret->fid.Vnode == fid_in->fid.Vnode) && 
	    (fid_ret->fid.Unique == fid_in->fid.Unique))
	    return &fid_tmp->fetched_fid;

	fid_tmp = fid_tmp->next;	   
    }
    return fid_in; 
}

/*
 *
 */

static void 
clear_index_list(void)
{
    index_list *temp=index_head;

    while(temp!=0) {
	index_list *tmp;
    
  	tmp = temp->next;
        free(temp);
	temp = tmp;
    }
    index_head = 0;
}

/*
 *
 */

static void 
clear_log_entry(void)
{
    log_ent_t *temp=log_head.next;

    while(temp!=0) {             
        log_ent_t *tmp;

        tmp = temp->next;
        free(temp);                          
        temp = tmp;
    }                        
    log_head.next = 0;    
}

/*
 *
 */

static int 
is_done_before(int no)
{
    index_list * temp = index_head;

    while(temp !=0) {
	if (temp->index == no)
	    return 1;
	else
	    temp = temp->next;
    }
    return 0;
}

/*
 *
 */

static void 
add_done_list(int no)
{
    if (!index_head) {
	index_head = (index_list *)malloc(sizeof(index_list));
	assert(index_head);
	index_head->index = no;
	index_head->next = 0;
    } else {
	index_list * temp;
	temp = (index_list *) malloc(sizeof(index_list));
	assert(temp);
	temp->next = index_head->next;
	index_head->next = temp;
	temp->index = no;
    }
}

/*
 *
 */

static void 
alloc_fid_trans(VenusFid *logged_fid)
{
    if (!fid_AD_head)
    {
	/*SWW Qiyue 28 Make sure we have the memory*/
	fid_AD_head = (fid_trans *) malloc(sizeof(fid_trans)); 
	assert(fid_AD_head);

	fid_AD_head->prev=fid_AD_head->next = 0;
	fid_AD_tail = fid_AD_head;
	fid_AD_tail->logged_fid = *logged_fid;

    } else{

	/*SWW Qiyue 28 Make sure we have the memory*/
	fid_AD_tail->next = (fid_trans *) malloc(sizeof(fid_trans)); 
	assert(fid_AD_tail->next);

	fid_AD_tail->next->prev = fid_AD_tail;
	fid_AD_tail->next->next = 0;
	fid_AD_tail = fid_AD_tail->next;  /*Ba ba: move tail ahead */
	fid_AD_tail->logged_fid = *logged_fid;
    }
}

/*
 *
 */

static void 
fill_fid_trans (VenusFid *fetched_fid)
{
    fid_AD_tail->fetched_fid = *fetched_fid;
}

/*
 *
 */

#if 0
void
update_entry_flag (FCacheEntry *entry)
{
    entry->flags.attrp = FALSE;
    entry->flags.datap = FALSE;
}
#endif

/*
 *
 */

int reconnect_nonmute(struct vcache *vcp, int op, struct timeval log_time)
{
    FCacheEntry *fce, fce_fetched;
    CredCacheEntry *ce;
    int error;
    VenusFid *fid; 
#if 0
    ConnCacheEntry *conn;
#endif

    arla_warnx (ADEBDISCONN,
		"Start of reconnect_nonmute by sww"); /*SWW Qiyue 25*/

    fid = &(vcp->fid);
    if (fid->Cell == -1) /* newly created file, skip reconnect */
	return 0;

    error = fcache_find(&fce, *fid);
    /* assert(fce); */
    if (error) /* nonmute op on nonexisting data */
    {     
	arla_log(ADEBDISCONN, 
		 "reconnect: nonmute op %d performed on cache "
		 "entry no longer exist locally!", 
		 op); 
	return -1;
    }   

    arla_log(ADEBDISCONN, 
	     "reconnect: DISCONNECTED nonmute "
	     "on fid.Cell=0x%x, fid.fid.Volume= 0x%x, fid.fid.Vnode=0x%x, "
	     "fid.fid.Unique=0x%x", fid->Cell, 
	     fid->fid.Volume,
	     fid->fid.Vnode, 
	     fid->fid.Unique);  

    ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
    assert (ce != NULL);
    error = 0;
  
    /* setting some stuff so do_read_attr would work */
    fce->flags.attrp = TRUE;
    fce->callback.CallBackType = 2;
    fce_fetched = *fce;
    /*conn = findconn (fce, ce);*/

    error = read_attr(&fce_fetched, ce);

    arla_log(ADEBDISCONN, 
	     "reconnect: logged DataVersion=%d, "
	     "fetched DataVersion=%d", 
	     vcp->DataVersion,
	     fce_fetched.status.DataVersion);
 
    if (vcp->DataVersion < fce_fetched.status.DataVersion)
    {
	if (log_time.tv_usec >= fce_fetched.status.ServerModTime)
	    arla_log(ADEBDISCONN, 
		     "Warning: nonmutating operation %d read stale data!", 
		     op);
	else if (log_time.tv_usec <= fce_fetched.status.ServerModTime && 
		 (vcp->DataVersion +1) == fce_fetched.status.DataVersion)
	    arla_log(ADEBDISCONN, 
		     "Notice: file modified once after nonmutating "
		     "operation %d.",
		     op);
	else
	    arla_log(ADEBDISCONN, 
		     "Warning: nonmutating operation %d might have read "
		     "stale data!", op);
    } 
 
    ReleaseWriteLock(&fce->lock);
    cred_free(ce);
    /*conn_free(conn);*/

    return error;
}

/*
 *
 */

int reconnect_remove(struct vcache *vcp, FCacheEntry *childentry, char *name)
{
    FCacheEntry *fce;
    CredCacheEntry *ce;
    int error;
    VenusFid *fid, tempfid; /* Ba san: to check the deletion of file*/
    Result res;
    int    isupdate;

    ConnCacheEntry *conn;
    fs_server_context context;
    AFSFetchStatus status;
    AFSVolSync volsync; 
    char tmp[2 * sizeof(int) + 2];
    fcache_cache_handle cache_handle;

    fid = &(vcp->fid); /* points to the VenusFid structure */
    fid = fid_translate(fid);

    arla_log(ADEBDISCONN, "reconnect: DISCONNECTED remove on "
	     "fid.Cell=0x%x, fid.fid.  Volume= 0x%x, fid.fid.Vnode=0x%x, "
	     "fid.fid.Unique=0x%x", 
	     fid->Cell, fid->fid.Volume, fid->fid.Vnode, fid->fid.Unique);

    /* ObtainWriteLock called in fcache_find */
    error = fcache_find(&fce, *fid); 
    assert (error == 0);

    ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
    assert (ce != NULL);

    if (connected_mode != CONNMODE_PARCONNECTED) {
	ObtainWriteLock(&childentry->lock);
	isupdate = reconnect_mut_chk(childentry, ce, 
				     childentry->status.DataVersion);
	ReleaseWriteLock(&childentry->lock);
  
	if (isupdate)
	{
	    fcache_cache_handle cache_handle;

	    arla_log(ADEBDISCONN, 
		     "reconnect_remove: can't remove because file modified!");
	    cred_free(ce);
	    adir_creat (fce, name, childentry->fid.fid);
	    childentry->flags.attrp = FALSE;
	    ReleaseWriteLock(&fce->lock);
	    conv_dir (fce, ce, 0, &cache_handle, tmp, sizeof(tmp));
	    ReleaseWriteLock(&fce->lock);
	    return -1;
	}
    }  /* Ba ershi: we dont need to do it in parconn */

    res.res = 0;

    AssertExclLocked(&fce->lock);

    conn = find_first_fs (fce, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed", fce->index);
	cred_free(ce);
	ReleaseWriteLock(&fce->lock);
	return ENETDOWN;
    }

/* Ba san: check the file exists 

   ReleaseWriteLock(&fce->lock);
   error = adir_lookup(fce->fid, name, &tempfid, NULL, &ce);
   assert (error == 0);                                             
   ObtainWriteLock(&fce->lock);  */
     
    res.res = RXAFS_RemoveFile (conn->connection,
				&fce->fid.fid,
				name,
				&status,
				&volsync);
    if (res.res) {
	arla_log (ADEBDISCONN, "Could not RemoveFile: %s (%d)",
		  koerr_gettext(res.res), res.res);
	goto out;
    }

    arla_warnx (ADEBDISCONN,"In reconnect_remove: Remove the file %s",name);

/* Ba san: Chcek the deletion of the file */
    ReleaseWriteLock(&fce->lock);
    error = adir_lookup(&fce->fid, name, &tempfid, NULL, &ce);
    ObtainWriteLock(&fce->lock);

    if (error == 0) {
	int result;
        
	arla_warnx (ADEBDISCONN,
		    "In reconnect_remove: file %s needs to be deleted",
		    name);
	result = adir_remove (fce,name);
	assert ( result == 0);
    } /* This is for the file produced during disconnect mode,
	 if error==ENOENT then the file is created during connect mode*/
  

    fce->status  = status;
    fce->volsync = volsync;
    childentry->host = 0;  /* Ba shiliu dont get callback */

    volcache_update_volsync (fce->volume, fce->volsync);
    conv_dir (fce, ce, 0, &cache_handle, tmp, sizeof(tmp));

 out:

    cred_free(ce);
    free_fs_server_context(&context);
    ReleaseWriteLock(&fce->lock); 
    return error;
}

/*
 *
 */

int 
reconnect_rmdir(struct vcache *vcp, FCacheEntry *childEntry, char *name)
{
    FCacheEntry *fce;
    CredCacheEntry *ce;
    int error;               
    VenusFid *fid, tempfid; /* Ba san: to check the deletion of file*/
    Result res;
    char tmp[2 * sizeof(int) + 2];
    int ret = 0; 
    Result tempres; 
    fcache_cache_handle cache_handle;
  
    ConnCacheEntry *conn;
    fs_server_context context;
    AFSFetchStatus status;
    AFSVolSync volsync;

    fid = &(vcp->fid); /* points to the VenusFid structure */
    fid = fid_translate(fid);

    ret = fcache_find(&fce, *fid);
    assert (ret == 0);

    ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
    assert (ce != NULL);

    AssertExclLocked(&fce->lock);

    conn = find_first_fs (fce, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed", fce->index);
	cred_free(ce);
	ReleaseWriteLock(&fce->lock);
	return ENETDOWN;
    }

    ret = RXAFS_RemoveDir (conn->connection,
			   &fce->fid.fid,
			   name,
			   &status,
			   &volsync);
    if (ret) {
	arla_log (ADEBDISCONN, 
		  "Could not RemoveDir : %s (%d)", 
		  koerr_gettext(res.res),res.res);
	goto out;
    }

/* Ba san: Chcek the deletion of the file */
    ReleaseWriteLock(&fce->lock);
    error = adir_lookup(&fce->fid, name, &tempfid, NULL, &ce);
    ObtainWriteLock(&fce->lock);

    if (error == 0) {
        int result;

        arla_warnx (ADEBDISCONN,
		    "In reconnect_rmdir: file %s needs to be deleted",name);
        result = adir_remove (fce,name);
        assert ( result == 0);
    } /* This is for the file produced during disconnect mode,
	 if error==ENOENT then the file is created during connect mode*/

    fce->status  = status;
    fce->volsync = volsync;

    volcache_update_volsync (fce->volume, fce->volsync);

    tempres = conv_dir(fce, ce, 0, &cache_handle, tmp, sizeof(tmp));

    childEntry->host = 0; /*Ba shiqi: no callback for this entry*/

 out:

    cred_free(ce);
    free_fs_server_context(&context);
    ReleaseWriteLock(&fce->lock);
    return error;
}

/*
 *
 */

static int 
reconnect_mut_chk(FCacheEntry *fce, CredCacheEntry *ce, int version)
{
    ConnCacheEntry *conn;
    fs_server_context context;
    FCacheEntry fetched = *fce;
    int ret;

    AFSFetchStatus status;                                                   
    AFSCallBack callback;                                                    
    AFSVolSync volsync;      

    AssertExclLocked(&fetched.lock);

/*SWW Aug 01: >= is changed into > */
    conn = find_first_fs (&fetched, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed", fce->index);
	return ENETDOWN;
    }
 
    ret = RXAFS_FetchStatus (conn->connection,
                             &fce->fid.fid,
                             &status,
                             &callback,
                             &volsync);
    if (ret) {
        if (ret == -1)
            ret = ENETDOWN;
        free_fs_server_context(&context);
        arla_warn (ADEBFCACHE, ret, "fetch-status");
        return ret;
    }

    if (status.DataVersion > version)
    {
	arla_log(ADEBDISCONN, "reconnect_mut_chk: concurrent writes detected!");
	return 1;
    }
    free_fs_server_context(&context);
    return 0;
}

/*
 *
 */

static void
fcache_backfile_name(char *name, size_t len)
{
    static int no = 1;

    snprintf (name, len, "%04X",no++);
    strlcat (name, "bak", len);
}

/*
 *
 */

static void
copy_cached_file(int from, int to)
{
    char name_from[2 * sizeof(int) + 1], name_to[2 * sizeof(int) + 1];
    int fd_from, n, fd_to;
    char buf[BUFSIZE];

    snprintf (name_from, sizeof(name_from), "%04X", from);
    snprintf (name_to,   sizeof(name_to),   "%04X", to);

    fd_from = open(name_from,O_RDONLY | O_BINARY);  
    fd_to   = open(name_to,  O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, 0600);

    while((n = read(fd_from, buf, BUFSIZE)) > 0)
	write(fd_to, buf, n);

#if 0
    if(fstat(fd_to, &statinfo)<0) {
	arla_warnx(ADEBDISCONN,"ERROR");
    }
#endif   

    close(fd_from);
    close(fd_to);
}

/*
 *
 */

static void
reconnect_update_fid (FCacheEntry *entry, VenusFid oldfid)
{
    if (entry->flags.kernelp)
	update_fid (oldfid, NULL, entry->fid, entry);
}

/*
 *
 */

static int
reconnect_mut_newfile(FCacheEntry **fcep, xfs_pag_t cred,VenusFid *new_fid)
{

    FCacheEntry *parent_fce;
    u_long host;
    char name[2 * sizeof(int) + 3 + 1], tmp[2 * sizeof(int) + 2];
    AFSStoreStatus store_attr;
    AFSFetchStatus fetch_attr;
    CredCacheEntry *ce; 
    AccessEntry *ae;
    VenusFid newfid;
    int ret;
    int from, to;
    fcache_cache_handle cache_handle;

    ret = fcache_find (&parent_fce, (*fcep)->parent);
    assert (ret == 0);

    host = (*fcep)->host;

    ce = cred_get((*fcep)->parent.Cell, cred, CRED_ANY);

    fcache_backfile_name (name, sizeof(name));

    store_attr.Mask = 8;
    store_attr.ClientModTime = 430144;
    store_attr.Owner = 1957724;
    store_attr.Group = 21516;
    store_attr.UnixModeBits = 420;
    store_attr.SegSize = 0;

    create_file(parent_fce, name, &store_attr, &newfid, &fetch_attr, ce);

    (*fcep)->flags.datap = FALSE; /* Ba shiqi: we need to get the old from FS*/
    *new_fid = newfid;
    from = (*fcep)->index;
    ret = fcache_find(fcep, newfid);
    assert (ret == 0);
    to   = (*fcep)->index;
    (*fcep)->host = host;
    (*fcep)->flags.attrp = TRUE;
    (*fcep)->flags.datap = TRUE;
    findaccess(ce->cred, (*fcep)->acccache, &ae); /*Ba shijiu obtain access */
    ae->cred   = ce->cred;
    ae->access = (*fcep)->status.CallerAccess;

    ReleaseWriteLock(&(*fcep)->lock);

    copy_cached_file(from, to);
    ret = adir_creat (parent_fce, name, newfid.fid);
    assert (ret ==0);
    conv_dir (parent_fce, ce, 0, &cache_handle, tmp, sizeof(tmp));
    ReleaseWriteLock(&parent_fce->lock);

    return 0;
}

   
/*
 *
 */

int reconnect_putattr(struct vcache *vcp, struct xfs_attr *xap)
{

    ConnCacheEntry *conn;
    fs_server_context context;
    struct rx_call *call;
    VenusFid *fid;
    CredCacheEntry *ce;
    FCacheEntry *fce, *tempce;
    AFSFetchStatus status;
    Result res;
    u_int32_t sizefs;
    AFSStoreStatus storestatus;
    AFSVolSync volsync;
    int ret;

    fid = &(vcp->fid); /* points to the VenusFid structure */
    fid = fid_translate(fid);

#if 0
    arla_log(ADEBDISCONN, "reconnect: DISCONNECTED write on fid.Cell=0x%x, "
	     "fid.fid.Volume= 0x%x, fid.fid.Vnode=0x%x, fid.fid.Unique=0x%x",
	     fid->Cell, 
	     fid->fid.Volume, 
	     fid->fid.Vnode, 
	     fid->fid.Unique);
#endif

    ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
    assert (ce != NULL);
    res.res = 0;

#if 0
/* Ba shier: should we send the file back to server?  */
    if (XA_VALID_SIZE(xap)){
	res = cm_ftruncate (*fid, xap->xa_size, ce);
    }  
#endif

    ret = fcache_find(&fce, *fid);
    assert (ret == 0);
    tempce = fce;

    sizefs=fce->status.Length;

#if 0 /* XXX */
    /* some people have written to the file while we are disconnected */
    /* we have to give it a different name on the server  */
    if (reconnect_mut_chk(fce, ce, vcp->DataVersion))
    {
	VenusFid new_fid;

	alloc_fid_trans(fid);
	reconnect_mut_newfile(&fce,vcp->cred.pag,&new_fid);  
	fce->status.Length = sizefs;
	fce->length = sizefs;
	ReleaseWriteLock(&tempce->lock);
	fill_fid_trans(&new_fid);
	tempce->flags.attrp = FALSE;
	tempce->flags.kernelp = FALSE;
    }   
#endif

    /* code from truncate file XXX join */
    conn = find_first_fs (fce, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed.");
	ReleaseWriteLock(&fce->lock);
	cred_free(ce);
	return ENETDOWN;
    }

    if (fce->status.FileType != TYPE_DIR) {

	call = rx_NewCall (conn->connection);
	if (call == NULL) {
	    arla_log (ADEBDISCONN, "Cannot call");
	    res.res = ENOMEM;
	    goto out;
	}

	storestatus.Mask = 0;
	res.res = StartRXAFS_StoreData (call,
					&(fce->fid.fid),
					&storestatus,
					0, 0, fce->status.Length);
	if(res.res) {
	    arla_log (ADEBDISCONN, "Could not start store, %s (%d)",
		      koerr_gettext(res.res), res.res);
	    rx_EndCall(call, 0);
	    goto out;
	}

	sizefs = htonl (sizefs);
	if (rx_Write (call, &sizefs, sizeof(sizefs)) != sizeof(sizefs)) {
	    res.res = conv_to_arla_errno(rx_Error(call));
	    arla_log (ADEBDISCONN, "Error writing length: %d", res.res);
	    rx_EndCall(call, 0);
	    goto out;
	}

	if (rx_Write (call, 0, 0) != 0) {
	    res.res = conv_to_arla_errno(rx_Error(call));
	    arla_log (ADEBDISCONN, "Error writing: %d", res.res);
	    rx_EndCall(call, 0);
	    goto out;
	}

	res.res = rx_EndCall (call, EndRXAFS_StoreData (call,
							&status,
							&volsync));
	if (res.res) {
	    arla_log (ADEBDISCONN, "Error rx_EndCall: %s (%d)",
		      koerr_gettext(res.res), res.res);
	    goto out;
	}

	fce->status   = status;
	fce->volsync  = volsync;

	volcache_update_volsync (fce->volume, fce->volsync);

    }
    /* code from write_attr XXX join */
    xfs_attr2afsstorestatus(xap, &storestatus);

    res.res = RXAFS_StoreStatus (conn->connection,
				 &fce->fid.fid,
				 &storestatus,
				 &status,
				 &volsync);
    if (res.res) {
        arla_log (ADEBDISCONN, "Could not make store-status call, %s (%d)",
		  koerr_gettext(res.res), res.res);
        goto out;
    }
    arla_log(ADEBDISCONN, 
	     "write_attr: status.Length = %d", status.Length);
    fce->status  = status;
    fce->volsync = volsync;

    volcache_update_volsync (fce->volume, fce->volsync);

 out:

    free_fs_server_context(&context);
    ReleaseWriteLock(&fce->lock);
    cred_free(ce);
    return res.res;
}

/*
 *
 */

static int 
reconnect_putdata(struct vcache *vcp)
{
    VenusFid *fid;
    FCacheEntry *fce;
    CredCacheEntry *ce;
    Result res;

    u_int32_t sizefs;
    int fd = -1;
    struct rx_call *call;
    ConnCacheEntry *conn;
    fs_server_context context;
    struct stat statinfo;
    AFSStoreStatus storestatus;
    AFSFetchStatus status;
    AFSVolSync volsync;
    int ret;

    fid = &(vcp->fid); /* points to the VenusFid structure */
    arla_log(ADEBDISCONN, "reconnect: putdata before fid_translate, "
	     "fid->Cell=0x%x, fid->fid.Volume=0x%x, fid->fid.Vnode=0x%x, "
	     "fid->fid.Unique=0x%x", 
	     fid->Cell, 
	     fid->fid.Volume, 
	     fid->fid.Vnode, 
	     fid->fid.Unique);

    fid = fid_translate(fid);

    arla_log(ADEBDISCONN, "reconnect: putdata after fid_translate, "
	     "fid->Cell=0x%x, fid->fid.Volume=0x%x, fid->fid.Vnode=0x%x, "
	     "fid->fid.Unique=0x%x", 
	     fid->Cell, 
	     fid->fid.Volume, 
	     fid->fid.Vnode, 
	     fid->fid.Unique);


    ce = cred_get (fid->Cell, vcp->cred.pag, CRED_ANY);
    assert (ce != NULL);

    ret = fcache_find (&fce, *fid);
    assert (ret == 0);

#if 0
    isupdate = reconnect_mut_chk(fce, ce, vcp->DataVersion);
    if (isupdate)
    {
	arla_log(ADEBDISCONN, 
		 "reconnect_putdata: send data back because "
		 "the file was modified!");
	cred_free(ce);
	ReleaseWriteLock(&fce->lock);
	reconnect_mut_newfile(&fce, vcp->cred.pag);  
	return -1;
    }

    if (reconnect_mut_chk (fce, ce)) {
	arla_log (ADEBDISCONN, "Reconnect_putdata: can not send the file"
		  "to FS becausethis file has been modified!");
	ReleaseWriteLock(&fce->lock);
	return -1;
    } 
#endif

    /* code taken from write_data XXX join */ 
    AssertExclLocked(&fce->lock);

    conn = find_first_fs (fce, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed");
	ReleaseWriteLock(&fce->lock); 
	cred_free(ce);
	return ENETDOWN;
    }

    fd = fcache_open_file (fce, O_RDONLY);
    if (fd < 0) {
	arla_log (ADEBDISCONN, "open %u failed", fce->index);
	res.res = errno;
	goto out;
    }

    if (fstat (fd, &statinfo) < 0) {
	arla_log (ADEBDISCONN, "Cannot stat file %u", fce->index);
	res.res = errno;
	goto out;
    }

    sizefs = statinfo.st_size;

    call = rx_NewCall (conn->connection);
    if (call == NULL) {
	arla_log (ADEBDISCONN, "Cannot call");
	res.res = ENOMEM;
	goto out;
    }

    storestatus.Mask = 0; /* Dont save anything */
    res.res = StartRXAFS_StoreData (call, &fce->fid.fid,
				    &storestatus,
				    0,
				    sizefs,
				    sizefs);
    if (res.res) {
	arla_log (ADEBDISCONN, "Could not start store, %s (%d)",
		  koerr_gettext(res.res), res.res);
	rx_EndCall(call, 0);
	goto out;
    }

    res.res = copyfd2rx (fd, call, 0, sizefs);
    if (res.res) {
	rx_EndCall(call, res.res);
	arla_log (ADEBDISCONN, "copyfd2rx failed: %d", res.res);
	goto out;
    }
     
    res.res = rx_EndCall (call, EndRXAFS_StoreData (call,
						    &status,
						    &volsync));
    if (res.res) {
	arla_log (ADEBDISCONN, "Error rx_EndCall: %s (%d)", 
		  koerr_gettext(res.res), res.res);
	goto out;
    }
    if (status.DataVersion > fce->status.DataVersion)
	arla_log(ADEBDISCONN, 
		 "reconnect: putdata, server incremented DataVersion!");

    fce->status   = status;
    fce->volsync  = volsync;

    volcache_update_volsync (fce->volume, fce->volsync);

 out:

    ReleaseWriteLock(&fce->lock); 
    if (fd != -1)
	close (fd);
    free_fs_server_context (&context);
  
    cred_free(ce);
    return res.res;
}

/*
 *
 */

int reconnect_rename(struct vcache *vcp_old, struct vcache *vcp_new, 
		     char *name_old, char *name_new)
{

    FCacheEntry *fce_old, *fce_new;
    CredCacheEntry *ce;
    VenusFid *fid_old, *fid_new,foo_fid,*tempnew_fid;
    int error;

    int ret = 0;
    Result res;
    char tmp[2 * sizeof(int) + 2];
    int isnewpar = 0;
    ConnCacheEntry *conn;
    fs_server_context context;
    AFSFetchStatus orig_status, new_status;
    AFSVolSync volsync;
    fcache_cache_handle cache_handle;

    fid_old = &vcp_old->fid;
    fid_old = fid_translate(fid_old);
 
    ret = fcache_find (&fce_old, *fid_old);
    assert (ret == 0);

    /* ReleaseWriteLock(&fce_old->lock);  SWW Qiyue 28 Maybe we dont need it*/
    assert(fce_old);
    arla_log(ADEBDISCONN, "reconnect: old rename on Cell=0x%x, "
	     "fid.Volume= 0x%x, fid.Vnode=0x%x, fid.Unique=0x%x", 
	     fce_old->fid.Cell,
	     fce_old->fid.fid.Volume,
	     fce_old->fid.fid.Vnode, 
	     fce_old->fid.fid.Unique);

    fid_new = tempnew_fid = &vcp_new->fid;
    fid_new = fid_translate(fid_new);
    
    if (tempnew_fid->fid.Volume != fid_new->fid.Volume ||
	tempnew_fid->fid.Vnode != fid_new->fid.Vnode ||
	tempnew_fid->fid.Unique != fid_new->fid.Unique)
        isnewpar = 1; 

/*Ba ba: the parent dir was created during disconnected */

    if (fid_old->fid.Volume == fid_new->fid.Volume &&
	fid_old->fid.Vnode == fid_new->fid.Vnode   &&
	fid_old->fid.Unique == fid_new->fid.Unique )
        ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/

    ret = fcache_find (&fce_new, *fid_new);
    assert (ret == 0);

    arla_log(ADEBDISCONN, "reconnect: new rename on Cell=0x%x, "
	     "fid.Volume= 0x%x, fid.Vnode=0x%x, fid.Unique=0x%x", 
	     fce_new->fid.Cell, 
	     fce_new->fid.fid.Volume, 
	     fce_new->fid.fid.Vnode, 
	     fce_new->fid.fid.Unique);



    arla_log(ADEBDISCONN, 
	     "reconnect_rename: fce_old = 0x%x, fce_new = 0x%x",
	     fce_old, fce_new);

    ce = cred_get (vcp_old->fid.Cell, vcp_old->cred.pag, CRED_ANY);
    assert (ce != NULL);

    AssertExclLocked(&fce_old->lock);
    AssertExclLocked(&fce_new->lock);

    conn = find_first_fs (fce_old, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed");
	ReleaseWriteLock(&fce_new->lock);
	
	if (fid_old->fid.Volume != fid_new->fid.Volume ||
	    fid_old->fid.Vnode != fid_new->fid.Vnode   ||
	    fid_old->fid.Unique != fid_new->fid.Unique )
	    ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/
	
	cred_free(ce);
	return ENETDOWN;
    }

    error = RXAFS_Rename (conn->connection,
			  &fce_old->fid.fid,
			  name_old,
			  &fce_new->fid.fid,
			  name_new,
			  &orig_status,
			  &new_status,
			  &volsync);

    if (error) {
	arla_log (ADEBDISCONN, "Could not Rename: %s (%d)", koerr_gettext(error), error); 
	goto out; }

    fce_old->status = orig_status;
    fce_new->status = new_status;

    fce_old->volsync = fce_new->volsync = volsync;

    volcache_update_volsync (fce_old->volume, fce_old->volsync);


/*SWW Aug 01 */
    arla_warnx (ADEBDISCONN,
		"reconnect_rename: we delete the old one %s volumn=0x%x, "
		"vnode=0x%x,unique=0x%x",
		name_old,fce_old->fid.fid.Volume,
		fce_old->fid.fid.Vnode,
		fce_old->fid.fid.Unique);                      

/*Ba Yi: get the VenuseFid for new file */
  #if 0
    if (fid_old->fid.Volume == fid_new->fid.Volume &&
	fid_old->fid.Vnode == fid_new->fid.Vnode   &&
	fid_old->fid.Unique == fid_new->fid.Unique ) ;
#endif
    ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/
    
    error = adir_lookup (&fce_old->fid, name_old, &foo_fid, NULL, &ce);
    
#if 0
    if (fid_old->fid.Volume == fid_new->fid.Volume &&
       fid_old->fid.Vnode == fid_new->fid.Vnode   &&
       fid_old->fid.Unique == fid_new->fid.Unique );
#endif
    ObtainWriteLock (&fce_old->lock);

/*Ba San: delete the old which was created during dis */
    if (error == 0) {
	arla_warnx (ADEBDISCONN,"reconnect_rename: we delete the old one %s "
		    "volumn=0x%x,vnode=0x%x,unique=0x%x",
		    name_old,
		    foo_fid.fid.Volume,
		    foo_fid.fid.Vnode,
		    foo_fid.fid.Unique);

	adir_remove(fce_old,name_old);
	adir_remove(fce_new,name_new);

	res = conv_dir (fce_old, ce, 0, &cache_handle, tmp, sizeof(tmp));

    } else {
	/* if found delete it */
/*Ba San: try to find the previous VenuseFid for old name */
	if (error == ENOENT) {
#if 0
	    if (fid_old->fid.Volume == fid_new->fid.Volume &&
		fid_old->fid.Vnode == fid_new->fid.Vnode   &&
		fid_old->fid.Unique == fid_new->fid.Unique );
#endif
	    ReleaseWriteLock(&fce_new->lock);
	    
	    error = adir_lookup (&fce_new->fid, name_new, &foo_fid, NULL, &ce);
	    
#if 0
	    if (fid_old->fid.Volume == fid_new->fid.Volume &&
               fid_old->fid.Vnode == fid_new->fid.Vnode   &&
               fid_old->fid.Unique == fid_new->fid.Unique );
#endif
	    ObtainWriteLock (&fce_new->lock);
	    if (error == 0) /*Ba Si: We need delete the faked new */
		adir_remove(fce_new,name_new);
	    else if (error == ENOENT) {
                int venusret;
		
                venusret = find_venus (name_new,&foo_fid);
                assert (venusret==0);
                arla_warnx (ADEBDISCONN,"I MUST WRITE A PROGRAM HERE");
                if (isnewpar == 1) {

		    arla_warnx(ADEBDISCONN,"In reconnect_rename: "
			       "new Volume=0x%x,Vnode=0x%x,Unique=0x%x",
			       fce_new->fid.fid.Volume,
			       fce_new->fid.fid.Vnode,
			       fce_new->fid.fid.Unique);
#if 0
		    error = adir_creat(fce_new, name_new, foo_fid.fid);
#endif
		}
	    }
	}
    }

    arla_warnx (ADEBDISCONN,"reconnect_rename: we add the new one %s "
		"volumn=0x%x,vnode=0x%x,unique=0x%x",
		name_new,
		foo_fid.fid.Volume,
		foo_fid.fid.Vnode,
		foo_fid.fid.Unique);
    error = adir_creat (fce_new, name_new, foo_fid.fid);
    res = conv_dir (fce_new, ce, 0, &cache_handle, tmp, sizeof(tmp));

/* Aug 1 */

 out:

    free_fs_server_context (&context);

    ReleaseWriteLock(&fce_new->lock);

    if (fid_old->fid.Volume != fid_new->fid.Volume ||
	fid_old->fid.Vnode != fid_new->fid.Vnode   ||
	fid_old->fid.Unique != fid_new->fid.Unique )
        ReleaseWriteLock(&fce_old->lock); /* old and new are the same*/

    cred_free(ce);
    return error;
}

/*
 *
 */

int reconnect_create(struct vcache *parent, struct vcache *child, char *name)
{

    ConnCacheEntry *conn;
    fs_server_context context;
    VenusFid *parent_fid;
    VenusFid *child_fid;
    VenusFid fakeFid; 

    CredCacheEntry *ce;
    FCacheEntry *parentEntry;
    FCacheEntry *childEntry;

    AFSFetchStatus fetch_attr;
    AFSStoreStatus store_attr; 

    AFSFetchStatus status;
    AFSCallBack callback;
    AFSVolSync volsync;
    int ret; 
    char tmp[2 * sizeof(int) + 2];
    fcache_cache_handle cache_handle;
    int32_t type;

    parent_fid = &(parent->fid); /* points to the VenusFid structure */
    child_fid = &(child->fid);
    fakeFid = *child_fid;

    /*Ba Liu: the parent dir may be created during dison mode*/
    parent_fid = fid_translate(parent_fid);

    ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
    assert (ce != NULL);

    ret = fcache_find (&parentEntry, *parent_fid);
    assert (ret == 0);

#if 0
    is_change = reconnect_mut_chk(parentEntry, 
				  ce, 
				  parentEntry->status.DataVersion);
#endif

/*SWW Qiyue 30 delete the old file entry in dir  */
    arla_warnx (ADEBDISCONN,
		"reconnect_rename: we delete the old one volumn=0x%x, "
		"vnode=0x%x,unique=0x%x",
		parentEntry->fid.fid.Volume,
		parentEntry->fid.fid.Vnode,
		parentEntry->fid.fid.Unique);

    adir_remove(parentEntry,name);  

    conn = find_first_fs (parentEntry, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN, "find_first_fs failed");
	ReleaseWriteLock(&parentEntry->lock);
	ReleaseWriteLock(&childEntry->lock);
	free_fs_server_context(&context);
	cred_free(ce);
	return ENETDOWN;
    }

    ret = fcache_find (&childEntry, *child_fid);
    assert (ret == 0);

    recon_hashtabdel(childEntry);

#if 0
    fetch_attr = &childEntry->status;
#endif

    store_attr.Mask 	   =    8;
    store_attr.ClientModTime =    childEntry->status.ClientModTime;
    store_attr.Owner = 	 	childEntry->status.Owner;
    store_attr.Group = 		childEntry->status.Group;
    store_attr.UnixModeBits = 	childEntry->status.UnixModeBits;
    store_attr.SegSize = 		childEntry->status.SegSize;

    arla_log(ADEBDISCONN, 
	     "reconnect: create before RXAFS_CreateFile, "
	     "Cell=0x%x, fid.Volume= 0x%x, fid.Vnode=0x%x, fid.Unique=0x%x", 
	     childEntry->fid.Cell, 
	     childEntry->fid.fid.Volume, 
	     childEntry->fid.fid.Vnode, 
	     childEntry->fid.fid.Unique);

    alloc_fid_trans(&childEntry->fid);
  

    ret = RXAFS_CreateFile (conn->connection,
                            &(parentEntry->fid.fid),
                            name, &store_attr,
                            &(childEntry->fid.fid), &fetch_attr,
                            &status,
                            &callback,
                            &volsync);
 
    if (ret) {
	if (ret == 17) {
	    ReleaseWriteLock(&parentEntry->lock);
	    reconnect_mut_newfile(&childEntry, 
				  parent->cred.pag, 
				  &childEntry->fid);
	    ObtainWriteLock(&parentEntry->lock);
	    fill_fid_trans(&childEntry->fid);
	    recon_hashtabadd(childEntry);
	    childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
	    reconnect_update_fid (childEntry, fakeFid);
	} else {
	    arla_log (ADEBDISCONN, "Could not CreateFile: %s (%d)",
		      koerr_gettext(ret), ret);
	}
	goto out;
    }
   
    parentEntry->status   = status;   
    parentEntry->callback = callback;
    parentEntry->volsync  = volsync;

    childEntry->fid.Cell = parentEntry->fid.Cell;

    arla_log(ADEBDISCONN, "reconnect: create after RXAFS_CreateFile, "
	     "Cell=0x%x, fid.Volume= 0x%x, fid .Vnode=0x%x, fid.Unique=0x%x", 
	     childEntry->fid.Cell,
	     childEntry->fid.fid.Volume, 
	     childEntry->fid.fid.Vnode, 
	     childEntry->fid.fid.Unique); 

    fill_fid_trans(&childEntry->fid);

#if 0
    ReleaseWriteLock(&childEntry->lock);
#endif


    ret = volcache_getbyid (childEntry->fid.fid.Volume,
			    childEntry->fid.Cell,
			    ce,
			    &childEntry->volume,
			    &type);

    recon_hashtabadd(childEntry); 

    arla_log(ADEBDISCONN, 
	     "reconnect: create after volcache_getbyid, Cell=0x%x, "
	     "fid.Volume= 0x%x, fid .Vnode=0x%x, fid.Unique=0x%x",
	     childEntry->fid.Cell, 
	     childEntry->fid.fid.Volume, 
	     childEntry->fid.fid.Vnode, 
	     childEntry->fid.fid.Unique); 

/* SWW Qiyue 30: add the new file entry in dir */
    arla_warnx (ADEBDISCONN,"reconnect_rename: we add the new one "
		"volumn=0x%x,vnode=0x%x,unique=0x%x",
		parentEntry->fid.fid.Volume,
		parentEntry->fid.fid.Vnode,
		parentEntry->fid.fid.Unique);

    adir_creat (parentEntry, name, childEntry->fid.fid);  

    childEntry->status = fetch_attr;
  
    childEntry->flags.attrp = TRUE;
    childEntry->flags.kernelp = TRUE;

    childEntry->flags.datap = TRUE;
    childEntry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;

    if (parentEntry->volume == NULL)
	ret = volcache_getbyid (parentEntry->fid.fid.Volume,
				parentEntry->fid.Cell,
				ce,
				&parentEntry->volume,
				&type);

    volcache_update_volsync (parentEntry->volume, parentEntry->volsync);



/*SWW Qiyue 28: Set the host for child entry */

    childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
    assert(childEntry->host);

/*SWW Qiyue 29:  */
    arla_warnx (ADEBDISCONN,
		"Replace fid.Volume=0x%x,Vnode=0x%x,Unique=0x%x with "
		"Volume=0x%x,Vnode=0x%x,Unqiue=0x%x",
		fakeFid.fid.Volume,
		fakeFid.fid.Vnode,
		fakeFid.fid.Unique,
		childEntry->fid.fid.Volume,
		childEntry->fid.fid.Vnode,
		childEntry->fid.fid.Unique);

    reconnect_update_fid (childEntry, fakeFid);

    conv_dir(parentEntry, ce, 0, &cache_handle, tmp, sizeof(tmp));

    ReleaseWriteLock(&childEntry->lock);

 out:

    ReleaseWriteLock(&parentEntry->lock);
    ReleaseWriteLock(&childEntry->lock);
    free_fs_server_context(&context);
    cred_free(ce);
    return ret;
}

/*
 *
 */

int reconnect_mkdir(struct vcache *parent, struct vcache *curdir, 
                    AFSStoreStatus *store_status, char *name)
{
    ConnCacheEntry *conn; 
    fs_server_context context;
    CredCacheEntry *ce;
    VenusFid *parent_fid;
    VenusFid *child_fid;
    VenusFid fakeFid;

    FCacheEntry *parentEntry, *childEntry, *tempEntry, *tempparEntry;
    Result tempres;
    int    ret = 0;
    int    tempret = 0;
    struct timeval tv;
    char tmp[2 * sizeof(int) + 2];

    AFSFid  Outfid;   /* Ba Wu: These are used to get the info from FS*/
    AFSFetchStatus fetch_attr;
    AFSFetchStatus status;
    AFSCallBack  callback;
    AFSVolSync   volsync;
    fcache_cache_handle cache_handle;
    int32_t type;

    parent_fid = &(parent->fid); /* points to the VenusFid structure */
    child_fid = &(curdir->fid);
    fakeFid = *child_fid;

    parent_fid = fid_translate(parent_fid);

    ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
    assert (ce != NULL);

    ret = fcache_find (&parentEntry, *parent_fid);
    assert (ret == 0);

    tempparEntry = parentEntry;

/*Ba ba: used to check whether name can be find  Deleted !!!
  ReleaseWriteLock(&parentEntry->lock);
  tempret = adir_lookup (parentEntry->fid , name , &foo_fid , NULL, ce);  */
/*Ba ba: used to check whether name can be find  Deleted !!! */

    /*Ba Wu Remove the dir name from itsparent dir */
    tempret = adir_remove(parentEntry,name);  
    conn = find_first_fs (parentEntry, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN,"Cannot make this connection");
	ReleaseWriteLock(&parentEntry->lock);
	ReleaseWriteLock(&childEntry->lock);
	cred_free(ce);
	return ENETDOWN;
    }

    ret = fcache_find(&childEntry, *child_fid);/*Ba Wu: remove the newly created dir */
    assert (ret == 0);

    recon_hashtabdel(childEntry);

    alloc_fid_trans(&childEntry->fid);

    gettimeofday(&tv, NULL);

    ret = RXAFS_MakeDir (conn->connection,
			 &parentEntry->fid.fid,
			 name,
			 store_status, 
			 &Outfid,
			 &fetch_attr,
			 &status,
			 &callback,
			 &volsync);

    if (ret) {
	arla_log (ADEBDISCONN, "Could not CreateFile: %s (%d)",
		  koerr_gettext(ret), ret);
	goto out;
    }

    parentEntry->status   = status;
    parentEntry->callback = callback;
    parentEntry->callback.ExpirationTime += tv.tv_sec;
    parentEntry->volsync  = volsync;

    childEntry->fid.Cell = parentEntry->fid.Cell;
    childEntry->fid.fid = Outfid;
    childEntry->status = fetch_attr;
    childEntry->flags.attrp = TRUE;
    childEntry->flags.kernelp = TRUE;
    childEntry->flags.datap = TRUE;
    childEntry->tokens |= XFS_ATTR_R | XFS_DATA_R | XFS_DATA_W;

    fill_fid_trans(&childEntry->fid);

    ret = volcache_getbyid (childEntry->fid.fid.Volume,
			    childEntry->fid.Cell,
			    ce,
			    &childEntry->volume,
			    &type);

    recon_hashtabadd(childEntry);

/*Ba ba: Need to change later!!! */
#if 0
    ReleaseWriteLock(&tempparEntry->lock);
    tempret = adir_changefid (tempparEntry->fid ,name, &Outfid,  ce);
    ReleaseWriteLock(&tempparEntry->lock);
    tempret = adir_lookup (tempparEntry->fid ,name, &foo_fid, NULL, ce);
#endif

    tempret = adir_creat (parentEntry, name, childEntry->fid.fid); 

    childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
    assert(childEntry->host);

    reconnect_update_fid(childEntry, fakeFid);
     
    tempres = conv_dir(parentEntry, ce, 0, &cache_handle,
		       tmp, sizeof(tmp));

    ReleaseWriteLock(&childEntry->lock);

    /*SWW Qiyue 29: This should be deleted later */                      
    ret = fcache_find (&tempEntry, childEntry->fid);  

    assert (ret == 0);
    ReleaseWriteLock(&tempEntry->lock);

 out:

    ReleaseWriteLock(&parentEntry->lock);
    ReleaseWriteLock(&childEntry->lock);
    free_fs_server_context(&context);
    cred_free(ce);
    return ret;
}

/*
 *
 */

int reconnect_link(struct vcache *parent, struct vcache *existing,
		   char *name)
{
    ConnCacheEntry *conn;
    fs_server_context context;
    CredCacheEntry *ce;
    VenusFid *parent_fid;  
    VenusFid *existing_fid;
    char tmp[2 * sizeof(int) + 2];
    int ret = 0;
    FCacheEntry *dir_entry,*existing_entry;
    Result res;

    AFSFetchStatus new_status;
    AFSFetchStatus status;
    AFSVolSync volsync;
    fcache_cache_handle cache_handle;


    parent_fid = &(parent->fid);
    existing_fid = &(existing->fid);

    parent_fid = fid_translate(parent_fid);

    ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
    assert (ce != NULL);

    ret = fcache_find (&dir_entry, *parent_fid);
    assert (ret == 0);

    ret = fcache_find (&existing_entry, *existing_fid);
    assert (ret == 0);

    conn = find_first_fs (dir_entry, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN,"Cannot make this connection");
	ReleaseWriteLock(&dir_entry->lock);
	ReleaseWriteLock(&existing_entry->lock);
	cred_free(ce);
	return ENETDOWN;
    }

    ret = RXAFS_Link (conn->connection,
		      &dir_entry->fid.fid,
		      name,
		      &existing_entry->fid.fid,
		      &new_status,
		      &status,
		      &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "Link");
	goto out;
    }

    dir_entry->status  = status;
    dir_entry->volsync = volsync;

    existing_entry->status = new_status;
    
    volcache_update_volsync (dir_entry->volume, dir_entry->volsync);

    res = conv_dir (dir_entry, ce, 0, &cache_handle, tmp, sizeof(tmp));
	
 out:
    ReleaseWriteLock(&dir_entry->lock);
    ReleaseWriteLock(&existing_entry->lock);
    cred_free(ce);
    free_fs_server_context (&context);
    return ret;
}

/*
 *
 */

int reconnect_symlink(struct vcache *parent, struct vcache *child,
		      AFSStoreStatus *store_attr, char *name, 
		      char *contents)
{
    ConnCacheEntry *conn;
    fs_server_context context;
    CredCacheEntry *ce;
    VenusFid *parent_fid, *child_fid, fakeFid;
    char tmp[2 * sizeof(int) + 2];
    int ret = 0;
    FCacheEntry *dir_entry, *childEntry;
    Result res;

    AFSFetchStatus fetch_attr, new_status;
    AFSVolSync volsync;
    fcache_cache_handle cache_handle;
    int32_t type;

    parent_fid = &(parent->fid);
    child_fid  = &(child->fid);
    fakeFid    = *child_fid;
    parent_fid = fid_translate(parent_fid);

    ce = cred_get (parent->fid.Cell, parent->cred.pag, CRED_ANY);
    assert (ce != NULL);

    ret = fcache_find (&dir_entry, *parent_fid);
    assert (ret == 0);

    adir_remove(dir_entry,name);

    ret = fcache_find (&childEntry, *child_fid);
    assert (ret == 0);

    recon_hashtabdel(childEntry);

    assert(ret==0);
    conn = find_first_fs (dir_entry, ce, &context);
    if (conn == NULL) {
	arla_log (ADEBDISCONN,"Cannot make this connection");
	ReleaseWriteLock(&dir_entry->lock);
	ReleaseWriteLock(&childEntry->lock);
	cred_free(ce);
	return ENOMEM;
    }
  
    alloc_fid_trans(&childEntry->fid);

    ret = RXAFS_Symlink (conn->connection,
			 &dir_entry->fid.fid,
			 name,
			 contents,
			 store_attr,
			 &(childEntry->fid.fid),
			 &fetch_attr,
			 &new_status,
			 &volsync);
    if (ret) {
	arla_warn (ADEBFCACHE, ret, "Symlink");
	goto out;
    }

    child_fid->Cell = dir_entry->fid.Cell;

    fill_fid_trans (&childEntry->fid);
    ret = volcache_getbyid (childEntry->fid.fid.Volume,
			    childEntry->fid.Cell,
			    ce,
			    &childEntry->volume,
			    &type);

    recon_hashtabadd (childEntry);

    adir_creat(dir_entry, name, childEntry->fid.fid);

    childEntry->status = fetch_attr;
    childEntry->flags.attrp = TRUE;
    childEntry->flags.kernelp = TRUE;
    childEntry->tokens |= XFS_ATTR_R;
    volcache_update_volsync (dir_entry->volume, dir_entry->volsync);

    childEntry->host = rx_HostOf (rx_PeerOf (conn->connection));
    assert(childEntry->host);

    reconnect_update_fid(childEntry, fakeFid);
    res = conv_dir (dir_entry, ce, 0, &cache_handle, tmp, sizeof(tmp));

 out: 
    ReleaseWriteLock(&dir_entry->lock);
    ReleaseWriteLock(&childEntry->lock);
    free_fs_server_context(&context);
    cred_free(ce);
    return ret;
}

#endif
