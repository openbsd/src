static uint32_t ipsec_util_seq = 0;
static int ipsec_util_pid = -1;

struct sadb_del_args {
	int				is_valid;
	uint32_t			spi[128];
	int				spiidx;
	struct sadb_address		src;
	union {
		struct sockaddr_in	sin4;
		struct sockaddr_in6	sin6;
	} src_sa;
	u_char				src_pad[8]; /* for PFKEY_ALIGN8 */
	struct sadb_address		dst;
	union {
		struct sockaddr_in	sin4;
		struct sockaddr_in6	sin6;
	} dst_sa;
	u_char				dst_pad[8]; /* for PFKEY_ALIGN8 */
};

static void        ipsec_util_prepare (void);
static int         delete_prepare (int, struct sockaddr *, struct sockaddr *, int, struct sadb_del_args *, struct sadb_del_args *);
static int         send_sadb_delete (int, struct sadb_del_args *);
static inline int  address_compar (struct sadb_address *, struct sockaddr *, int);
static int         sadb_del_args_init (struct sadb_del_args *, uint32_t, struct sadb_address *, struct sadb_address *, int);
static int         sockaddr_is_valid (struct sockaddr *);

#ifndef countof
#define	countof(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

struct timeval const KEYSOCK_RCVTIMEO = { .tv_sec = 0, .tv_usec = 500000L };
