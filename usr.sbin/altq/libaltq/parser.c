/*	$OpenBSD: parser.c,v 1.1.1.1 2001/06/27 18:23:24 kjc Exp $	*/
/*	$KAME: parser.c,v 1.6 2001/05/30 10:30:44 kjc Exp $	*/
/*******************************************************************

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <altq/altq.h>
#include <altq/altq_cdnr.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>
#include "altq_qop.h"
#include "qop_cdnr.h"

#define show_help(op)  printf(cmd_tab[op].cmd_help)

/*
 * Forward & External Declarations
 */
static int is_qdisc_name(const char *qname);
static int qdisc_interface_parser(const char * qname, const char *ifname,
				  int argc, char **argv);
static int qdisc_class_parser(const char *qname, const char *ifname,
			      const char *class_name, const char *parent_name,
			      int argc, char **argv);

static int pfxcmp(const char *s1, const char *s2);
static int next_word(char **cpp, char *b);

static int do_cmd(int op, char *cmdbuf);
static int get_ifname(char **cpp, char **ifnamep);
static int get_addr(char **cpp, struct in_addr *addr, struct in_addr *mask);
static int get_port(const char *name, u_int16_t *port_no);
static int get_proto(const char *name, int *proto_no);
static int get_fltr_opts(char **cpp, char *fltr_name, int *ruleno);
static int interface_parser(char *cmdbuf);
static int class_parser(char *cmdbuf) ;
static int filter_parser(char *cmdbuf);
#ifdef INET6
static int filter6_parser(char *cmdbuf);
static int get_ip6addr(char **cpp, struct in6_addr *addr,
		       struct in6_addr *mask);
#endif
static int ctl_parser(char *cmdbuf);
static int delete_parser(char *cmdbuf);
static int red_parser(char *cmdbuf);
static int rio_parser(char *cmdbuf);
static int conditioner_parser(char *cmdbuf);
static int tc_action_parser(char *ifname, char **cpp,
			    struct tc_action *action);

/*
 * Globals
 */
#define MAX_NFLWDS      64
#define MAX_T           64

int             TNO = 1;  	/* Current Thread number */
int		line_no = 0;
int		filter_dontwarn;

static char	if_names[MAX_T][IFNAMSIZ]; 
static struct if_nameindex *if_namelist = NULL;

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

enum op_codes {
        /* order must be same as entries cmd_tab[].cmd_op below!! */
        OP_HELP = 1, 	OP_QUIT, 
	OP_IFACE,	OP_CLASS,	OP_FILTER,
	OP_ALTQ,		OP_DEL,		
#ifdef INET6
	OP_FILTER6,
#endif
	OP_RED,		OP_RIO,
	OP_CDNR,
        OP_NULL, 	OP_BUG
};

/*	Following table MUST match enum order of op_codes !
 */
struct cmds {
	char           *cmd_verb;
	int             cmd_op;
	char           *cmd_help;
}		cmd_tab[] = {

  	{ "?",		OP_HELP, 	"Commands are:\n" },
	{ "help",	OP_HELP, 	" help | ?\n" },
	{ "quit",	OP_QUIT, 	" quit\n" },
	{ "interface",	OP_IFACE,	" interface if_name [bandwidth bps] [cbq|hfsc]\n" },
	{ "class",	OP_CLASS,	" class discipline if_name class_name [parent]\n" },
	{ "filter",	OP_FILTER,	" filter if_name class_name [name filt_name] dst [netmask #] dport src [netmask #] sport proto [tos # [tosmask #] [gpi #] [dontwarn]\n" },
	{ "altq",	OP_ALTQ,	" disc if_name {enable|disable}\n" },
	{ "delete",	OP_DEL,		" delete if_name class_name\n" },
#ifdef INET6
	{ "filter6",	OP_FILTER6,	" filter6 if_name class_name [name filt_name] dst[/prefix] dport src[/prefix] sport proto [flowlabel #][tclass # [tclassmask #]][gpi #] [dontwarn]\n" },
#endif
	{ "red", 	OP_RED,		" red th_min th_max inv_pmax\n" },
	{ "rio", 	OP_RIO,		" rio low_th_min low_th_max low_inv_pmax med_th_min med_th_max med_inv_pmax high_th_min high_th_max high_inv_pmax\n" },
	{ "conditioner", OP_CDNR,	" conditioner if_name cdnr_name <tc_action>\n" },
	{ "bug",	OP_BUG,		" bug (On/Off)\n" },
	{ "",		OP_NULL,	"" } /* MUST BE LAST IN CMD TABLE */
};

