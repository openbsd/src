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
OBSOLETE ** These are defines for the different type of disconnected
OBSOLETE ** operations stored in the log.
OBSOLETE */
OBSOLETE 
OBSOLETE #ifndef  _DISCONNH
OBSOLETE #define _DISCONNH
OBSOLETE 
OBSOLETE 
OBSOLETE /* these are define for integrity checking */
OBSOLETE #define CHECK_SLOTS	(long) 0x1
OBSOLETE #define CHECK_LRUQ	(long) 0x2
OBSOLETE #define CHECK_FREEVS	(long) 0x4
OBSOLETE 
OBSOLETE /* values for the dflags in the vcache */
OBSOLETE #define VC_DIRTY	0x01
OBSOLETE #define KEEP_VC		0x04
OBSOLETE #define DBAD_VC		0x08		/* This is a know bad vcache */
OBSOLETE /* this flags is used by GetVSlot to mark when a vcache was read from
OBSOLETE ** the disk.  
OBSOLETE */
OBSOLETE #define READ_DISK	0x10
OBSOLETE 
OBSOLETE /* Flags for dflags in the fcache */
OBSOLETE #define	KEEP_DC		0x01
OBSOLETE 
OBSOLETE /* Flags in the afs_VindexFlags */
OBSOLETE 
OBSOLETE #define VC_FREE		0x01 
OBSOLETE #define HAS_CCORE	0x02
OBSOLETE /* 0x04 is used by KEEP_VC */
OBSOLETE #define VC_DATAMOD	0x08
OBSOLETE #define VC_FLAG		0x10
OBSOLETE #define VC_CLEARAXS	0x20
OBSOLETE #define VC_HAVECB	0x40
OBSOLETE 
OBSOLETE /* magic number for data files */
OBSOLETE 
OBSOLETE #define AFS_DHMAGIC         0x7635fac1
OBSOLETE 
OBSOLETE /* these are the file name extensions for various errors */
OBSOLETE #define STORE_EXT	".store"
OBSOLETE #define RENAME_EXT	".ren"
OBSOLETE #define CREATE_EXT	".creat"
OBSOLETE #define MKDIR_EXT	".mkdir"
OBSOLETE #define LINK_EXT	".link"
OBSOLETE #define SYMLINK_EXT	".slink"
OBSOLETE #define ORPH_EXT	".orph"
OBSOLETE 
OBSOLETE /* conflicting defs with arla_local.h */
OBSOLETE /*enum discon_modes {
OBSOLETE 	CONNECTED,
OBSOLETE 	DISCONNECTED,
OBSOLETE 	FETCH_ONLY,
OBSOLETE 	PARTIALLY_CONNECTED
OBSOLETE };*/
OBSOLETE 
OBSOLETE #define	IS_DISCONNECTED(state)	(state == DISCONNECTED)
OBSOLETE #define	IS_FETCHONLY(state)	(state == FETCH_ONLY)
OBSOLETE #define	IS_PARTIAL(state)	(state == PARTIALLY_CONNECTED)
OBSOLETE #define	IS_CONNECTED(state)	(state == CONNECTED)
OBSOLETE #define LOG_OPERATIONS(state)	((state == DISCONNECTED) ||  \
OBSOLETE 		(state == FETCH_ONLY) || (state == PARTIALLY_CONNECTED))
OBSOLETE #define USE_OPTIMISTIC(state)	((state == DISCONNECTED) ||  \
OBSOLETE 		(state == FETCH_ONLY))
OBSOLETE 	
OBSOLETE 
OBSOLETE /* These are the different levels of error logging */
OBSOLETE #define	DISCON_ERR	0
OBSOLETE #define	DISCON_NOTICE	1
OBSOLETE #define	DISCON_INFO	2
OBSOLETE #define	DISCON_DEBUG	3
OBSOLETE 
OBSOLETE /* pioctl flags */
OBSOLETE #define AFS_DIS_RECON		0	/* normal reconnect */
OBSOLETE #define AFS_DIS_DISCON		1	/* disconnect */
OBSOLETE #define AFS_DIS_PANIC		2	/* die, monster devil, die */
OBSOLETE #define AFS_DIS_RECONTOSS	3	/* reconnect now! */
OBSOLETE #define AFS_DIS_QUERY		4	/* query disconnected state */
OBSOLETE #define AFS_DIS_FETCHONLY	5	/* disconnect, fetch-only mode */
OBSOLETE #define AFS_DIS_PARTIAL		6	/* partially connected mode */
OBSOLETE #define AFS_DIS_DISCONTOSS	7	/* disconnect without discarding callbacks */
OBSOLETE 
OBSOLETE 
OBSOLETE /* these are items defined to fhe PSetDOps */
OBSOLETE 
OBSOLETE typedef enum { GET_BACKUP_LOG_NAME, 
OBSOLETE 	SET_USERLOG_LEVEL,
OBSOLETE 	SET_FILELOG_LEVEL, 
OBSOLETE 	SET_LOGFILE, 
OBSOLETE 	UPDATE_FLAGS, 
OBSOLETE 	PING_SERVER, 
OBSOLETE 	GET_LOG_NAME, 
OBSOLETE 	GIVEUP_CBS,
OBSOLETE 	PRINT_INFO, 
OBSOLETE 	VERIFY_VCACHE, 
OBSOLETE 	VERIFY_DCACHE } dis_setopt_op_t;
OBSOLETE 
OBSOLETE #if 0
OBSOLETE #define MAX_NAME  255
OBSOLETE #endif
OBSOLETE 
OBSOLETE typedef struct dis_setop_info {
OBSOLETE 	dis_setopt_op_t	op;
OBSOLETE 	char 	data[MAX_NAME];
OBSOLETE } dis_setop_info_t;
OBSOLETE 
OBSOLETE 
OBSOLETE #ifdef KERNEL
OBSOLETE 
OBSOLETE 
OBSOLETE #define CELL_DIRTY   0x01
OBSOLETE #define REALLY_BIG 1024
OBSOLETE 
OBSOLETE struct save_cell {
OBSOLETE     long cell;				/* unique id assigned by venus */
OBSOLETE     char cellName[MAX_NAME];		/* char string name of cell */
OBSOLETE     short cellHosts[MAXHOSTS];		/* volume *location* hosts for this cell */
OBSOLETE     short lcell;			/* Associated linked cell */
OBSOLETE     short states;			/* state flags */
OBSOLETE     long fsport;			/* file server port */
OBSOLETE     long vlport;			/* volume server port */
OBSOLETE     short cellIndex;                    /* relative index number per cell */
OBSOLETE     short dindex;			/* disconnected index */
OBSOLETE };
OBSOLETE 
OBSOLETE #define SERV_DIRTY 0x01
OBSOLETE 
OBSOLETE struct save_server {
OBSOLETE     unsigned int cell; 		/* cell in which this host resides */
OBSOLETE     long host;			/* in network byte order, except subsys */
OBSOLETE     long portal;		/* in network byte order */
OBSOLETE     unsigned int random;	/* server priority, used for randomizing requests */
OBSOLETE     char isDown;		/* result of decision if server is down. */
OBSOLETE     char vcbCount;		/* count of vcbs */
OBSOLETE     short dindex;		/* disconnected index */
OBSOLETE };
OBSOLETE 
OBSOLETE 
OBSOLETE #define VOL_DIRTY 0x01
OBSOLETE 
OBSOLETE struct save_volume {
OBSOLETE     long cell;                  /* the cell in which the volume resides */
OBSOLETE     long volume;                /* This volume's ID number. */
OBSOLETE     char name[MAX_NAME];        /* This volume's name, or 0 if unknown */
OBSOLETE     short serverHost[MAXHOSTS];    /* servers serving this volume */
OBSOLETE     struct VenusFid dotdot;     /* dir to access as .. */
OBSOLETE     struct VenusFid mtpoint;    /* The mount point for this volume. */
OBSOLETE     long rootVnode, rootUnique;	/* Volume's root fid */
OBSOLETE     long roVol;
OBSOLETE     long backVol;
OBSOLETE     long rwVol;                 /* For r/o vols, original read/write volume. */
OBSOLETE     long accessTime;            /* last time we used it */
OBSOLETE     long copyDate;              /* copyDate field, for tracking vol releases */
OBSOLETE     char states;                /* snuck here for alignment reasons */
OBSOLETE     short dindex;
OBSOLETE };
OBSOLETE 
OBSOLETE 
OBSOLETE #define LLIST_SIZE  1024
OBSOLETE 
OBSOLETE 
OBSOLETE #ifndef GET_WRITE
OBSOLETE #define GET_WRITE   0x1
OBSOLETE #define GET_READ    0x2
OBSOLETE #define GET_SHARED  0x4
OBSOLETE 
OBSOLETE #define REL_WRITE   0x10
OBSOLETE #define REL_READ    0x20
OBSOLETE #define REL_SHARED  0x40
OBSOLETE 
OBSOLETE #define S_TO_W	    0x100
OBSOLETE #define W_TO_S	    0x200
OBSOLETE #define W_TO_R	    0x400
OBSOLETE #define S_TO_R      0x800
OBSOLETE #endif /* GET_WRITE */
OBSOLETE 
OBSOLETE struct llist {
OBSOLETE     struct afs_lock *lk;
OBSOLETE     short	operation;
OBSOLETE     short	who;
OBSOLETE     struct llist * next;
OBSOLETE     struct llist *prev;
OBSOLETE };
OBSOLETE 
OBSOLETE /* These are definition for the translation flags fields */
OBSOLETE 
OBSOLETE #define KNOWNBAD	0x20
OBSOLETE #define SYMLINK		0x40
OBSOLETE struct name_trans {
OBSOLETE     struct VenusFid pfid;
OBSOLETE     int ntrans_idx;
OBSOLETE     int oname_idx;
OBSOLETE     int nname_idx;
OBSOLETE     int next_nt_idx;
OBSOLETE     char *old_name;
OBSOLETE     char *new_name;
OBSOLETE     struct name_trans *next;
OBSOLETE };
OBSOLETE 
OBSOLETE struct translations {
OBSOLETE     struct VenusFid oldfid;
OBSOLETE     struct VenusFid newfid;
OBSOLETE     u_long flags;
OBSOLETE     hyper validDV;
OBSOLETE     int	trans_idx;
OBSOLETE     long callback;
OBSOLETE     long cbExpires;
OBSOLETE     int	nl_idx;
OBSOLETE     struct translations *next;
OBSOLETE     struct name_trans *name_list;
OBSOLETE };
OBSOLETE 
OBSOLETE 
OBSOLETE 
OBSOLETE /* 
OBSOLETE  * this struct is used to help speed up finding the number of callbacks for
OBSOLETE  * each server
OBSOLETE  */
OBSOLETE 
OBSOLETE struct serv_cbcount {
OBSOLETE     long  server;
OBSOLETE     long  count;
OBSOLETE     struct serv_cbcount *next;
OBSOLETE };
OBSOLETE 
OBSOLETE /* Stuff for the link name persistence */
OBSOLETE 
OBSOLETE #define MAP_ENTS	100
OBSOLETE #define	NAME_UNIT	255
OBSOLETE #define	NAME_DIRTY	0x1
OBSOLETE 
OBSOLETE /* header for the name backing file */
OBSOLETE typedef struct	map_header {
OBSOLETE     long	magic;		/* magic number */
OBSOLETE     int		num_ents;	/* number of names stored */
OBSOLETE     char	flags;		/* flags for in core copy */
OBSOLETE } map_header_t;
OBSOLETE 
OBSOLETE /* commented out */ 
OBSOLETE /*
OBSOLETE  * this struct holds all the information pertaining to a certain
OBSOLETE  * backing store used to keep persistance information
OBSOLETE  */
OBSOLETE /*typedef struct backing_store {
OBSOLETE 	long		bs_inode;	
OBSOLETE 	struct osi_dev	bs_dev;
OBSOLETE 	char	 	*bs_name;
OBSOLETE 	map_header_t	*bs_header;
OBSOLETE 	char 		*bs_map;
OBSOLETE 	struct afs_lock	bs_lock;
OBSOLETE 	struct osi_file	*tfile;
OBSOLETE } backing_store_t;*/
OBSOLETE 
OBSOLETE #endif /* KERNEL */
OBSOLETE 
OBSOLETE /* CacheItems file has a header of type struct afs_fheader (keep aligned properly) */
OBSOLETE struct afs_dheader {
OBSOLETE     long magic;
OBSOLETE     long firstCSize;
OBSOLETE     long otherCSize;
OBSOLETE     long spare;
OBSOLETE     long current_op;
OBSOLETE     enum connected_mode mode;
OBSOLETE };
OBSOLETE 
OBSOLETE #ifdef KERNEL
OBSOLETE 
OBSOLETE #define have_shared_lock(lock) \
OBSOLETE 	((((lock)->excl_locked==SHARED_LOCK) &&  \
OBSOLETE 	   ((lock)->proc == osi_curpid())) ?  1 :  0)
OBSOLETE 
OBSOLETE #define have_write_lock(lock) \
OBSOLETE 	((((lock)->excl_locked==WRITE_LOCK) &&  \
OBSOLETE 		((lock)->proc == osi_curpid())) ? 1 :  0)
OBSOLETE 
OBSOLETE extern struct llist *llist;
OBSOLETE extern struct llist *cur_llist;
OBSOLETE 
OBSOLETE /* these are function declarations so I can compile with -Wall in gcc,
OBSOLETE  * not really needed, but help make clean compiles.
OBSOLETE  */
OBSOLETE 
OBSOLETE extern int strlen();
OBSOLETE #ifndef AFS_NETBSD_ENV
OBSOLETE extern void strcpy();
OBSOLETE #endif	/* AFS_NETBSD_ENV */
OBSOLETE extern void bcopy();
OBSOLETE extern int dir_Delete();
OBSOLETE extern int dir_FindBlobs();
OBSOLETE extern int dir_Lookup();
OBSOLETE extern int dir_Create();
OBSOLETE extern int find_file_name();
OBSOLETE extern int afs_create();
OBSOLETE extern int afs_PutDCache();
OBSOLETE 
OBSOLETE #endif /* KERNEL */
OBSOLETE 
OBSOLETE #endif /*  _DISCONNH */
OBSOLETE 
OBSOLETE 
OBSOLETE #ifdef DISCONN
OBSOLETE typedef struct _fid_cb {
OBSOLETE VenusFid  fid;
OBSOLETE struct _fid_cb  *next; 
OBSOLETE } fid_cb;
OBSOLETE #endif
OBSOLETE 
OBSOLETE 
