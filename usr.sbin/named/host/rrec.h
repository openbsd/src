/*
** Resource record structures.
**
**	These define the various resource record fields after decoding
**	from the internal representation in the nameserver answer buffer.
**
**	@(#)rrec.h              e07@nikhef.nl (Eric Wassenaar) 941205
*/

#define MAXSTRING 255		/* maximum size of single encoded string */
#define MAXSTRLEN MAXDLEN	/* maximum size of total  encoded string */

typedef struct rr_data {
	u_char databuf[MAXDLEN];	/* generic data buffer */
} rr_data_t;

/*
** Record-specific data fields.
*/
	/* traditional records */

typedef struct a_data {
	ipaddr_t address;		/* internet address of host */
} a_data_t;

typedef struct ns_data {
	char nameserver[MAXDNAME+1];	/* name of domain nameserver */
} ns_data_t;

typedef struct md_data {
	char destination[MAXDNAME+1];	/* name of mail destination */
} md_data_t;

typedef struct mf_data {
	char forwarder[MAXDNAME+1];	/* name of mail forwarder */
} mf_data_t;

typedef struct cname_data {
	char canonical[MAXDNAME+1];	/* canonical domain name */
} cname_data_t;

typedef struct soa_data {
	char primary[MAXDNAME+1];	/* name of primary nameserver */
	char hostmaster[MAXDNAME+1];	/* name of hostmaster mailbox */
	int serial;			/* serial (version) number */
	int refresh;			/* refresh time in seconds */
	int retry;			/* refresh retry time in seconds */
	int expire;			/* expiration time in seconds */
	int defttl;			/* default time_to_live */
} soa_data_t;

typedef struct mb_data {
	char mailhost[MAXDNAME+1];	/* name of mailbox host */
} mb_data_t;

typedef struct mg_data {
	char memberbox[MAXDNAME+1];	/* mailbox of mail group member */
} mg_data_t;

typedef struct mr_data {
	char aliasbox[MAXDNAME+1];	/* mailbox of mail alias */
} mr_data_t;

typedef struct null_data {
	u_char nullbuf[MAXDLEN];	/* generic data buffer */
} null_data_t;

typedef struct wks_data {
	ipaddr_t servaddress;		/* internet address of host */
	int protocol;			/* protocol number */
	u_char services[32];		/* ports 0-255 */
} wks_data_t;

typedef struct ptr_data {
	char domain[MAXDNAME+1];	/* domain name of pointer */
} ptr_data_t;

typedef struct hinfo_data {
	char cputype[MAXSTRING+1];	/* machine description */
	char ostype[MAXSTRING+1];	/* operating system type */
} hinfo_data_t;

typedef struct minfo_data {
	char ownerbox[MAXDNAME+1];	/* name of owner mailbox */
	char errorbox[MAXDNAME+1];	/* name of error mailbox */
} minfo_data_t;

typedef struct mx_data {
	int preference;			/* preference value */
	char mxhost[MAXDNAME+1];	/* name of mx host */
} mx_data_t;

typedef struct txt_data {
	char text[MAXSTRLEN+1];		/* concatenated substrings */
} txt_data_t;

	/* later additions */

typedef struct rp_data {
	char mailbox[MAXDNAME+1];	/* name of person mailbox */
	char txtinfo[MAXDNAME+1];	/* name of description txt record */
} rp_data_t;

typedef struct afsdb_data {
	int afstype;			/* type of afs server */
	char afshost[MAXDNAME+1];	/* name of afs server */
} afsdb_data_t;

typedef struct x25_data {
	char psdnaddress[MAXSTRING+1];	/* x25 psdn address */
} x25_data_t;

typedef struct isdn_data {
	char isdnaddress[MAXSTRING+1];	/* isdn address */
	char isdnsubaddr[MAXSTRING+1];	/* isdn subaddress */
} isdn_data_t;

typedef struct rt_data {
	int routepref;			/* preference value */
	char routehost[MAXDNAME+1];	/* name of route-through host */
} rt_data_t;