static int 
is_qdisc_name(const char *qname)
{
	struct qdisc_parser *qp;
	
	for (qp = qdisc_parser; qp->qname != NULL; qp++)
		if (strncmp(qp->qname, qname, strlen(qp->qname)) == 0)
			return (1);
	return (0);
}

static int 
qdisc_interface_parser(const char * qname, const char *ifname,
		       int argc, char **argv)
{
	struct qdisc_parser *qp;
	
	for (qp = qdisc_parser; qp->qname != NULL; qp++)
		if (strncmp(qp->qname, qname, strlen(qp->qname)) == 0)
			return (*qp->interface_parser)(ifname, argc, argv);
	return (0);
}

static int 
qdisc_class_parser(const char *qname, const char *ifname,
		   const char *class_name, const char *parent_name,
		   int argc, char **argv)
{
	struct qdisc_parser *qp;
	struct ifinfo	*ifinfo;

	for (qp = qdisc_parser; qp->qname != NULL; qp++)
		if (strncmp(qp->qname, qname, strlen(qp->qname)) == 0) {
			if (qp->class_parser == NULL) {
				LOG(LOG_ERR, 0,
				    "class can't be specified for %s", qp->qname);
				return (0);
			}
			if ((ifinfo = ifname2ifinfo(ifname)) == NULL) {
				LOG(LOG_ERR, 0,
				    "no such interface, line %d\n", line_no);
				return (0);
			}
			if (strncmp(ifinfo->qdisc->qname, qname,
				    strlen(ifinfo->qdisc->qname)) != 0) {
				LOG(LOG_ERR, 0,
				    "qname doesn't match the interface, line %d\n",
				    line_no);
				return (0);
			}
			return (*qp->class_parser)(ifname, class_name,
						   parent_name, argc, argv);
		}
	return (0);
}


/*
 * Read the config file to learn about tunnel vifs and non-default phyint
 * parameters.
 */
int
qcmd_config(void)
{
	FILE		*f;
	int		i, rc = 1;

	if (if_namelist != NULL)
		if_freenameindex(if_namelist);
	if_namelist = if_nameindex();

	for (i = 0; i < MAX_T; i++)
		if_names[i][0] = '\0';

	LOG(LOG_INFO, 0, "ALTQ config file is %s\n", altqconfigfile);

	f = fopen(altqconfigfile, "r");
	if (f == NULL) {
		LOG(LOG_ERR, errno, "Can't open %s", altqconfigfile, 0);
		return (QOPERR_INVAL);
	}
	line_no = 0;
	while (rc)
		rc = DoCommand(altqconfigfile, f);

	(void) fclose(f);
	line_no = 0;
	return (0);
}

/*
 *  Do_Command(): Top-level routine to read the next line from a given
 *	file and execute the command it contains.
 *	returns 1 if OK, 0 if EOF.
 */
int
DoCommand(char *infile, FILE *infp)
{
	char	cmd_line[256], cmd_op[80];
	struct	cmds *cmdp;
	char	*cp;
	int	rc;

	if (fgets(cmd_line, sizeof(cmd_line), infp) == NULL)
		/* EOF */
		return(0);
	line_no++;

	/* check escaped newline */
	while ((cp = strrchr(cmd_line, '\\')) != NULL && cp[1] == '\n') {
		if (fgets(cp, &cmd_line[256] - cp, infp) != NULL)
			line_no++;
	}

	/* remove trailing NL */
	cp = cmd_line + strlen(cmd_line) - 1;
	if (*cp == '\n')
		*cp = '\0';
	else if (!feof(infp)) {
		printf("LINE %d > 255 CHARS: %s.\n", line_no, cmd_line);
		exit(1);
	}
	/*** printf("DoCommand: %s\n", cmd_line); ***/

	if (cmd_line[0] == '#') {	/* Comment, skip this line */
		return(1);
	}
	cp = cmd_line;
	if (!next_word(&cp, cmd_op))
		return(1);
	if (cmd_op[0] == 'T') {
		TNO = atoi(&cmd_op[1]);
		if (!next_word(&cp, cmd_op))
			return(1);
	}
	cmdp = cmd_tab;
	while ((cmdp->cmd_op != OP_NULL) && pfxcmp(cmd_op, cmdp->cmd_verb))
		cmdp++;

	if (cmdp->cmd_op == OP_NULL) {
		if (cmd_op[0])
			printf(" ?? %s\n", cmd_op);
		return(1);
	}
	rc = do_cmd(cmdp->cmd_op, cp);
	if (rc == 0) {
		if (infile) {
			/* error in the config file.  cleanup and exit. */
			LOG(LOG_ERR, 0, "Config failed. Exiting.\n");
			(void) qcmd_destroyall();
			(void) fclose(infp);
			exit(1);
		} else {
			/* interactive mode */
			printf("error: usage :");
			show_help(cmdp->cmd_op);
		}
	}
	return(1);
}


