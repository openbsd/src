/*	$OpenBSD: altq_qop.h,v 1.2 2002/02/13 08:21:45 kjc Exp $	*/
/*	$KAME: altq_qop.h,v 1.4 2000/10/18 09:15:18 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _ALTQ_QOP_H_
#define _ALTQ_QOP_H_

#include <sys/queue.h>
#include <altq/altq.h>
#include <altq/altq_red.h>

struct ifinfo;
struct classinfo;
struct fltrinfo;

/* queueing discipline specific command parsers */
struct qdisc_parser {
	char	*qname;
	int	(*interface_parser)(const char *ifname, int argc, char **argv);
	int	(*class_parser)(const char *ifname, const char *clname,
				const char *parent, int argc, char **argv);
};

/* queueing discipline specific operations */
struct qdisc_ops {
	int	qdisc_type;	/* discipline type (e.g., ALTQT_CBQ) */
	char	*qname;		/* discipline name (e.g., cbq) */

	/* interface operations */
	int	(*attach)(struct ifinfo *);
	int	(*detach)(struct ifinfo *);
	int	(*clear)(struct ifinfo *);
	int	(*enable)(struct ifinfo *);
	int	(*disable)(struct ifinfo *);

	/* class operations (optional) */
	int	(*add_class)(struct classinfo *);
	int	(*modify_class)(struct classinfo *, void *);
	int	(*delete_class)(struct classinfo *);

	/* filter operations (optional) */
	int	(*add_filter)(struct fltrinfo *);
	int	(*delete_filter)(struct fltrinfo *);
};

/*
 * interface info
 */
struct ifinfo {
	LIST_ENTRY(ifinfo)	next;		/* next entry on iflist */
	char			*ifname;	/* interface name */
	u_int			bandwidth;	/* bandwidth in bps */
	u_int			ifmtu;		/* mtu of the interface */
	u_int			ifindex;	/* interface index */
	int			enabled;	/* hfsc on/off state */
	LIST_HEAD(, classinfo)	cllist;		/* class list */
	LIST_HEAD(, fltrinfo)	fltr_rules;	/* filter rule list */

	struct classinfo	*resv_class;	/* special class for rsvp */

	/* discipline info */
	struct qdisc_ops	*qdisc;		/* qdisc system interface */
	void			*private;	/* discipline specific data */
	int	(*enable_hook)(struct ifinfo *);
	int	(*delete_hook)(struct ifinfo *);
};

/*
 * class info
 */
struct classinfo {
	LIST_ENTRY(classinfo)	next;		/* next entry on cllist
						   of ifinfo */
	u_long			handle;		/* class handle */
	char			*clname;	/* class name */
	struct ifinfo		*ifinfo;	/* back pointer to ifinfo */
	struct classinfo	*parent;	/* parent class */
	struct classinfo	*sibling;	/* sibling class */
	struct classinfo	*child;		/* child class */
	LIST_HEAD(, fltrinfo)	fltrlist;	/* filters for this class */

	void			*private;	/* discipline specific data */
	int	(*delete_hook)(struct classinfo *);
};

/*
 * filter info
 */
struct fltrinfo {
	LIST_ENTRY(fltrinfo)	next;		/* next entry on fltrlist
						   of classinfo */
	LIST_ENTRY(fltrinfo)	nextrule;	/* next entry on fltr_rules
						   of ifinfo */
	u_long			handle;		/* filter handle */
	char			*flname;	/* filter name, if specified */
	struct flow_filter	fltr;		/* filter value */
	struct classinfo	*clinfo;	/* back pointer to classinfo */

	/* for consistency check */
	int			line_no;	/* config file line number */
	int			dontwarn;	/* supress warning msg */
};

int do_command(FILE *infp);
int qcmd_enable(const char *ifname);
int qcmd_disable(const char *ifname);
int qcmd_delete_if(const char *ifname);
int qcmd_clear_hierarchy(const char *ifname);
int qcmd_enableall(void);
int qcmd_disableall(void);
int qcmd_config(void);
int qcmd_init(void);
int qcmd_clear(const char *ifname);
int qcmd_destroyall(void);
int qcmd_restart(void);
int qcmd_delete_class(const char *ifname, const char *clname);
int qcmd_add_filter(const char *ifname, const char *clname, const char *flname,
		    const struct flow_filter *fltr);
int qcmd_delete_filter(const char *ifname, const char *clname,
		       const char *flname);
int qcmd_tbr_register(const char *ifname, u_int rate, u_int size);
int qop_enable(struct ifinfo *ifinfo);
int qop_disable(struct ifinfo *ifinfo);
int qop_delete_if(struct ifinfo *ifinfo);
int qop_clear(struct ifinfo *ifinfo);

int qop_add_if(struct ifinfo **rp, const char *ifname, u_int bandwidth,
	       struct qdisc_ops *qdisc_ops, void *if_private);
