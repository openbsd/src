/*	$OpenBSD: ns_func.h,v 1.3 2002/02/16 21:28:06 millert Exp $	*/

/* ns_func.h - declarations for ns_*.c's externally visible functions
 *
 * $From: ns_func.h,v 8.13 1996/11/11 06:36:49 vixie Exp $
 */

/* ++from ns_resp.c++ */
extern void		ns_resp(u_char *, int),
			prime_cache(void),
			delete_all(struct namebuf *, int, int),
			delete_stale(struct namebuf *);
extern struct qinfo	*sysquery __P((const char *, int, int,
				       struct in_addr *, int, int));
extern struct notify	*findNotifyPeer __P((const struct zoneinfo *,
					   struct in_addr));
extern void		sysnotify(const char *, int, int);
extern int		doupdate __P((u_char *, int, u_char *, int,
				      struct databuf **, int, u_int)),
			send_msg(u_char *, int, struct qinfo *),
			findns __P((struct namebuf **, int,
				    struct databuf **, int *, int)),
			finddata __P((struct namebuf *, int, int, HEADER *,
				      char **, int *, int *)),
			wanted(struct databuf *, int, int),
			add_data __P((struct namebuf *,
				      struct databuf **,
				      u_char *, int, int *));
/* --from ns_resp.c-- */

/* ++from ns_req.c++ */
extern void		ns_req __P((u_char *, int, int,
				    struct qstream *,
				    struct sockaddr_in *,
				    int)),
			free_addinfo(void),
			free_nsp(struct databuf **);
extern int		stale(struct databuf *),
			make_rr __P((const char *, struct databuf *,
				     u_char *, int, int)),
			doaddinfo(HEADER *, u_char *, int),
			doaddauth __P((HEADER *, u_char *, int,
				       struct namebuf *,
				       struct databuf *));
#ifdef BIND_NOTIFY
extern int		findZonePri __P((const struct zoneinfo *,
					 const struct sockaddr_in *));
#endif
/* --from ns_req.c-- */

/* ++from ns_forw.c++ */
extern time_t		retrytime(struct qinfo *);
extern int		ns_forw __P((struct databuf *nsp[],
				     u_char *msg,
				     int msglen,
				     struct sockaddr_in *fp,
				     struct qstream *qsp,
				     int dfd,
				     struct qinfo **qpp,
				     char *dname,
				     int class, int type,
				     struct namebuf *np)),
			haveComplained(const char *, const char *),
			nslookup __P((struct databuf *nsp[],
				      struct qinfo *qp,
				      const char *syslogdname,
				      const char *sysloginfo)),
			qcomp(struct qserv *, struct qserv *);
extern struct qdatagram	*aIsUs(struct in_addr);
extern void		schedretry(struct qinfo *, time_t),
			unsched(struct qinfo *),
			retry(struct qinfo *),
			qflush(void),
			qremove(struct qinfo *),
			nsfree(struct qinfo *, char *),
			qfree(struct qinfo *);
extern struct qinfo	*qfindid(u_int16_t),
			*qnew(const char *, int, int);
/* --from ns_forw.c-- */

/* ++from ns_main.c++ */
extern u_int32_t	net_mask(struct in_addr);
extern void		sqrm(struct qstream *),
			sqflush(struct qstream *allbut),
			dqflush(time_t gen),
			sq_done(struct qstream *),
			ns_setproctitle(char *, int),
			getnetconf(void),
			nsid_init(void);
extern u_int16_t	nsid_next(void);
extern struct netinfo	*findnetinfo(struct in_addr);
/* --from ns_main.c-- */

/* ++from ns_maint.c++ */
extern void		ns_maint(void),
			sched_maint(void),
#ifdef CLEANCACHE
			remove_zone(struct hashbuf *, int, int),
#else
			remove_zone(struct hashbuf *, int),
#endif
#ifdef PURGE_ZONE
			purge_zone(const char *, struct hashbuf *, int),
#endif
			loadxfer(void),
			qserial_query(struct zoneinfo *),
			qserial_answer(struct qinfo *, u_int32_t);
extern void		holdsigchld(void);
extern void		releasesigchld(void);
extern SIG_FN		reapchild();
extern void		endxfer(void);
extern const char *	zoneTypeString(const struct zoneinfo *);
#ifdef DEBUG
extern void		printzoneinfo(int);
#endif
/* --from ns_maint.c-- */

/* ++from ns_sort.c++ */
extern struct netinfo	*local(struct sockaddr_in *);
extern void		sort_response __P((u_char *, int,
					   struct netinfo *,
					   u_char *));
/* --from ns_sort.c-- */

/* ++from ns_init.c++ */
extern void		ns_refreshtime(struct zoneinfo *, time_t),
			ns_retrytime(struct zoneinfo *, time_t),
			ns_init(char *);
extern enum context	ns_ptrcontext(const char *owner);
extern enum context	ns_ownercontext(int type, enum transport);
extern int		ns_nameok __P((const char *name, int class,
				       enum transport, enum context,
				       const char *owner,
				       struct in_addr source));
extern int		ns_wildcard(const char *name);
/* --from ns_init.c-- */

/* ++from ns_ncache.c++ */
extern void		cache_n_resp(u_char *, int);
/* --from ns_ncache.c-- */

/* ++from ns_udp.c++ */
extern void		ns_udp(void);
/* --from ns_udp.c-- */

/* ++from ns_stats.c++ */
extern void		ns_stats(void);
#ifdef XSTATS
extern void		ns_logstats(void);
#endif
extern void		qtypeIncr(int qtype);
extern struct nameser	*nameserFind(struct in_addr addr, int flags);
#define NS_F_INSERT	0x0001
extern void		nameserIncr __P((struct in_addr addr,
					 enum nameserStats which));
/* --from ns_stats.c-- */

/* ++from ns_validate.c++ */
extern int
#ifdef NCACHE
			validate __P((char *, char *, struct sockaddr_in *,
				      int, int, char *, int, int)),
#else
			validate __P((char *, char *, struct sockaddr_in *,
				      int, int, char *, int)),
#endif
			dovalidate __P((u_char *, int, u_char *, int, int,
					char *, struct sockaddr_in *, int *)),
			update_msg(u_char *, int *, int Vlist[], int);
extern void		store_name_addr __P((const char *, struct in_addr,
					     const char *, const char *));
/* --from ns_validate.c-- */