/*
 * Prefix string comparison: Return 0 if s1 string is prefix of s2 string, 1
 * otherwise.
 */
static int 
pfxcmp(const char *s1, const char *s2)
{
	while (*s1)
		if (*s1++ != *s2++)
			return (1);
	return (0);
}

/*
 * Skip leading blanks, then copy next word (delimited by blank or zero, but
 * no longer than 63 bytes) into buffer b, set scan pointer to following 
 * non-blank (or end of string), and return 1.  If there is no non-blank text,
 * set scan ptr to point to 0 byte and return 0.
 */
static int 
next_word(char **cpp, char *b)
{
	char           *tp;
	size_t		L;

	*cpp += strspn(*cpp, " \t");
	if (**cpp == '\0' || **cpp == '\n' || **cpp == '#')
		return(0);

	tp = strpbrk(*cpp, " \t\n#");
	L = MIN((tp)?(tp-*cpp):strlen(*cpp), 63);
	strncpy(b, *cpp, L);
	*(b + L) = '\0';
	*cpp += L;
	*cpp += strspn(*cpp, " \t");
	return (1);
}

/*
 * do_cmd executes a command input.
 * returns 1 if OK, 0 if an error occurs.
 */
static int
do_cmd(int op, char *cmdbuf)
{
	int i, rval = 0;
    
	switch (op) {
	case OP_HELP:
		for (i = 0; i < OP_NULL; i++)
			show_help(i);
		rval = 1;
		break;
	case OP_QUIT:
		qcmd_destroyall();
		exit(0);
		break;
	case OP_IFACE:
		rval = interface_parser(cmdbuf);
		break;
	case OP_CLASS:
		rval = class_parser(cmdbuf);
		break;
	case OP_FILTER:
		rval = filter_parser(cmdbuf);
		break;
	case OP_ALTQ:
		rval = ctl_parser(cmdbuf);
		break;
	case OP_DEL:
		rval = delete_parser(cmdbuf);
		break;
#ifdef INET6
	case OP_FILTER6:
		rval = filter6_parser(cmdbuf);
		break;
#endif
	case OP_RED:
		rval = red_parser(cmdbuf);
		break;
	case OP_RIO:
		rval = rio_parser(cmdbuf);
		break;
	case OP_CDNR:
		rval = conditioner_parser(cmdbuf);
		break;
	case OP_BUG:
		if (m_debug & DEBUG_ALTQ) {
			/* turn off verbose */
			l_debug = LOG_INFO;
			m_debug &= ~DEBUG_ALTQ;
		} else {
			/* turn on verbose */
			l_debug = LOG_DEBUG;
			m_debug |= DEBUG_ALTQ;
		}
		break;
	default:
		printf("command %d not supported\n", op);
		rval = 0;
		break;
	}
	return(rval);
}

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

char *cur_ifname(void)
{
	return (if_names[TNO]);
}

u_int
get_ifindex(const char *ifname)
{
	struct if_nameindex *ifnp;

	for (ifnp = if_namelist; ifnp->if_name != NULL; ifnp++)
		if (strcmp(ifname, ifnp->if_name) == 0)
			return (ifnp->if_index);
	return (0);
}

static int
get_ifname(char **cpp, char **ifnamep)
{
	char w[128], *ocp;
	struct if_nameindex *ifnp;

	ocp = *cpp;
	if (next_word(&ocp, w) && if_namelist != NULL)
		for (ifnp = if_namelist; ifnp->if_name != NULL; ifnp++)
			if (strcmp(w, ifnp->if_name) == 0) {
				/* if_name found. advance the word pointer */
				*cpp = ocp; 
				strcpy(if_names[TNO], w);
				*ifnamep = if_names[TNO];
				return (1);
			}

	/* this is not interface name. use one in the context. */
	if (if_names[TNO][0] == 0)
		return (0);
	*ifnamep = if_names[TNO];
	return (1);
}