int qop_delete_if(struct ifinfo *ifinfo);

int qop_add_class(struct classinfo **rp, const char *clname,
		  struct ifinfo *ifinfo, struct classinfo *parent,
		  void *class_private);
int qop_modify_class(struct classinfo *clinfo, void *arg);
int qop_delete_class(struct classinfo *clinfo);

int qop_add_filter(struct fltrinfo **rp,
		   struct classinfo *clinfo,
		   const char *flname,
		   const struct flow_filter *fltr,
		   struct fltrinfo **conflict);
int qop_delete_filter(struct fltrinfo *fltr);

int is_q_enabled(const char *ifname);
struct ifinfo *ifname2ifinfo(const char *ifname);
struct ifinfo *input_ifname2ifinfo(const char *ifname);
struct classinfo *clname2clinfo(const struct ifinfo *ifinfo,
				const char *clname);
struct classinfo * clhandle2clinfo(struct ifinfo *ifinfo, u_long handle);
struct fltrinfo *flname2flinfo(const struct classinfo *clinfo,
			       const char *flname);
struct fltrinfo *flhandle2fltrinfo(struct ifinfo *ifinfo, u_long handle);
void print_filter(const struct flow_filter *filt);
const char *qoperror(int qoperrno);
u_int get_ifindex(const char *ifname);
struct classinfo *get_rootclass(struct ifinfo *ifinfo);
struct classinfo *get_nextclass(struct classinfo *clinfo);
u_long atobps(const char *s);
u_long atobytes(const char *s);
int qop_red_set_defaults(int th_min, int th_max, int inv_pmax);
int qop_rio_set_defaults(struct redparams *params);
int open_module(const char *devname, int flags);
int client_input(FILE *fp);

/* misc system errors */
#define QOPERR_OK		0	/* no error */
#define QOPERR_SYSCALL		1	/* syscall err; see errno */
#define QOPERR_NOMEM		2	/* not enough memory */
#define QOPERR_INVAL		3	/* invalid parameter */
#define QOPERR_RANGE		4	/* out of range	*/
#define QOPERR_BADIF		5	/* bad interface name */
#define QOPERR_BADCLASS		6	/* bad class name */
#define QOPERR_BADFILTER	7	/* bad filter name */

/* class errors */
#define QOPERR_CLASS		8	/* class failure */
#define QOPERR_CLASS_INVAL	9	/* bad class value */
#define QOPERR_CLASS_PERM	10	/* class operation not permitted */

/* filter errors */
#define QOPERR_FILTER		11	/* filter failure */
#define QOPERR_FILTER_INVAL	12	/* bad filter value */
#define QOPERR_FILTER_SHADOW	13	/* shadows an existing filter */

/* addmission errors */
#define QOPERR_ADMISSION	14	/* admission control failure */
#define QOPERR_ADMISSION_NOBW	15	/* insufficient bandwidth */
#define QOPERR_ADMISSION_DELAY	16	/* cannot meet delay bound req */
#define QOPERR_ADMISSION_NOSVC	17	/* no service available */

/* policy errors */
#define QOPERR_POLICY		18	/* policy control failure */

#define QOPERR_MAX		18

extern int	filter_dontwarn;/* supress warning for the current filter */
extern char	*altqconfigfile;	/* config file name */
extern const char *qop_errlist[];	/* error string list */
extern struct qdisc_ops nop_qdisc;
extern char *cur_ifname(void);
extern struct qdisc_parser qdisc_parser[];
extern int	Debug_mode;

#ifndef RSVPD
/* rename LOG() to log_write() */
#define LOG	log_write
void log_write(int, int, const char *, ...);

/* stuff defined in rsvp headers */
#define IsDebug(type)  (l_debug >= LOG_DEBUG && (m_debug & (type)))
#define DEBUG_ALTQ	0x40

#define ntoh16(x)	((u_int16_t)ntohs((u_int16_t)(x)))
#define ntoh32(x)	((u_int32_t)ntohl((u_int32_t)(x)))
#define hton16(x)	((u_int16_t)htons((u_int16_t)(x)))
#define hton32(x)	((u_int32_t)htonl((u_int32_t)(x)))

extern int	if_num;		/* number of phyints */
extern int	m_debug;	/* Debug output control bits */
extern int	l_debug;	/* Logging severity level */
extern int	line_no;	/* current line number in config file */
extern int	daemonize;	/* log_write uses stderr if daemonize is 0 */

#endif /* !RSVPD */

#ifdef INET6
/* a macro to handle v6 address in 32-bit fields */
#define IN6ADDR32(a, i)	(*(u_int32_t *)(&(a)->s6_addr[(i)<<2]))
#endif

#endif /* _ALTQ_QOP_H_ */