typedef struct nsap_data {
	u_char nsapaddr[MAXNSAP];	/* binary nsap address */
} nsap_data_t;

typedef struct nsapptr_data {
	char nsapdomain[MAXDNAME+1];	/* domain name of nsap pointer */
} nsapptr_data_t;

typedef struct px_data {
	int mappref;			/* preference value */
	char map822[MAXDNAME+1];	/* rfc822 domain name */
	char mapx400[MAXDNAME+1];	/* x400 domain name */
} px_data_t;

typedef struct gpos_data {
	char longpos[MAXSTRING+1];	/* geographical longitude */
	char latpos[MAXSTRING+1];	/* geographical latitude */
	char altpos[MAXSTRING+1];	/* geographical altitude */
} gpos_data_t;

typedef struct loc_data {
	int locversion;			/* version number */
	int objectsize;			/* size of object */
	int hprecision;			/* horizontal precision */
	int vprecision;			/* vertical precision */
	int longitude;			/* geographical longitude */
	int latitude;			/* geographical latitude */
	int altitude;			/* geographical altitude */
} loc_data_t;

	/* nonstandard records */

typedef struct uinfo_data {
	char userinfo[MAXSTRLEN+1];	/* user description */
} uinfo_data_t;

typedef struct uid_data {
	int userid;			/* user uid */
} uid_data_t;

typedef struct gid_data {
	int groupid;			/* user gid */
} gid_data_t;

typedef struct unspec_data {
	u_char unspecbuf[MAXDLEN];	/* generic data buffer */
} unspec_data_t;

/*
** Generic resource record description.
*/

typedef struct rrecord {
	char	name[MAXDNAME+1];	/* resource record name */
	int	type;			/* resource record type */
	int	class;			/* resource record class */
	int	ttl;			/* time_to_live value */
	union {
		rr_data_t	data_rr;
		a_data_t	data_a;
		ns_data_t	data_ns;
		md_data_t	data_md;
		mf_data_t	data_mf;
		cname_data_t	data_cname;
		soa_data_t	data_soa;
		mb_data_t	data_mb;
		mg_data_t	data_mg;
		mr_data_t	data_mr;
		null_data_t	data_null;
		wks_data_t	data_wks;
		ptr_data_t	data_ptr;
		hinfo_data_t	data_hinfo;
		minfo_data_t	data_minfo;
		mx_data_t	data_mx;
		txt_data_t	data_txt;
		rp_data_t	data_rp;
		afsdb_data_t	data_afsdb;
		x25_data_t	data_x25;
		isdn_data_t	data_isdn;
		rt_data_t	data_rt;
		nsap_data_t	data_nsap;
		nsapptr_data_t	data_nsapptr;
		px_data_t	data_px;
		gpos_data_t	data_gpos;
		loc_data_t	data_loc;
		uinfo_data_t	data_uinfo;
		uid_data_t	data_uid;
		gid_data_t	data_gid;
		unspec_data_t	data_unspec;
	} data;
} rrecord_t;

#define t_rr		data.data_rr
#define t_a		data.data_a
#define t_ns		data.data_ns
#define t_md		data.data_md
#define t_mf		data.data_mf
#define t_cname		data.data_cname
#define t_soa		data.data_soa
#define t_mb		data.data_mb
#define t_mg		data.data_mg
#define t_mr		data.data_mr
#define t_null		data.data_null
#define t_wks		data.data_wks
#define t_ptr		data.data_ptr
#define t_hinfo		data.data_hinfo
#define t_minfo		data.data_minfo
#define t_mx		data.data_mx
#define t_txt		data.data_txt
#define t_rp		data.data_rp
#define t_afsdb		data.data_afsdb
#define t_x25		data.data_x25
#define t_isdn		data.data_isdn
#define t_rt		data.data_rt
#define t_nsap		data.data_nsap
#define t_nsapptr	data.data_nsapptr
#define t_px		data.data_px
#define t_gpos		data.data_gpos
#define t_loc		data.data_loc
#define t_uinfo		data.data_uinfo
#define t_uid		data.data_uid
#define t_gid		data.data_gid
#define t_unspec	data.data_unspec
