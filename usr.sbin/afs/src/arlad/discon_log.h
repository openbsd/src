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
 * This file contains all the relevant information pertaining to logging
 * for disconnected afs.
 */
/* replace vattr with xfs_attr in the log */
#include <xfs/xfs_attr.h>
#ifndef  _DISCONN_LOG_H

#define _DISCONN_LOG_H

#define BUFSIZE 256

#define MAX_NAME AFSNAMEMAX

enum log_ops {
	DIS_STORE,
	DIS_MKDIR,
	DIS_CREATE,
	DIS_REMOVE,
	DIS_RMDIR,
	DIS_RENAME,
	DIS_LINK,
	DIS_SYMLINK,
	DIS_SETATTR,
	DIS_FSYNC,
	DIS_ACCESS,
	DIS_READDIR,
	DIS_READLINK,
	DIS_INFO,
	DIS_START_OPT,
	DIS_END_OPT,
	DIS_REPLAYED
};

typedef enum log_ops log_ops_t;

/* These are defines for the different log flags that can be set */
#define       LOG_INACTIVE    0x1     /* log no longer needs replay */
#define       LOG_REPLAYED    0x2     /* log entry was replayed */
#define       LOG_OPTIMIZED   0x4     /* log entry was optimized away */
#define       LOG_GENERATED   0x8     /* log entry created by optimizer */


/* defines to make access easier */
#define	st_fid		log_data.st.fid
#define	st_origdv	log_data.st.origdv

/* added flag option */
#define st_flag		log_data.st.flag

typedef u_int32_t hyper;

/* a really stripped down version of CITI's vcache */
struct vcache {
	u_int32_t	DataVersion;
	struct VenusFid fid;
        xfs_cred 	cred;
        u_int           flag;           /* write flag */
};

typedef	struct store_log_data {
	struct VenusFid	fid;		/* fid of the file */
	u_int flag;		/* write flag */
    	hyper 		origdv;	/* cached data version of file */
} store_log_data_t;

/* defines to make access easier */
#define	md_dirfid		log_data.md.dirfid
#define	md_parentfid		log_data.md.parentfid
#define	md_vattr		log_data.md.vattr
#define	md_dversion		log_data.md.dversion
#define	md_name			log_data.md.name

typedef	struct mkdir_log_data {
	struct VenusFid	dirfid;		/* Fid of this dir */
	struct VenusFid	parentfid;	/* Fid of parent */
	/* struct vattr	vattr;		 attrs to create with */
        AFSStoreStatus  vattr;        /* log store_status */ /*Ba Wu */
    	hyper 		dversion;		/* cached data version of file */
	char		name[MAX_NAME]; /* space to store create name */
} mkdir_log_data_t;


/* defines to make access easier */
#define	cr_filefid		log_data.cr.filefid
#define	cr_parentfid		log_data.cr.parentfid
#define	cr_vattr		log_data.cr.vattr
#define	cr_mode			log_data.cr.mode
#define	cr_exists		log_data.cr.exists
#define	cr_excl			log_data.cr.excl
#define	cr_origdv		log_data.cr.origdv
#define	cr_name			log_data.cr.name

typedef struct	create_log_data {
	struct VenusFid	filefid;	/* Fid of this file */
	struct VenusFid	parentfid;	/* Fid of parent */
	struct xfs_attr	vattr;		/* attrs to create with */
	int		mode;		/* mode to create with */
	int		exists;		/* did file exists */
	int		excl;		/* is file create exclusive ? */
    	hyper 		origdv;		/* cached data version of file */
	char		name[MAX_NAME]; /* space to store create name */
} create_log_data_t;




/* defines to make access easier */
#define	rm_filefid		log_data.rm.filefid
#define rm_chentry		log_data.rm.chentry
#define	rm_origdv		log_data.rm.origdv
#define	rm_name			log_data.rm.name

typedef struct	remove_log_data {
	struct VenusFid	filefid;	/* Fid of this file */
	/*struct VenusFid	parentfid;*/	/* Fid of parent */
	FCacheEntry	       *chentry;   /*The entry for the deleted file*/
    	hyper 		origdv;		/* cached data version of file */
	char		name[MAX_NAME]; /* space to store remove name */
} remove_log_data_t;



/* defines to make access easier  */
#define	rd_direntry 		log_data.rd.direntry  
#define	rd_parentfid		log_data.rd.parentfid
#define	rd_name			log_data.rd.name

