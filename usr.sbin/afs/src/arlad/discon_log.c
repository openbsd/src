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
 * This file contains functions that relate to performance statistics
 * for disconnected operation.
 */

#include "arla_local.h"

RCSID("$KTH: discon_log.c,v 1.8.40.1 2001/06/04 22:16:35 ahltorp Exp $");

extern int dlog_mod;
extern long current_op_no;

extern DARLA_file log_data;

extern long  Log_is_open;

log_ent_t   log_head;

int DARLA_Open(DARLA_file *Dfp, char *fname, int oflag);
int DARLA_Read(DARLA_file *Dfp, char *cp, int len);
int DARLA_Write(DARLA_file *Dfp, char *cp, int len);
int DARLA_Seek(DARLA_file *Dfp, int offset, int whence);

#if 0
/*
 * read an entry from the log file described by tfile.  The result is
 * put into the log_ent data.  This return 0 if successful, 1 if
 * it failed to some reason ( ie. no more data ).
 */

int
read_log_ent(DARLA_file * tfile, log_ent_t *in_log)
{
    int len;
    char *bp;

    if (DARLA_Read(tfile, (char *) in_log, sizeof (int)) != sizeof(int))
	return 1;

    len = in_log->log_len - sizeof(int);
    bp = (char *) in_log + sizeof(int);

    if (DARLA_Read(tfile, bp, len) != len) {
	printf("read_log_ent: short read \n");
	return 1;
    }
    return 0;
}

void
update_log_ent(offset, flags)
long offset;
int flags;
{
    struct DARLA_file *tfile;
    log_ent_t *log_ent;
    int code;

    tfile = DARLA_UFSOpen(&log_data.bs_dev, log_data.bs_inode);
    if (!tfile)
	panic("update_log_ent: failed to open log file");

    DARLA_Seek(tfile, offset);

    log_ent = (log_ent_t *) malloc(sizeof(log_ent_t));
    code = read_log_ent(tfile, log_ent);

    if (code) {
	printf("update_log_ent: failed to read log entry at %d \n",
	       offset);
    } else {

	/* set the log flags */
	log_ent->log_flags |= flags;

	/* write the entry back out */
	DARLA_Seek(tfile, offset);
	DARLA_Write(tfile, (char *) log_ent, log_ent->log_len);
    }
    free(log_ent);
    DARLA_Close(tfile);
}
#endif


/* write the log entries to disk */
static long
write_log_ent(int len, log_ent_t *log)
{

    long new_num;
    static int index=0;

    arla_warnx (ADEBDISCONN,"We are writing a log");
    if (!Log_is_open) {
	return -1;
    }

    if (log_head.next == 0) { 
	log_head.next = (log_ent_t *) malloc(sizeof(log_ent_t));
	*log_head.next = *log;
	log_head.next->next = 0;
    }
    else {
        log->next = log_head.next;
        log_head.next = (log_ent_t *) malloc(sizeof(log_ent_t));
        *log_head.next = *log;
    }

    ObtainWriteLock(&log_data.bs_lock);

    new_num = 0;
  
    log->log_opno = new_num;
    gettimeofday(&log->log_time, 0);

    log->log_offset = log_data.offset;
    log->log_flags = 0;
    log->log_index = index++;

    DARLA_Write(&log_data, (char *) log, len);

    ReleaseWriteLock(&log_data.bs_lock);

    return (new_num);
}

long
log_dis_store(struct vcache *avc)
{
    log_ent_t	*store;
    long	op_no;
    
    store = (log_ent_t *) malloc(sizeof(log_ent_t));
    
    store->log_op = DIS_STORE;
    store->st_fid = avc->fid;
    store->st_origdv =  avc->DataVersion;
    store->st_flag = avc->flag;
    
    /* have to log cred as well */
    store->cred = avc->cred;
    
    /* figure out the length of a store entry */
    store->log_len = ((char *) &(store->st_origdv)) - ((char *) store)
	+ sizeof(store->st_origdv);
    
    op_no = write_log_ent(store->log_len, store);
    
    free(store);
    return op_no;
}


