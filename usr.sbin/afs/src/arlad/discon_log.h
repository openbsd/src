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
OBSOLETE  * This file contains all the relevant information pertaining to logging
OBSOLETE  * for disconnected afs.
OBSOLETE  */
OBSOLETE /* replace vattr with nnpfs_attr in the log */
OBSOLETE #include <nnpfs/nnpfs_attr.h>
OBSOLETE #ifndef  _DISCONN_LOG_H
OBSOLETE 
OBSOLETE #define _DISCONN_LOG_H
OBSOLETE 
OBSOLETE #define BUFSIZE 256
OBSOLETE 
OBSOLETE #define MAX_NAME AFSNAMEMAX
OBSOLETE 
OBSOLETE enum log_ops {
OBSOLETE 	DIS_STORE,
OBSOLETE 	DIS_MKDIR,
OBSOLETE 	DIS_CREATE,
OBSOLETE 	DIS_REMOVE,
OBSOLETE 	DIS_RMDIR,
OBSOLETE 	DIS_RENAME,
OBSOLETE 	DIS_LINK,
OBSOLETE 	DIS_SYMLINK,
OBSOLETE 	DIS_SETATTR,
OBSOLETE 	DIS_FSYNC,
OBSOLETE 	DIS_ACCESS,
OBSOLETE 	DIS_READDIR,
OBSOLETE 	DIS_READLINK,
OBSOLETE 	DIS_INFO,
OBSOLETE 	DIS_START_OPT,
OBSOLETE 	DIS_END_OPT,
OBSOLETE 	DIS_REPLAYED
OBSOLETE };
OBSOLETE 
OBSOLETE typedef enum log_ops log_ops_t;
OBSOLETE 
OBSOLETE /* These are defines for the different log flags that can be set */
OBSOLETE #define       LOG_INACTIVE    0x1     /* log no longer needs replay */
OBSOLETE #define       LOG_REPLAYED    0x2     /* log entry was replayed */
OBSOLETE #define       LOG_OPTIMIZED   0x4     /* log entry was optimized away */
OBSOLETE #define       LOG_GENERATED   0x8     /* log entry created by optimizer */
OBSOLETE 
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	st_fid		log_data.st.fid
OBSOLETE #define	st_origdv	log_data.st.origdv
OBSOLETE 
OBSOLETE /* added flag option */
OBSOLETE #define st_flag		log_data.st.flag
OBSOLETE 
OBSOLETE typedef uint32_t hyper;
OBSOLETE 
OBSOLETE /* a really stripped down version of CITI's vcache */
OBSOLETE struct vcache {
OBSOLETE 	uint32_t	DataVersion;
OBSOLETE 	struct VenusFid fid;
OBSOLETE         nnpfs_cred 	cred;
OBSOLETE         u_int           flag;           /* write flag */
OBSOLETE };
OBSOLETE 
OBSOLETE typedef	struct store_log_data {
OBSOLETE 	struct VenusFid	fid;		/* fid of the file */
OBSOLETE 	u_int flag;		/* write flag */
OBSOLETE     	hyper 		origdv;	/* cached data version of file */
OBSOLETE } store_log_data_t;
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	md_dirfid		log_data.md.dirfid
OBSOLETE #define	md_parentfid		log_data.md.parentfid
OBSOLETE #define	md_vattr		log_data.md.vattr
OBSOLETE #define	md_dversion		log_data.md.dversion
OBSOLETE #define	md_name			log_data.md.name
OBSOLETE 
OBSOLETE typedef	struct mkdir_log_data {
OBSOLETE 	struct VenusFid	dirfid;		/* Fid of this dir */
OBSOLETE 	struct VenusFid	parentfid;	/* Fid of parent */
OBSOLETE 	/* struct vattr	vattr;		 attrs to create with */
OBSOLETE         AFSStoreStatus  vattr;        /* log store_status */ /*Ba Wu */
OBSOLETE     	hyper 		dversion;		/* cached data version of file */
OBSOLETE 	char		name[MAX_NAME]; /* space to store create name */
OBSOLETE } mkdir_log_data_t;
OBSOLETE 
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	cr_filefid		log_data.cr.filefid
OBSOLETE #define	cr_parentfid		log_data.cr.parentfid
OBSOLETE #define	cr_vattr		log_data.cr.vattr
OBSOLETE #define	cr_mode			log_data.cr.mode
OBSOLETE #define	cr_exists		log_data.cr.exists
OBSOLETE #define	cr_excl			log_data.cr.excl
OBSOLETE #define	cr_origdv		log_data.cr.origdv
OBSOLETE #define	cr_name			log_data.cr.name
OBSOLETE 
OBSOLETE typedef struct	create_log_data {
OBSOLETE 	struct VenusFid	filefid;	/* Fid of this file */
OBSOLETE 	struct VenusFid	parentfid;	/* Fid of parent */
OBSOLETE 	struct nnpfs_attr	vattr;		/* attrs to create with */
OBSOLETE 	int		mode;		/* mode to create with */
OBSOLETE 	int		exists;		/* did file exists */
OBSOLETE 	int		excl;		/* is file create exclusive ? */
OBSOLETE     	hyper 		origdv;		/* cached data version of file */
OBSOLETE 	char		name[MAX_NAME]; /* space to store create name */
OBSOLETE } create_log_data_t;
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	rm_filefid		log_data.rm.filefid
OBSOLETE #define rm_chentry		log_data.rm.chentry
OBSOLETE #define	rm_origdv		log_data.rm.origdv
OBSOLETE #define	rm_name			log_data.rm.name
OBSOLETE 
OBSOLETE typedef struct	remove_log_data {
OBSOLETE 	struct VenusFid	filefid;	/* Fid of this file */
OBSOLETE 	/*struct VenusFid	parentfid;*/	/* Fid of parent */
OBSOLETE 	FCacheEntry	       *chentry;   /*The entry for the deleted file*/
OBSOLETE     	hyper 		origdv;		/* cached data version of file */
OBSOLETE 	char		name[MAX_NAME]; /* space to store remove name */
OBSOLETE } remove_log_data_t;
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE /* defines to make access easier  */
OBSOLETE #define	rd_direntry 		log_data.rd.direntry  
OBSOLETE #define	rd_parentfid		log_data.rd.parentfid
OBSOLETE #define	rd_name			log_data.rd.name
OBSOLETE 
OBSOLETE typedef struct	rmdir_log_data {
OBSOLETE 	FCacheEntry		*direntry;	 /*Entry of this dir */
OBSOLETE 	struct 	VenusFid	parentfid;	/* Fid of parent */
OBSOLETE 	char		name[MAX_NAME]; /* space to store dir name */
OBSOLETE } rmdir_log_data_t;
OBSOLETE 
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define rn_oparentfid           log_data.rn.oparentfid
OBSOLETE #define rn_nparentfid           log_data.rn.nparentfid
OBSOLETE #define rn_renamefid            log_data.rn.renamefid
OBSOLETE #define rn_overfid              log_data.rn.overfid
OBSOLETE #define rn_origdv               log_data.rn.origdv
OBSOLETE #define rn_overdv               log_data.rn.overdv
OBSOLETE #define rn_names                log_data.rn.names
OBSOLETE 
OBSOLETE typedef struct  rename_log_data {
OBSOLETE         struct VenusFid oparentfid;     /* Fid of parent */
OBSOLETE         struct VenusFid nparentfid;     /* Fid of parent */
OBSOLETE         struct VenusFid renamefid;      /* Fid of file being rename */
OBSOLETE         struct VenusFid overfid;        /* Fid of overwritten file */
OBSOLETE         hyper           origdv;         /* cached data version of file */
OBSOLETE         hyper           overdv;         /* overwritten version of cached data */
OBSOLETE         char            names[MAX_NAME * 2]; /* space to store dir name */
OBSOLETE } rename_log_data_t;
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	ln_linkfid		log_data.ln.linkfid
OBSOLETE #define	ln_parentfid		log_data.ln.parentfid
OBSOLETE #define	ln_name			log_data.ln.name
OBSOLETE 
OBSOLETE typedef struct	link_log_data {
OBSOLETE 	struct VenusFid	linkfid;	/* Fid of this dir */
OBSOLETE 	struct VenusFid	parentfid;	/* Fid of parent */
OBSOLETE 	char		name[MAX_NAME]; /* space to store create name */
OBSOLETE } link_log_data_t;
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	sy_linkfid		log_data.sy.linkfid
OBSOLETE #define	sy_parentfid		log_data.sy.parentfid
OBSOLETE #define	sy_filefid		log_data.sy.filefid
OBSOLETE #define	sy_attr		  	log_data.sy.attr
OBSOLETE #define	sy_name	        	log_data.sy.name
OBSOLETE #define sy_content		log_data.sy.content
OBSOLETE 
OBSOLETE 
OBSOLETE typedef struct	slink_log_data {
OBSOLETE 	struct VenusFid	linkfid;	/* Fid of this link */
OBSOLETE 	struct VenusFid	parentfid;	/* Fid of parent */
OBSOLETE 	struct VenusFid	filefid;	/* Fid of file */
OBSOLETE 	AFSStoreStatus  attr;		/* attrs to create with */
OBSOLETE 	char		name[MAX_NAME]; /* space to name */
OBSOLETE 	char		content[MAX_NAME]; /* space to new name */
OBSOLETE } slink_log_data_t;
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	sa_fid		log_data.sa.fid
OBSOLETE #define	sa_vattr	log_data.sa.vattr
OBSOLETE #define	sa_origdv	log_data.sa.origdv
OBSOLETE 
OBSOLETE typedef struct	setattr_log_data {
OBSOLETE 	struct VenusFid	fid;		/* operand fid */
OBSOLETE 	struct nnpfs_attr	vattr;		/* attrs to set */
OBSOLETE 	hyper		origdv;		/* cached data version number */
OBSOLETE } setattr_log_data_t;
OBSOLETE 
OBSOLETE 
OBSOLETE /* defines to make access easier */
OBSOLETE #define	nm_fid		log_data.nm.fid
OBSOLETE #define	nm_origdv	log_data.nm.origdv
OBSOLETE 
OBSOLETE typedef struct	nonmute_log_data {
OBSOLETE 	struct VenusFid	fid;		/* fid */
OBSOLETE 	hyper		origdv;		/* cached data version */
OBSOLETE } nonmute_log_data_t;
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE typedef struct log_ent {
OBSOLETE 	int		log_len;	/* len of this entry */
OBSOLETE 	log_ops_t	log_op;		/* operation */
OBSOLETE 	long		log_opno;	/* operation number */
OBSOLETE 	struct timeval	log_time;	/* time operation was logged */
OBSOLETE 	long            log_offset;     /* offset into the log file */
OBSOLETE 	short           log_flags;      /* offset into the log file */
OBSOLETE 	uid_t           log_uid;        /* uid of person performing op */
OBSOLETE         nnpfs_cred        cred;           /* user credential */
OBSOLETE         int		log_index;      /* index for the log */
OBSOLETE 	struct log_ent  *next;          /* point to the next one */
OBSOLETE 
OBSOLETE 	union {
OBSOLETE 		store_log_data_t	st;
OBSOLETE 		mkdir_log_data_t	md;
OBSOLETE 		create_log_data_t	cr;
OBSOLETE 		remove_log_data_t	rm;
OBSOLETE 		rmdir_log_data_t	rd;
OBSOLETE 		rename_log_data_t	rn;
OBSOLETE 		link_log_data_t		ln;
OBSOLETE 		slink_log_data_t	sy;
OBSOLETE 		setattr_log_data_t	sa;
OBSOLETE 		nonmute_log_data_t	nm;
OBSOLETE 	} log_data;
OBSOLETE } log_ent_t;
OBSOLETE 
OBSOLETE 
OBSOLETE long log_dis_create(struct vcache *parent, struct vcache *child, char *name);
OBSOLETE long log_dis_store(struct vcache *avc);
OBSOLETE long log_dis_setattr(struct vcache *tvc, struct nnpfs_attr *attrs);
OBSOLETE long log_dis_mkdir(struct vcache *pvc, struct vcache *dvc, 
OBSOLETE 		   AFSStoreStatus *attrs, char *name);
OBSOLETE 
OBSOLETE extern int Log_is_open;
OBSOLETE extern DARLA_file log_data;
OBSOLETE extern log_ent_t  log_head;
OBSOLETE 
OBSOLETE #endif /* _DISCONN_LOG_H */
OBSOLETE 
