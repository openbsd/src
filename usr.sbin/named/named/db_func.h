/*	$OpenBSD: db_func.h,v 1.5 2002/02/16 21:28:06 millert Exp $	*/

/* db_proc.h - prototypes for functions in db_*.c
 *
 * $From: db_func.h,v 8.13 1997/06/01 20:34:34 vixie Exp $
 */

/* ++from db_update.c++ */
extern int		db_update __P((char name[],
				       struct databuf *odp,
				       struct databuf *newdp,
				       int flags,
				       struct hashbuf *htp)),
			db_cmp(struct databuf *,struct databuf *),
			findMyZone(struct namebuf *np, int class);
extern void		fixttl(struct databuf *dp);
/* --from db_update.c-- */

/* ++from db_reload.c++ */
extern void		db_reload(int);
/* --from db_reload.c-- */

/* ++from db_save.c++ */
extern struct namebuf	*savename(const char *, int);
extern struct databuf	*savedata(int, int, u_int32_t, u_char *, int);
extern struct hashbuf	*savehash(struct hashbuf *);
/* --from db_save.c-- */

/* ++from db_dump.c++ */
extern int		db_dump(struct hashbuf *, FILE *, int, char *),
			zt_dump(FILE *),
			atob(char *, int, char *, int, int *);
extern void		doachkpt(void),
			doadump(void);
extern u_int		db_getclev(const char *);
/* --from db_dump.c-- */

/* ++from db_load.c++ */
extern void		endline(FILE *),
			get_netlist __P((FILE *, struct netinfo **,
					 int, char *)),
			free_netlist(struct netinfo **);
extern int		getword(char *, int, FILE *, int),
			getnum(FILE *, const char *, int),
			db_load __P((const char *, const char *,
				     struct zoneinfo *, const char *)),
			position_on_netlist __P((struct in_addr,
						 struct netinfo *));
extern struct netinfo	*addr_on_netlist __P((struct in_addr,
					      struct netinfo *));
/* --from db_load.c-- */

/* ++from db_glue.c++ */
extern const char	*sin_ntoa(const struct sockaddr_in *);
extern void		panic(int, const char *),
			buildservicelist(void),
			buildprotolist(void),
			gettime(struct timeval *),
			getname(struct namebuf *, char *, int);
extern int		servicenumber(char *),
			protocolnumber(char *),
			my_close(int),
			my_fclose(FILE *),
#ifdef GEN_AXFR
			get_class(char *),
#endif
			writemsg(int, u_char *, int),
			dhash(const u_char *, int),
			nhash(const char *),
			samedomain(const char *, const char *);
extern char		*protocolname(int),
			*servicename(u_int16_t, char *),
			*savestr(const char *);
#ifndef BSD
extern int		getdtablesize(void);
#endif
extern struct databuf	*rm_datum __P((struct databuf *,
				       struct namebuf *,
				       struct databuf *));
extern struct namebuf	*rm_name __P((struct namebuf *, 
				      struct namebuf **,
				      struct namebuf *));
#ifdef INVQ
extern void		addinv(struct namebuf *, struct databuf *),
			rminv(struct databuf *);
struct invbuf		*saveinv(void);
#endif
extern char *		ctimel(long);
extern struct in_addr	data_inaddr(const u_char *data);
extern void		setsignal __P((int, int, SIG_FN (*)())),
			resignal __P((int, int, SIG_FN (*)()));
extern void		db_free(struct databuf *);
/* --from db_glue.c-- */

/* ++from db_lookup.c++ */
extern struct namebuf	*nlookup __P((const char *, struct hashbuf **,
				      const char **, int));
extern struct namebuf	*np_parent(struct namebuf *);
extern int		match(struct databuf *, int, int);
/* --from db_lookup.c-- */

/* ++from db_secure.c++ */
#ifdef SECURE_ZONES
extern int		build_secure_netlist(struct zoneinfo *);
#endif
/* --from db_secure.c-- */