/* Log a mkdir operation */
long
log_dis_mkdir(struct vcache *pvc, struct vcache *dvc, 
	      AFSStoreStatus *attrs, char *name)
{
    log_ent_t	*mkdir;
    long	op_no;
    
    mkdir = (log_ent_t *) malloc(sizeof(log_ent_t));
    
    mkdir->log_op = DIS_MKDIR;
    mkdir->md_dirfid = dvc->fid;
    /*Ba Wu: the data vers. for child dir*/
    mkdir->md_dversion = dvc->DataVersion; 
    mkdir->md_parentfid = pvc->fid;
    mkdir->cred = pvc->cred;
    mkdir->md_vattr = *attrs;
    
    /* save the name */
    strlcpy((char *) mkdir->md_name, name, sizeof mkdir->md_name);
    
    /* calculate the length of this record */
    mkdir->log_len = ((char *) mkdir->md_name - (char *) mkdir)
	+ strlen(name) + 1;
    
    op_no = write_log_ent(mkdir->log_len, mkdir);
    
    free(mkdir);
    return op_no;
}


long
log_dis_create(struct vcache *parent, struct vcache *child, char *name)
{
    log_ent_t *create;
    long	op_no;
    struct vcache *ch;
    struct vcache *par;
    
    ch = child;
    par = parent;
    ch->DataVersion = child->DataVersion;
    
    create = (log_ent_t *) malloc(sizeof(log_ent_t));
    
    create->log_op = DIS_CREATE;
    create->cr_filefid = ch->fid;
    create->cr_parentfid = par->fid;
    create->cr_origdv = ch->DataVersion;
    create->cred = parent->cred;
    
    strlcpy((char *) create->cr_name, name, sizeof create->cr_name);
    
    create->log_len = ((char *) create->cr_name - (char *) create) +
	strlen(name) + 1;
    
    op_no = write_log_ent(create->log_len, create);
    
    free(create);
    return op_no;
}

#if 0
long
log_dis_remove(struct vcache *avc, FCacheEntry *childentry, char *name)
{
    log_ent_t	*remove;
    long	op_no;
    remove = (log_ent_t *) malloc(sizeof(log_ent_t));
    
    remove->log_op = DIS_REMOVE;
    remove->cred = avc->cred;
    remove->rm_filefid = avc->fid;
    remove->rm_origdv = avc->DataVersion;
    remove->rm_chentry = childentry;
    
    strlcpy((char *) remove->rm_name, name, sizeof remove->rm_name);
    
    remove->log_len = ((char *) remove->rm_name - (char *) remove) +
	strlen(name) + 1;
    
    op_no = write_log_ent(remove->log_len, remove);
    
    free(remove);
    arla_log(ADEBDISCONN, "remove: fid.Cell=%ld, fid.fid.Volume=%ld, "
	     "fid.Unique=%ld", \
	     remove->rm_filefid.Cell, 
	     remove->rm_filefid.fid.Volume,
	     remove->rm_filefid.fid.Unique);              
    
    return op_no;
}


long
log_dis_rmdir(struct vcache *dir, FCacheEntry *cce, const char *name)
{
    log_ent_t	*rmdir;
    long	op_no;
    
    rmdir = malloc(sizeof(log_ent_t));
    
    rmdir->log_op = DIS_RMDIR;
    rmdir->cred = dir->cred;
    rmdir->rd_parentfid = dir->fid;
    rmdir->rd_direntry = cce;
    
    strlcpy((char *) rmdir->rd_name, name, sizeof rmdir->rd_name);
    
    rmdir->log_len = ((char *) rmdir->rd_name - (char *) rmdir) +
	strlen(name) + 1;
    
    op_no = write_log_ent(rmdir->log_len, rmdir);
    
    free(rmdir);
    return op_no;
}


long
log_dis_rename(struct vcache *old_dir, char *old_name, 
	       struct vcache *new_dir, char *new_name)
{
    log_ent_t	*rename;
    char *cp;
    
    rename = malloc(sizeof(log_ent_t));
    
    rename->log_op = DIS_RENAME;
    rename->rn_oparentfid = old_dir->fid;
    rename->rn_nparentfid = new_dir->fid;
    rename->rn_origdv = old_dir->DataVersion;
    rename->rn_overdv = new_dir->DataVersion;
    rename->cred = old_dir->cred;
    
    strlcpy((char *) rename->rn_names, old_name, MAX_NAME);
    cp = (char *) rename->rn_names + strlen(old_name) + 1;
    
    strlcpy((char *) cp, new_name, MAX_NAME);
    cp += strlen(new_name) + 1;
    
    rename->log_len = (char *) cp - (char *) rename;
    
    write_log_ent(rename->log_len, rename);
    
    free(rename);
    return 0;
}



