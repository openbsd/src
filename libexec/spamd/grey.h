
#define MAX_MAIL 1024 /* how big an email address will we consider */
#define PASSTIME (60 * 30) /* pass after first retry seen after 30 mins */
#define GREYEXP (60 * 60 * 4) /* remove grey entries after 4 hours */
#define WHITEEXP (60 * 60 * 24 * 36) /* remove white entries after 36 days */
#define PATH_PFCTL "/sbin/pfctl"
#define DB_SCAN_INTERVAL 60
#define PATH_SPAMD_DB "/var/db/spamd"

struct gdata {
	time_t first;  /* when did we see it first */
	time_t pass;   /* when was it whitelisted */
	time_t expire; /* when will we get rid of this entry */
	int bcount;    /* how many times have we blocked it */
	int pcount;    /* how many good connections have we seen after wl */
};

extern int greywatcher(void);
