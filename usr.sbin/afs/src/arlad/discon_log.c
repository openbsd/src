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
OBSOLETE  * This file contains functions that relate to performance statistics
OBSOLETE  * for disconnected operation.
OBSOLETE  */
OBSOLETE 
OBSOLETE #include "arla_local.h"
OBSOLETE 
OBSOLETE RCSID("$arla: discon_log.c,v 1.12 2002/09/07 10:43:05 lha Exp $");
OBSOLETE 
OBSOLETE int Log_is_open;
OBSOLETE DARLA_file log_data;
OBSOLETE 
OBSOLETE log_ent_t   log_head;
OBSOLETE 
OBSOLETE #if 0
OBSOLETE /*
OBSOLETE  * read an entry from the log file described by tfile.  The result is
OBSOLETE  * put into the log_ent data.  This return 0 if successful, 1 if
OBSOLETE  * it failed to some reason ( ie. no more data ).
OBSOLETE  */
OBSOLETE 
OBSOLETE int
OBSOLETE read_log_ent(DARLA_file * tfile, log_ent_t *in_log)
OBSOLETE {
OBSOLETE     int len;
OBSOLETE     char *bp;
OBSOLETE 
OBSOLETE     if (DARLA_Read(tfile, (char *) in_log, sizeof (int)) != sizeof(int))
OBSOLETE 	return 1;
OBSOLETE 
OBSOLETE     len = in_log->log_len - sizeof(int);
OBSOLETE     bp = (char *) in_log + sizeof(int);
OBSOLETE 
OBSOLETE     if (DARLA_Read(tfile, bp, len) != len) {
OBSOLETE 	printf("read_log_ent: short read \n");
OBSOLETE 	return 1;
OBSOLETE     }
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE void
OBSOLETE update_log_ent(offset, flags)
OBSOLETE long offset;
OBSOLETE int flags;
OBSOLETE {
OBSOLETE     struct DARLA_file *tfile;
OBSOLETE     log_ent_t *log_ent;
OBSOLETE     int code;
OBSOLETE 
OBSOLETE     tfile = DARLA_UFSOpen(&log_data.bs_dev, log_data.bs_inode);
OBSOLETE     if (!tfile)
OBSOLETE 	panic("update_log_ent: failed to open log file");
OBSOLETE 
OBSOLETE     DARLA_Seek(tfile, offset);
OBSOLETE 
OBSOLETE     log_ent = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     code = read_log_ent(tfile, log_ent);
OBSOLETE 
OBSOLETE     if (code) {
OBSOLETE 	printf("update_log_ent: failed to read log entry at %d \n",
OBSOLETE 	       offset);
OBSOLETE     } else {
OBSOLETE 
OBSOLETE 	/* set the log flags */
OBSOLETE 	log_ent->log_flags |= flags;
OBSOLETE 
OBSOLETE 	/* write the entry back out */
OBSOLETE 	DARLA_Seek(tfile, offset);
OBSOLETE 	DARLA_Write(tfile, (char *) log_ent, log_ent->log_len);
OBSOLETE     }
OBSOLETE     free(log_ent);
OBSOLETE     DARLA_Close(tfile);
OBSOLETE }
OBSOLETE #endif
OBSOLETE 
OBSOLETE 
OBSOLETE /* write the log entries to disk */
OBSOLETE static long
OBSOLETE write_log_ent(int len, log_ent_t *log)
OBSOLETE {
OBSOLETE 
OBSOLETE     long new_num;
OBSOLETE     static int index=0;
OBSOLETE 
OBSOLETE     arla_warnx (ADEBDISCONN,"We are writing a log");
OBSOLETE     if (!Log_is_open) {
OBSOLETE 	return -1;
OBSOLETE     }
OBSOLETE 
OBSOLETE     if (log_head.next == 0) { 
OBSOLETE 	log_head.next = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE 	*log_head.next = *log;
OBSOLETE 	log_head.next->next = 0;
OBSOLETE     }
OBSOLETE     else {
OBSOLETE         log->next = log_head.next;
OBSOLETE         log_head.next = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE         *log_head.next = *log;
OBSOLETE     }
OBSOLETE 
OBSOLETE     ObtainWriteLock(&log_data.bs_lock);
OBSOLETE 
OBSOLETE     new_num = 0;
OBSOLETE   
OBSOLETE     log->log_opno = new_num;
OBSOLETE     gettimeofday(&log->log_time, 0);
OBSOLETE 
OBSOLETE     log->log_offset = log_data.offset;
OBSOLETE     log->log_flags = 0;
OBSOLETE     log->log_index = index++;
OBSOLETE 
OBSOLETE     DARLA_Write(&log_data, (char *) log, len);
OBSOLETE 
OBSOLETE     ReleaseWriteLock(&log_data.bs_lock);
OBSOLETE 
OBSOLETE     return (new_num);
OBSOLETE }
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_store(struct vcache *avc)
OBSOLETE {
OBSOLETE     log_ent_t	*store;
OBSOLETE     long	op_no;
OBSOLETE     
OBSOLETE     store = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     store->log_op = DIS_STORE;
OBSOLETE     store->st_fid = avc->fid;
OBSOLETE     store->st_origdv =  avc->DataVersion;
OBSOLETE     store->st_flag = avc->flag;
OBSOLETE     
OBSOLETE     /* have to log cred as well */
OBSOLETE     store->cred = avc->cred;
OBSOLETE     
OBSOLETE     /* figure out the length of a store entry */
OBSOLETE     store->log_len = ((char *) &(store->st_origdv)) - ((char *) store)
OBSOLETE 	+ sizeof(store->st_origdv);
OBSOLETE     
OBSOLETE     op_no = write_log_ent(store->log_len, store);
OBSOLETE     
OBSOLETE     free(store);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE /* Log a mkdir operation */
OBSOLETE long
OBSOLETE log_dis_mkdir(struct vcache *pvc, struct vcache *dvc, 
OBSOLETE 	      AFSStoreStatus *attrs, char *name)
OBSOLETE {
OBSOLETE     log_ent_t	*mkdir;
OBSOLETE     long	op_no;
OBSOLETE     
OBSOLETE     mkdir = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     mkdir->log_op = DIS_MKDIR;
OBSOLETE     mkdir->md_dirfid = dvc->fid;
OBSOLETE     /*Ba Wu: the data vers. for child dir*/
OBSOLETE     mkdir->md_dversion = dvc->DataVersion; 
OBSOLETE     mkdir->md_parentfid = pvc->fid;
OBSOLETE     mkdir->cred = pvc->cred;
OBSOLETE     mkdir->md_vattr = *attrs;
OBSOLETE     
OBSOLETE     /* save the name */
OBSOLETE     strcpy((char *) mkdir->md_name, name);
OBSOLETE     
OBSOLETE     /* calculate the length of this record */
OBSOLETE     mkdir->log_len = ((char *) mkdir->md_name - (char *) mkdir)
OBSOLETE 	+ strlen(name) + 1;
OBSOLETE     
OBSOLETE     op_no = write_log_ent(mkdir->log_len, mkdir);
OBSOLETE     
OBSOLETE     free(mkdir);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_create(struct vcache *parent, struct vcache *child, char *name)
OBSOLETE {
OBSOLETE     log_ent_t *create;
OBSOLETE     long	op_no;
OBSOLETE     struct vcache *ch;
OBSOLETE     struct vcache *par;
OBSOLETE     
OBSOLETE     ch = child;
OBSOLETE     par = parent;
OBSOLETE     ch->DataVersion = child->DataVersion;
OBSOLETE     
OBSOLETE     create = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     create->log_op = DIS_CREATE;
OBSOLETE     create->cr_filefid = ch->fid;
OBSOLETE     create->cr_parentfid = par->fid;
OBSOLETE     create->cr_origdv = ch->DataVersion;
OBSOLETE     create->cred = parent->cred;
OBSOLETE     
OBSOLETE     strcpy((char *) create->cr_name, name);
OBSOLETE     
OBSOLETE     create->log_len = ((char *) create->cr_name - (char *) create) +
OBSOLETE 	strlen(name) + 1;
OBSOLETE     
OBSOLETE     op_no = write_log_ent(create->log_len, create);
OBSOLETE     
OBSOLETE     free(create);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE #if 0
OBSOLETE long
OBSOLETE log_dis_remove(struct vcache *avc, FCacheEntry *childentry, char *name)
OBSOLETE {
OBSOLETE     log_ent_t	*remove;
OBSOLETE     long	op_no;
OBSOLETE     remove = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     remove->log_op = DIS_REMOVE;
OBSOLETE     remove->cred = avc->cred;
OBSOLETE     remove->rm_filefid = avc->fid;
OBSOLETE     remove->rm_origdv = avc->DataVersion;
OBSOLETE     remove->rm_chentry = childentry;
OBSOLETE     
OBSOLETE     strcpy((char *) remove->rm_name, name);
OBSOLETE     
OBSOLETE     remove->log_len = ((char *) remove->rm_name - (char *) remove) +
OBSOLETE 	strlen(name) + 1;
OBSOLETE     
OBSOLETE     op_no = write_log_ent(remove->log_len, remove);
OBSOLETE     
OBSOLETE     free(remove);
OBSOLETE     arla_log(ADEBDISCONN, "remove: fid.Cell=%ld, fid.fid.Volume=%ld, "
OBSOLETE 	     "fid.Unique=%ld", \
OBSOLETE 	     remove->rm_filefid.Cell, 
OBSOLETE 	     remove->rm_filefid.fid.Volume,
OBSOLETE 	     remove->rm_filefid.fid.Unique);              
OBSOLETE     
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_rmdir(struct vcache *dir, FCacheEntry *cce, const char *name)
OBSOLETE {
OBSOLETE     log_ent_t	*rmdir;
OBSOLETE     long	op_no;
OBSOLETE     
OBSOLETE     rmdir = malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     rmdir->log_op = DIS_RMDIR;
OBSOLETE     rmdir->cred = dir->cred;
OBSOLETE     rmdir->rd_parentfid = dir->fid;
OBSOLETE     rmdir->rd_direntry = cce;
OBSOLETE     
OBSOLETE     strcpy((char *) rmdir->rd_name, name);
OBSOLETE     
OBSOLETE     rmdir->log_len = ((char *) rmdir->rd_name - (char *) rmdir) +
OBSOLETE 	strlen(name) + 1;
OBSOLETE     
OBSOLETE     op_no = write_log_ent(rmdir->log_len, rmdir);
OBSOLETE     
OBSOLETE     free(rmdir);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_rename(struct vcache *old_dir, char *old_name, 
OBSOLETE 	       struct vcache *new_dir, char *new_name)
OBSOLETE {
OBSOLETE     log_ent_t	*rename;
OBSOLETE     char *cp;
OBSOLETE     
OBSOLETE     rename = malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     rename->log_op = DIS_RENAME;
OBSOLETE     rename->rn_oparentfid = old_dir->fid;
OBSOLETE     rename->rn_nparentfid = new_dir->fid;
OBSOLETE     rename->rn_origdv = old_dir->DataVersion;
OBSOLETE     rename->rn_overdv = new_dir->DataVersion;
OBSOLETE     rename->cred = old_dir->cred;
OBSOLETE     
OBSOLETE     strcpy((char *) rename->rn_names, old_name);
OBSOLETE     cp = (char *) rename->rn_names + strlen(old_name) + 1;
OBSOLETE     
OBSOLETE     strcpy((char *) cp, new_name);
OBSOLETE     cp += strlen(new_name) + 1;
OBSOLETE     
OBSOLETE     rename->log_len = (char *) cp - (char *) rename;
OBSOLETE     
OBSOLETE     write_log_ent(rename->log_len, rename);
OBSOLETE     
OBSOLETE     free(rename);
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE /* Log a link operation */
OBSOLETE long
OBSOLETE log_dis_link(struct vcache *pvc, struct vcache *lvc, char *name)
OBSOLETE 
OBSOLETE {
OBSOLETE     log_ent_t *link;
OBSOLETE     long	op_no;
OBSOLETE     
OBSOLETE     link = malloc(sizeof(log_ent_t));
OBSOLETE 
OBSOLETE     link->log_op = DIS_LINK;
OBSOLETE     link->cred   = pvc->cred;
OBSOLETE     link->ln_linkfid = lvc->fid;
OBSOLETE     link->ln_parentfid = pvc->fid;
OBSOLETE     
OBSOLETE     /* save the name */
OBSOLETE     strcpy((char *) link->ln_name, name);
OBSOLETE     /* calculate the length of this record */
OBSOLETE     link->log_len = ((char *) link->ln_name - (char *) link) +
OBSOLETE 	strlen(name) + 1;
OBSOLETE     
OBSOLETE     op_no = write_log_ent(link->log_len, link);
OBSOLETE     
OBSOLETE     free(link);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE /* Log a symlink operation */
OBSOLETE long
OBSOLETE log_dis_symlink(struct vcache *pvc, struct vcache *cvc, 
OBSOLETE 		AFSStoreStatus *attr, char *linkname, char *content)
OBSOLETE {
OBSOLETE     log_ent_t *slink;
OBSOLETE     long op_no;
OBSOLETE     
OBSOLETE     slink = malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     slink->log_op = DIS_SYMLINK;
OBSOLETE     slink->sy_parentfid = pvc->fid;
OBSOLETE     slink->sy_filefid = cvc->fid;
OBSOLETE     slink->sy_attr = *attr;
OBSOLETE     slink->cred = pvc->cred;
OBSOLETE     
OBSOLETE     /* copy in the link name */
OBSOLETE     strcpy((char *) slink->sy_name, linkname);
OBSOLETE     strcpy((char *) slink->sy_content, content);
OBSOLETE     
OBSOLETE     /* calculate the length of this record */
OBSOLETE     slink->log_len = ( (char *) slink->sy_content -
OBSOLETE 		       (char *) slink) + 
OBSOLETE 	strlen(content) + 1;
OBSOLETE     
OBSOLETE     op_no = write_log_ent(slink->log_len, slink);
OBSOLETE     
OBSOLETE     free(slink);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE #endif
OBSOLETE 
OBSOLETE /* Log a setattr operation */
OBSOLETE long
OBSOLETE log_dis_setattr(struct vcache *tvc, struct nnpfs_attr *attrs)
OBSOLETE {
OBSOLETE     log_ent_t	*setattr;
OBSOLETE     long op_no;
OBSOLETE     
OBSOLETE     setattr = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE     
OBSOLETE     setattr->log_op = DIS_SETATTR;
OBSOLETE     setattr->sa_fid = tvc->fid;
OBSOLETE     setattr->cred = tvc->cred;
OBSOLETE     setattr->sa_origdv = tvc->DataVersion;
OBSOLETE     
OBSOLETE     setattr->sa_vattr = *attrs;
OBSOLETE     
OBSOLETE     /* calculate the length of this record */
OBSOLETE     setattr->log_len = ((char *) &setattr->sa_origdv - (char *) setattr) +
OBSOLETE 	sizeof(setattr->sa_origdv);
OBSOLETE     
OBSOLETE     op_no = write_log_ent(setattr->log_len, setattr);
OBSOLETE     
OBSOLETE     arla_log(ADEBDISCONN, "log_dis_setattr: fid.Cell=0x%x, fid.fid.Volume=0x%x,"
OBSOLETE 	     "fid.fid.Vnode=0x%x, fid.fid.Unique=0x%x", 
OBSOLETE 	     tvc->fid.Cell, 
OBSOLETE 	     tvc->fid.fid.Volume, 
OBSOLETE 	     tvc->fid.fid.Vnode, 
OBSOLETE 	     tvc->fid.fid.Unique);
OBSOLETE 
OBSOLETE     arla_log(ADEBDISCONN, "log_dis_setattr: writing %d byte log entry.");
OBSOLETE     
OBSOLETE     free(setattr);
OBSOLETE     return op_no;
OBSOLETE }
OBSOLETE 
OBSOLETE #if 0
OBSOLETE long
OBSOLETE log_dis_nonmutating(struct vcache *tvc, log_ops_t op)
OBSOLETE {
OBSOLETE #ifdef LOGNONMUTE
OBSOLETE     log_ent_t	*non_mute;
OBSOLETE     long	op_no;
OBSOLETE 
OBSOLETE     non_mute = (log_ent_t *) malloc(sizeof(log_ent_t));
OBSOLETE 
OBSOLETE     non_mute->log_op = op;
OBSOLETE     non_mute->cred = tvc->cred;
OBSOLETE     non_mute->nm_fid = tvc->fid;
OBSOLETE     non_mute->nm_origdv =  tvc->DataVersion;
OBSOLETE     non_mute->log_len = ((char *) &non_mute->nm_origdv -
OBSOLETE 			 (char *) non_mute) + sizeof(non_mute->nm_origdv);
OBSOLETE 
OBSOLETE     /* XXX lhuston removed for debugging */
OBSOLETE     op_no = write_log_ent(non_mute->log_len, non_mute);
OBSOLETE 
OBSOLETE     free(non_mute);
OBSOLETE     return op_no;
OBSOLETE #else
OBSOLETE     return 0; /*   0 was current_op_no */
OBSOLETE #endif
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_access(struct vcache *tvc)
OBSOLETE {
OBSOLETE     return log_dis_nonmutating(tvc, DIS_ACCESS);
OBSOLETE }
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_readdir(struct vcache *tvc)
OBSOLETE {
OBSOLETE     return log_dis_nonmutating(tvc, DIS_READDIR);
OBSOLETE }
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_readlink(struct vcache *tvc)
OBSOLETE {
OBSOLETE     return log_dis_nonmutating(tvc, DIS_READLINK);
OBSOLETE }
OBSOLETE 
OBSOLETE long
OBSOLETE log_dis_fsync(struct vcache *tvc)
OBSOLETE {
OBSOLETE     /* treat an fsync as a store */
OBSOLETE     return log_dis_nonmutating(tvc, DIS_FSYNC);
OBSOLETE }
OBSOLETE #endif