/* Log a link operation */
long
log_dis_link(struct vcache *pvc, struct vcache *lvc, char *name)

{
    log_ent_t *link;
    long	op_no;
    
    link = malloc(sizeof(log_ent_t));

    link->log_op = DIS_LINK;
    link->cred   = pvc->cred;
    link->ln_linkfid = lvc->fid;
    link->ln_parentfid = pvc->fid;
    
    /* save the name */
    strlcpy((char *) link->ln_name, name, sizeof link->ln_name);
    /* calculate the length of this record */
    link->log_len = ((char *) link->ln_name - (char *) link) +
	strlen(name) + 1;
    
    op_no = write_log_ent(link->log_len, link);
    
    free(link);
    return op_no;
}

/* Log a symlink operation */
long
log_dis_symlink(struct vcache *pvc, struct vcache *cvc, 
		AFSStoreStatus *attr, char *linkname, char *content)
{
    log_ent_t *slink;
    long op_no;
    
    slink = malloc(sizeof(log_ent_t));
    
    slink->log_op = DIS_SYMLINK;
    slink->sy_parentfid = pvc->fid;
    slink->sy_filefid = cvc->fid;
    slink->sy_attr = *attr;
    slink->cred = pvc->cred;
    
    /* copy in the link name */
    strlcpy((char *) slink->sy_name, linkname, sizeof slink->sy_name);
    strlcpy((char *) slink->sy_content, content, sizeof slink->sy_content);
    
    /* calculate the length of this record */
    slink->log_len = ( (char *) slink->sy_content -
		       (char *) slink) + 
	strlen(content) + 1;
    
    op_no = write_log_ent(slink->log_len, slink);
    
    free(slink);
    return op_no;
}
#endif

/* Log a setattr operation */
long
log_dis_setattr(struct vcache *tvc, struct xfs_attr *attrs)
{
    log_ent_t	*setattr;
    long op_no;
    
    setattr = (log_ent_t *) malloc(sizeof(log_ent_t));
    
    setattr->log_op = DIS_SETATTR;
    setattr->sa_fid = tvc->fid;
    setattr->cred = tvc->cred;
    setattr->sa_origdv = tvc->DataVersion;
    
    setattr->sa_vattr = *attrs;
    
    /* calculate the length of this record */
    setattr->log_len = ((char *) &setattr->sa_origdv - (char *) setattr) +
	sizeof(setattr->sa_origdv);
    
    op_no = write_log_ent(setattr->log_len, setattr);
    
    arla_log(ADEBDISCONN, "log_dis_setattr: fid.Cell=0x%x, fid.fid.Volume=0x%x,"
	     "fid.fid.Vnode=0x%x, fid.fid.Unique=0x%x", 
	     tvc->fid.Cell, 
	     tvc->fid.fid.Volume, 
	     tvc->fid.fid.Vnode, 
	     tvc->fid.fid.Unique);

    arla_log(ADEBDISCONN, "log_dis_setattr: writing %d byte log entry.");
    
    free(setattr);
    return op_no;
}

#if 0
long
log_dis_nonmutating(struct vcache *tvc, log_ops_t op)
{
#ifdef LOGNONMUTE
    log_ent_t	*non_mute;
    long	op_no;

    non_mute = (log_ent_t *) malloc(sizeof(log_ent_t));

    non_mute->log_op = op;
    non_mute->cred = tvc->cred;
    non_mute->nm_fid = tvc->fid;
    non_mute->nm_origdv =  tvc->DataVersion;
    non_mute->log_len = ((char *) &non_mute->nm_origdv -
			 (char *) non_mute) + sizeof(non_mute->nm_origdv);

    /* XXX lhuston removed for debugging */
    op_no = write_log_ent(non_mute->log_len, non_mute);

    free(non_mute);
    return op_no;
#else
    return 0; /*   0 was current_op_no */
#endif
}


long
log_dis_access(struct vcache *tvc)
{
    return log_dis_nonmutating(tvc, DIS_ACCESS);
}

long
log_dis_readdir(struct vcache *tvc)
{
    return log_dis_nonmutating(tvc, DIS_READDIR);
}

long
log_dis_readlink(struct vcache *tvc)
{
    return log_dis_nonmutating(tvc, DIS_READLINK);
}

long
log_dis_fsync(struct vcache *tvc)
{
    /* treat an fsync as a store */
    return log_dis_nonmutating(tvc, DIS_FSYNC);
}
#endif