/* set address and netmask in network byte order */
static int
get_addr(char **cpp, struct in_addr *addr, struct in_addr *mask)
{
	char w[128], *ocp;
	u_long tmp;
	
	addr->s_addr = 0;
	mask->s_addr = 0xffffffff;

	if (!next_word(cpp, w))
		return (0);

	if ((tmp = inet_addr((char *)w)) == INADDR_NONE) {
		/* try gethostbyname */
		struct hostent *h;

		if ((h = gethostbyname(w)) == NULL
		    || h->h_addrtype != AF_INET || h->h_length != 4)
			return (0);
		
		bcopy(h->h_addr, &tmp, (size_t)h->h_length);
	}

	addr->s_addr = tmp;

	/* check if netmask option is present */
	ocp = *cpp;
	if (next_word(&ocp, w) && EQUAL(w, "netmask")) {
		if (!next_word(&ocp, w))
			return (0);
		
		if (inet_aton((char *)w, (struct in_addr *)&tmp) == 0)
			return (0);

		mask->s_addr = tmp;
		*cpp = ocp;	
		return (1);
	}
	/* no netmask option */
	return (1);
}

/* returns service number in network byte order */
static int
get_port(const char *name, u_int16_t *port_no)
{
	struct servent *s;
	u_int16_t num;
	
	if (isdigit(name[0])) {
		num = (u_int16_t)strtol(name, NULL, 0);
		*port_no = htons(num);
		return (1);
	}

	if ((s = getservbyname(name, 0)) == NULL)
		return (0);

	*port_no = (u_int16_t)s->s_port;
	return (1);
}

static int
get_proto(const char *name, int *proto_no)
{
	struct protoent *p;
	
	if (isdigit(name[0])) {
		*proto_no = (int)strtol(name, NULL, 0);
		return (1);
	}

	if ((p = getprotobyname(name)) == NULL)
		return (0);

	*proto_no = p->p_proto;
	return (1);
}

static int
get_fltr_opts(char **cpp, char *fltr_name, int *ruleno)
{
	char w[128], *ocp;

	ocp = *cpp;
	while (next_word(&ocp, w)) {
		if (EQUAL(w, "name")) {
			if (!next_word(&ocp, w))
				return (0);
			strcpy(fltr_name, w);
			*cpp = ocp;
		} else if (EQUAL(w, "ruleno")) {
			if (!next_word(&ocp, w))
				return (0);
			*ruleno = (int)strtol(w, NULL, 0);
			*cpp = ocp;
		} else
			break;
	}
	return (1);
}


#define	DISCIPLINE_NONE		0