typedef struct	rmdir_log_data {
	FCacheEntry		*direntry;	 /*Entry of this dir */
	struct 	VenusFid	parentfid;	/* Fid of parent */
	char		name[MAX_NAME]; /* space to store dir name */
} rmdir_log_data_t;


/* defines to make access easier */
#define rn_oparentfid           log_data.rn.oparentfid
#define rn_nparentfid           log_data.rn.nparentfid
#define rn_renamefid            log_data.rn.renamefid
#define rn_overfid              log_data.rn.overfid
#define rn_origdv               log_data.rn.origdv
#define rn_overdv               log_data.rn.overdv
#define rn_names                log_data.rn.names

typedef struct  rename_log_data {
        struct VenusFid oparentfid;     /* Fid of parent */
        struct VenusFid nparentfid;     /* Fid of parent */
        struct VenusFid renamefid;      /* Fid of file being rename */
        struct VenusFid overfid;        /* Fid of overwritten file */
        hyper           origdv;         /* cached data version of file */
        hyper           overdv;         /* overwritten version of cached data */
        char            names[MAX_NAME * 2]; /* space to store dir name */
} rename_log_data_t;

/* defines to make access easier */
#define	ln_linkfid		log_data.ln.linkfid
#define	ln_parentfid		log_data.ln.parentfid
#define	ln_name			log_data.ln.name

typedef struct	link_log_data {
	struct VenusFid	linkfid;	/* Fid of this dir */
	struct VenusFid	parentfid;	/* Fid of parent */
	char		name[MAX_NAME]; /* space to store create name */
} link_log_data_t;

/* defines to make access easier */
#define	sy_linkfid		log_data.sy.linkfid
#define	sy_parentfid		log_data.sy.parentfid
#define	sy_filefid		log_data.sy.filefid
#define	sy_attr		  	log_data.sy.attr
#define	sy_name	        	log_data.sy.name
#define sy_content		log_data.sy.content


typedef struct	slink_log_data {
	struct VenusFid	linkfid;	/* Fid of this link */
	struct VenusFid	parentfid;	/* Fid of parent */
	struct VenusFid	filefid;	/* Fid of file */
	AFSStoreStatus  attr;		/* attrs to create with */
	char		name[MAX_NAME]; /* space to name */
	char		content[MAX_NAME]; /* space to new name */
} slink_log_data_t;

/* defines to make access easier */
#define	sa_fid		log_data.sa.fid
#define	sa_vattr	log_data.sa.vattr
#define	sa_origdv	log_data.sa.origdv

typedef struct	setattr_log_data {
	struct VenusFid	fid;		/* operand fid */
	struct xfs_attr	vattr;		/* attrs to set */
	hyper		origdv;		/* cached data version number */
} setattr_log_data_t;


/* defines to make access easier */
#define	nm_fid		log_data.nm.fid
#define	nm_origdv	log_data.nm.origdv

typedef struct	nonmute_log_data {
	struct VenusFid	fid;		/* fid */
	hyper		origdv;		/* cached data version */
} nonmute_log_data_t;



typedef struct log_ent {
	int		log_len;	/* len of this entry */
	log_ops_t	log_op;		/* operation */
	long		log_opno;	/* operation number */
	struct timeval	log_time;	/* time operation was logged */
	long            log_offset;     /* offset into the log file */
	short           log_flags;      /* offset into the log file */
	uid_t           log_uid;        /* uid of person performing op */
        xfs_cred        cred;           /* user credential */
        int		log_index;      /* index for the log */
	struct log_ent  *next;          /* point to the next one */

	union {
		store_log_data_t	st;
		mkdir_log_data_t	md;
		create_log_data_t	cr;
		remove_log_data_t	rm;
		rmdir_log_data_t	rd;
		rename_log_data_t	rn;
		link_log_data_t		ln;
		slink_log_data_t	sy;
		setattr_log_data_t	sa;
		nonmute_log_data_t	nm;
	} log_data;
} log_ent_t;


long log_dis_create(struct vcache *parent, struct vcache *child, char *name);
long log_dis_store(struct vcache *avc);
long log_dis_setattr(struct vcache *tvc, struct xfs_attr *attrs);
long log_dis_mkdir(struct vcache *pvc, struct vcache *dvc, 
		   AFSStoreStatus *attrs, char *name);

#endif /* _DISCONN_LOG_H */