static int
interface_parser(char *cmdbuf)
{
	char	w[256], *ap, *cp = cmdbuf;
	char	*ifname, *argv[64], qdisc_name[64];
	int     argc, rval;
    
	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/*
	 * Create argment list & look for scheduling discipline options.
	 */
	sprintf(qdisc_name, "null");
	argc = 0;
	ap = w;
	while (next_word(&cp, ap)) {
		if (is_qdisc_name(ap))
			strcpy(qdisc_name, ap);

		argv[argc] = ap;
		ap += strlen(ap) + 1;
		argc++;
	}

	rval = qdisc_interface_parser(qdisc_name, ifname, argc, argv);
	if (rval == 0) {
		LOG(LOG_ERR, 0, "Error in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	return (1);
}

static int
class_parser(char *cmdbuf) 
{
	char	w[256], *cp = cmdbuf;
	char 	*ifname, qdisc_name[128], class_name[128], parent_name[128];
	char	*clname = class_name;
	char	*parent = NULL;
	char	*argv[64], *ap;
	int	argc, rval;

	/* get scheduling class */
	if (!next_word(&cp, qdisc_name)) {
		LOG(LOG_ERR, 0, "missing scheduling discipline in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!is_qdisc_name(qdisc_name)) {
		LOG(LOG_ERR, 0,
		    "unknown scheduling discipline '%s' in %s, line %d\n",
		    qdisc_name, altqconfigfile, line_no);
		return (0);
	}

	/* get interface name */
	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get class name */
	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0, "missing class name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get parent name */
	if (!next_word(&cp, parent_name)) {
		LOG(LOG_ERR, 0, "missing parent class in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!EQUAL(parent_name, "null") && !EQUAL(parent_name, "NULL")) {
		parent = parent_name;
	} else {
		parent = NULL;
	}

	ap = w;
	argc = 0;
	while (next_word(&cp, ap)) {
		argv[argc] = ap;
		ap += strlen(ap) + 1;
		argc++;
	}

	rval = qdisc_class_parser(qdisc_name, ifname, clname, parent,
				 argc, argv);
    	if (rval == 0) {
		LOG(LOG_ERR, 0, "can't add class '%s' on interface '%s'\n",
		    clname, ifname);
		return (0);
	}
    
	return (1);
}

static int
filter_parser(char *cmdbuf) 
{
	char 	w[128], *cp = cmdbuf;
	char 	*ifname, class_name[64], fltr_name[64], *flname = NULL;
	struct flow_filter	sfilt;
	int	protocol;
	u_char	tos, tosmask;
	int	ruleno;
	int	dontwarn = 0;
	int	error;

	memset(&sfilt, 0, sizeof(sfilt));
	sfilt.ff_flow.fi_family = AF_INET;

	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0,
		    "missing class name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	fltr_name[0] = '\0';
	ruleno = 0;
	if (!get_fltr_opts(&cp, &fltr_name[0], &ruleno)) {
		LOG(LOG_ERR, 0,
		    "bad filter option in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (fltr_name[0] != '\0')
		flname = fltr_name;
	sfilt.ff_ruleno = ruleno;

	/* get filter destination Address */
	if (!get_addr(&cp, &sfilt.ff_flow.fi_dst, &sfilt.ff_mask.mask_dst)) {
		LOG(LOG_ERR, 0,
		    "bad filter destination address in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get filter destination port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0,
		    "missing filter destination port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow.fi_dport)) {
		LOG(LOG_ERR, 0, "bad filter destination port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
    
	/* get filter source address */
	if (!get_addr(&cp, &sfilt.ff_flow.fi_src, &sfilt.ff_mask.mask_src)) {
		LOG(LOG_ERR, 0, "bad filter source address in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get filter source port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter source port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow.fi_sport)) {
		LOG(LOG_ERR, 0, "bad filter source port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get filter protocol id */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter protocol id in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!get_proto(w, &protocol)) {
		LOG(LOG_ERR, 0, "bad protocol in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	sfilt.ff_flow.fi_proto = protocol; 

	while (next_word(&cp, w)) {
		if (EQUAL(w, "tos")) {
			tos = 0;
			tosmask = 0xff;
			
			if (next_word(&cp, w)) {
				tos = (u_char)strtol(w, NULL, 0);
				if (next_word(&cp, w)) {
					if (EQUAL(w, "tosmask")) {
						next_word(&cp, w);
						tosmask = (u_char)strtol(w, NULL, 0);
					}
				}
			}
			sfilt.ff_flow.fi_tos = tos; 
			sfilt.ff_mask.mask_tos = tosmask; 
		} else if (EQUAL(w, "gpi")) {
			if (next_word(&cp, w)) {
				sfilt.ff_flow.fi_gpi =
					(u_int32_t)strtoul(w, NULL, 0);
				sfilt.ff_flow.fi_gpi =
					htonl(sfilt.ff_flow.fi_gpi);
			}
		} else if (EQUAL(w, "dontwarn"))
			dontwarn = 1;
	}

	/*
	 * Add the filter.
	 */
	filter_dontwarn = dontwarn;	/* XXX */
	error = qcmd_add_filter(ifname, class_name, flname, &sfilt);
	filter_dontwarn = 0;		/* XXX */
	if (error) {
		LOG(LOG_ERR, 0,
		    "can't add filter to class '%s' on interface '%s'\n",
		    class_name, ifname);
		return (0);
	}

	return (1);
}

#ifdef INET6
static int
filter6_parser(char *cmdbuf)
{
	char 	w[128], *cp = cmdbuf;
	char 	*ifname, class_name[128], fltr_name[64], *flname = NULL;
	struct flow_filter6	sfilt;
	int	protocol;
	u_char	tclass, tclassmask;
	int	ruleno;
	int	dontwarn = 0;
	int	ret;

	memset(&sfilt, 0, sizeof(sfilt));
	sfilt.ff_flow6.fi6_family = AF_INET6;

	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0, "missing class name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	fltr_name[0] = '\0';
	ruleno = 0;
	if (!get_fltr_opts(&cp, &fltr_name[0], &ruleno)) {
		LOG(LOG_ERR, 0,
		    "bad filter option in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (fltr_name[0] != '\0')
		flname = fltr_name;
	sfilt.ff_ruleno = ruleno;

	/* get filter destination address */
	if (!get_ip6addr(&cp, &sfilt.ff_flow6.fi6_dst,
			 &sfilt.ff_mask6.mask6_dst)) {
		LOG(LOG_ERR, 0, "bad destination address in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	
	/* get filter destination port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0,
		    "missing filter destination port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow6.fi6_dport)) {
		LOG(LOG_ERR, 0, "bad filter destination port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
    
	/* get filter source address */
	if (!get_ip6addr(&cp, &sfilt.ff_flow6.fi6_src,
			 &sfilt.ff_mask6.mask6_src)) {
		LOG(LOG_ERR, 0, "bad source address in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get filter source port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter source port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow6.fi6_sport)) {
		LOG(LOG_ERR, 0, "bad filter source port in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get filter protocol id */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter protocol id in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	if (!get_proto(w, &protocol)) {
		LOG(LOG_ERR, 0, "bad protocol in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	sfilt.ff_flow6.fi6_proto = protocol; 

	while (next_word(&cp, w)) {
		if (EQUAL(w, "tclass")) {
			tclass = 0;
			tclassmask = 0xff;

			if (next_word(&cp, w)) {
				tclass = (u_char)strtol(w, NULL, 0);
				if (next_word(&cp, w)) {
					if (EQUAL(w, "tclassmask")) {
						next_word(&cp, w);
						tclassmask =
						    (u_char)strtol(w, NULL, 0);
					}
				}
			}
			sfilt.ff_flow6.fi6_tclass = tclass; 
			sfilt.ff_mask6.mask6_tclass = tclassmask; 
		} else if (EQUAL(w, "gpi")) {
			if (next_word(&cp, w)) {
				sfilt.ff_flow6.fi6_gpi =
					(u_int32_t)strtoul(w, NULL, 0);
				sfilt.ff_flow6.fi6_gpi =
					htonl(sfilt.ff_flow6.fi6_gpi);
			}
		} else if (EQUAL(w, "flowlabel")) {
			if (next_word(&cp, w)) {
				sfilt.ff_flow6.fi6_flowlabel =
				   (u_int32_t)strtoul(w, NULL, 0) & 0x000fffff;
				sfilt.ff_flow6.fi6_flowlabel =
					htonl(sfilt.ff_flow6.fi6_flowlabel);
			}
		} else if (EQUAL(w, "dontwarn"))
			dontwarn = 1;
	}

	/*
	 * Add the filter.
	 */
	filter_dontwarn = dontwarn;	/* XXX */
	ret = qcmd_add_filter(ifname, class_name, flname,
			      (struct flow_filter *)&sfilt);
	filter_dontwarn = 0;		/* XXX */
	if (ret) {
		LOG(LOG_ERR, 0,
		    "can't add filter to class '%s' on interface '%s'\n",
		    class_name, ifname);
		return (0);
	}

	return (1);
}

static int
get_ip6addr(char **cpp, struct in6_addr *addr, struct in6_addr *mask)
{
	char w[128], *prefix;
	u_char *cp;
	int len;

	*addr = in6addr_any;  /* set all 0 */
	*mask = in6addr_any;  /* set all 0 */

	if (!next_word(cpp, w))
		return (0);

	if (EQUAL(w, "0"))
		/* abbreviation of a wildcard (::0) */
		return (1);

	if ((prefix = strchr(w, '/')) != NULL) {
		/* address has prefix length */
		*prefix++ = '\0';
	}

	if (inet_pton(AF_INET6, w, addr) <= 0)
		return (0);

	if (IN6_IS_ADDR_UNSPECIFIED(addr) && prefix == NULL)
		/* wildcard */
		return (1);

	/* convert address prefix length to address mask */
	if (prefix != NULL) {
		len = (int)strtol(prefix, NULL, 0);
		if ((len < 0) || (len > 128))
			return (0);
		for (cp = (u_char *)mask; len > 7; len -= 8)
			*cp++ = 0xff;
		if (len > 0)
			*cp = (0xff << (8 - len)) & 0xff;

		IN6ADDR32(addr, 0) &= IN6ADDR32(mask, 0);
		IN6ADDR32(addr, 1) &= IN6ADDR32(mask, 1);
		IN6ADDR32(addr, 2) &= IN6ADDR32(mask, 2);
		IN6ADDR32(addr, 3) &= IN6ADDR32(mask, 3);
	} else
		/* full mask */
		memset(mask, 0xff, sizeof(struct in6_addr));

	return (1);
}

#endif /* INET6 */

static int
ctl_parser(char *cmdbuf)
{
	char	w[128], *cp = cmdbuf;
	char	*ifname;
	int	state;
	int	rval;
    
	if (!get_ifname(&cp, &ifname)) {
		printf("missing interface name in %s, line %d",
		       altqconfigfile, line_no);
		return (0);
	}

	if (!next_word(&cp, w)) {
		state = is_q_enabled(ifname);
		printf("altq %s on %s\n",
		       state ? "enabled" : "disabled", ifname);
		return (1);
	}
	
	if (EQUAL(w, "enable")) {
		rval = qcmd_enable(ifname);
		printf("altq %s on %s\n",
		       (rval == 0) ? "enabled" : "enable failed!", ifname);
	} else if (EQUAL(w, "disable")) {
		rval = qcmd_disable(ifname);
		printf("altq %s on %s\n",
		       (rval == 0) ? "disabled" : "disable failed!", ifname);
	} else if (EQUAL(w, "reload")) {
		printf("reinitializing altq...\n");
		qcmd_destroyall();
		qcmd_init();
	} else
		return (0);
	return (1);
}


static int
delete_parser(char *cmdbuf)
{
	char	*cp = cmdbuf;
	char	*ifname, class_name[128];
	int	ret;
    
	if (!get_ifname(&cp, &ifname)) {
		printf("missing interface name in %s, line %d",
		       altqconfigfile, line_no);
		return (0);
	}

	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0,
		    "missing class name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	ret = qcmd_delete_class(ifname, class_name);
	if (ret) {
		LOG(LOG_ERR, 0,
		    "can't delete class '%s' on interface '%s'\n",
		    class_name, ifname);
		return (0);
	}

	return (1);
}

static int
red_parser(char *cmdbuf)
{
	char	w[128], *cp = cmdbuf;
	int th_min, th_max, inv_pmax;

	if (!next_word(&cp, w))
		goto bad;
	th_min = (int)strtol(w, NULL, 0);

	if (!next_word(&cp, w))
		goto bad;
	th_max = (int)strtol(w, NULL, 0);

	if (!next_word(&cp, w))
		goto bad;
	inv_pmax = (int)strtol(w, NULL, 0);

	if (qop_red_set_defaults(th_min, th_max, inv_pmax) != 0) {
		LOG(LOG_ERR, 0, "can't set red default parameters\n");
		return (0);
	}

	return (1);

 bad:
	LOG(LOG_ERR, 0, "bad red parameter in %s, line %d\n",
	    altqconfigfile, line_no);
	return (0);
}

static int
rio_parser(char *cmdbuf)
{
	char	w[128], *cp = cmdbuf;
	int	i;
	struct redparams params[RIO_NDROPPREC];

	for (i = 0; i < RIO_NDROPPREC; i++) {
		if (!next_word(&cp, w))
			goto bad;
		params[i].th_min = (int)strtol(w, NULL, 0);

		if (!next_word(&cp, w))
			goto bad;
		params[i].th_max = (int)strtol(w, NULL, 0);

		if (!next_word(&cp, w))
			goto bad;
		params[i].inv_pmax = (int)strtol(w, NULL, 0);
	}

	if (qop_rio_set_defaults(&params[0]) != 0) {
		LOG(LOG_ERR, 0, "can't set rio default parameters\n");
		return (0);
	}

	return (1);

 bad:
	LOG(LOG_ERR, 0, "bad rio parameter in %s, line %d\n",
	    altqconfigfile, line_no);
	return (0);
}

static int
conditioner_parser(char *cmdbuf)
{
	char	cdnr_name[128], *cp = cmdbuf;
	char	*ifname;
	struct tc_action action[64];
		
	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	/* get conditioner name */
	if (!next_word(&cp, cdnr_name)) {
		LOG(LOG_ERR, 0, "missing cdnr name in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}

	if (tc_action_parser(ifname, &cp, &action[0]) == 0)
		return (0);

	if (qcmd_cdnr_add_element(NULL, ifname, cdnr_name, &action[0]) != 0)
		return (0);
	return (1);
}

/*
 * recursively parse '<'tc_action'>'
 * note that array "action" grows during recursive parse.
 */
static int
tc_action_parser(char *ifname, char **cpp, struct tc_action *action)
{
	char	*cp, *start, *end;
	char	type[128], w[128];
	int	depth, i;
	struct tb_profile profile[2];
	
	/*
	 * find a possibly nested pair of '<' and '>',
	 * make them pointed by 'start' and 'end'.
	 */
	start = strchr(*cpp, '<');
	if (start == NULL) {
		LOG(LOG_ERR, 0, "conditioner action missing in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	depth = 1;
	cp = start + 1;
	do {
		end = strpbrk(cp, "<>");
		if (end == NULL) {
			LOG(LOG_ERR, 0,
			    "conditioner action delimiter mismatch in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		if (*end == '<')
			depth++;
		else if (*end == '>')
			depth--;
		cp = end + 1;
	} while (depth > 0);
	*end = '\0';
	*cpp = end + 1;
	cp = start + 1;

	if (IsDebug(DEBUG_ALTQ)) {
		printf("tc_action_parser: [%s]\n", cp);
	}

	if (!next_word(&cp, type)) {
		LOG(LOG_ERR, 0,
		    "missing conditioner action type in %s, line %d\n",
		    altqconfigfile, line_no);
		return (0);
	}
	
	/*
	 * action type specific process
	 */
	if (EQUAL(type, "conditioner")) {
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0,
			    "missing conditioner name in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		action->tca_code = TCACODE_HANDLE;
		action->tca_handle = cdnr_name2handle(ifname, w);
		if (action->tca_handle == CDNR_NULL_HANDLE) {
			LOG(LOG_ERR, 0,
			    "wrong conditioner name %s in %s, line %d\n",
			    w, altqconfigfile, line_no);
			return (0);
		}
	} else if (EQUAL(type, "pass")) {
		action->tca_code = TCACODE_PASS;
	} else if (EQUAL(type, "drop")) {
		action->tca_code = TCACODE_DROP;
	} else if (EQUAL(type, "mark")) {
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing dscp in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		action->tca_code = TCACODE_MARK;
		action->tca_dscp = (u_int8_t)strtol(w, NULL, 0);
	} else if (EQUAL(type, "tbmeter")) {
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing tb profile in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		profile[0].rate = atobps(w);
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing tb profile in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		profile[0].depth = atobytes(w);
		if (tc_action_parser(ifname, &cp, &action[1]) == 0)
			return (0);
		if (tc_action_parser(ifname, &cp, &action[2]) == 0)
			return (0);

		if (qcmd_cdnr_add_tbmeter(action, ifname, NULL, &profile[0],
					  &action[1], &action[2]) != 0)
			return (0);
	} else if (EQUAL(type, "trtcm")) {
		int coloraware = 0;	/* default is color-blind */

		for (i=0; i<2; i++) {
			if (!next_word(&cp, w)) {
				LOG(LOG_ERR, 0,
				    "missing tb profile in %s, line %d\n",
				    altqconfigfile, line_no);
				return (0);
			}
			profile[i].rate = atobps(w);
			if (!next_word(&cp, w)) {
				LOG(LOG_ERR, 0,
				    "missing tb profile in %s, line %d\n",
				    altqconfigfile, line_no);
				return (0);
			}
			profile[i].depth = atobytes(w);
		}
		if (tc_action_parser(ifname, &cp, &action[1]) == 0)
			return (0);
		if (tc_action_parser(ifname, &cp, &action[2]) == 0)
			return (0);
		if (tc_action_parser(ifname, &cp, &action[3]) == 0)
			return (0);
		if (next_word(&cp, w)) {
			if (EQUAL(w, "coloraware"))
				coloraware = 1;
			else if (EQUAL(w, "colorblind"))
				coloraware = 0;
		}

		if (qcmd_cdnr_add_trtcm(action, ifname, NULL,
					&profile[0], &profile[1],
					&action[1], &action[2], &action[3],
					coloraware) != 0)
			return (0);
	} else if (EQUAL(type, "tswtcm")) {
		u_int32_t cmtd_rate, peak_rate, avg_interval;
		
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing cmtd rate in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		cmtd_rate = atobps(w);

		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing peak rate in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		peak_rate = atobps(w);

		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing avg interval in %s, line %d\n",
			    altqconfigfile, line_no);
			return (0);
		}
		avg_interval = (u_int32_t)strtoul(w, NULL, 0);

		if (tc_action_parser(ifname, &cp, &action[1]) == 0)
			return (0);
		if (tc_action_parser(ifname, &cp, &action[2]) == 0)
			return (0);
		if (tc_action_parser(ifname, &cp, &action[3]) == 0)
			return (0);

		if (qcmd_cdnr_add_tswtcm(action, ifname, NULL,
					 cmtd_rate, peak_rate, avg_interval,
					 &action[1], &action[2], &action[3])
		    != 0)
			return (0);
	} else {
		LOG(LOG_ERR, 0,
		    "Unkown action type %s in %s, line %d\n",
		    type, altqconfigfile, line_no);
		return (0);
	}
	    
	*end = '>';	/* restore the end delimiter */

	return (1);
}

