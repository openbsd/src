/*	$OpenBSD: parser.c,v 1.5 2002/02/13 08:21:45 kjc Exp $	*/
/*	$KAME: parser.c,v 1.13 2002/02/12 10:14:01 kjc Exp $	*/
/*
 * Copyright (C) 1999-2002
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

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <netdb.h>

#include <altq/altq.h>
#include <altq/altq_cdnr.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>
#include "altq_qop.h"
#include "qop_cdnr.h"

static int is_qdisc_name(const char *);
static int qdisc_interface_parser(const char *, const char *, int, char **);
static int qdisc_class_parser(const char *, const char *, const char *,
			      const char *, int, char **);
static int next_word(char **, char *);

static int do_cmd(int, char *);
static int get_ifname(char **, char **);
static int get_addr(char **, struct in_addr *, struct in_addr *);
static int get_port(const char *, u_int16_t *);
static int get_proto(const char *, int *);
static int get_fltr_opts(char **, char *, size_t, int *);
static int interface_parser(char *);
static int class_parser(char *) ;
static int filter_parser(char *);
#ifdef INET6
static int filter6_parser(char *);
static int get_ip6addr(char **, struct in6_addr *, struct in6_addr *);
#endif
static int ctl_parser(char *);
static int delete_parser(char *);
static int red_parser(char *);
static int rio_parser(char *);
static int conditioner_parser(char *);
static int tc_action_parser(char *, char **, struct tc_action *);

#define MAX_LINE	1024
#define MAX_WORD	64
#define MAX_ARGS	64
#define MAX_ACTIONS	16

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

int	line_no = 0;
int	filter_dontwarn;

static char	curifname[IFNAMSIZ];
static struct if_nameindex *if_namelist = NULL;

struct cmd_tab {
	const char	*cmd;
	int		(*parser)(char *);
	const char	*help;
} cmd_tab[] = {
	{"help",	NULL,	"help | ?"},
	{"quit",	NULL,	"quit"},
	{"interface",	interface_parser,	"interface if_name [bandwidth bps] [cbq|hfsc]"},
	{"class",	class_parser,	"class discipline if_name class_name [parent]"},
	{"filter",	filter_parser,	"filter if_name class_name [name filt_name] dst [netmask #] dport src [netmask #] sport proto [tos # [tosmask #] [gpi #] [dontwarn]"},
	{"altq",	ctl_parser,	"altq if_name {enable|disable}"},
	{"delete",	delete_parser,	"delete if_name class_name [filter_name]"},
#ifdef INET6
	{"filter6",	filter6_parser,	"filter6 if_name class_name [name filt_name] dst[/prefix] dport src[/prefix] sport proto [flowlabel #][tclass # [tclassmask #]][gpi #] [dontwarn]"},
#endif
	{"red",		red_parser,	"red th_min th_max inv_pmax"},
	{"rio",		rio_parser,	"rio low_th_min low_th_max low_inv_pmax med_th_min med_th_max med_inv_pmax high_th_min high_th_max high_inv_pmax"},
	{"conditioner",	conditioner_parser,	"conditioner if_name cdnr_name <tc_action>"},
	{"debug",	NULL,		"debug"},
	{NULL,		NULL,		NULL}	/* termination */
};

/*
 * read one line from the specified stream. if it's a command,
 * execute the command.
 * returns 1 if OK, 0 if error or EOF.
 */
do_command(FILE *fp)
{
	char	cmd_line[MAX_LINE], cmd[MAX_WORD], *cp;
	struct cmd_tab *tp;
	int	len, rval;

	/*
	 * read a line from the stream and make it a null-terminated string
	 */
	cp = cmd_line;
read_line:
	if (fgets(cp, &cmd_line[MAX_LINE] - cp, fp) == NULL)
		/* EOF or error */
		return(0);
	line_no++;

	/* null-terminate the line */
	if ((len = strlen(cmd_line)) > 0) {
		cp = cmd_line + len - 1;
		if (*cp == '\n') {
			/* if escaped newline, read next line */
			if (len > 1 &&  *(cp - 1) == '\\')
				goto read_line;
			*cp = '\0';
		} else if (!feof(fp))
			err(1, "LINE %d too long!", line_no);
	}
	/* trim comments */
	if ((cp = strchr(cmd_line, '#')) != NULL)
		*cp = '\0';

	cp = cmd_line;
	if ((len = next_word(&cp, cmd)) == 0)
		/* no command in this line */
		return (1);

	/* fnind the corresponding parser */
	rval = 0;
	for (tp = cmd_tab; tp->cmd != NULL; tp++)
		if (strncmp(cmd, tp->cmd, len) == 0)
			break;

	if (tp->cmd == NULL) {
		if (fp == stdin) {
			printf(" ?? %s\n", cmd);
			rval = 1;
		} else
			LOG(LOG_ERR, 0, "unknown command: %s", cmd);
		return (rval);
	}

	if (tp->parser != NULL)
		rval = (*tp->parser)(cp);
	else {
		/* handle other commands */
		if (strcmp(tp->cmd, "quit") == 0)
			rval = 0;
		else if (strcmp(tp->cmd, "help") == 0 ||
			 strcmp(tp->cmd, "?") == 0) {
			for (tp = cmd_tab; tp->cmd != NULL; tp++)
				printf("%s\n", tp->help);
			rval = 1;
		} else if (strcmp(tp->cmd, "debug") == 0) {
			if (m_debug & DEBUG_ALTQ) {
				/* turn off verbose */
				l_debug = LOG_INFO;
				m_debug &= ~DEBUG_ALTQ;
			} else {
				/* turn on verbose */
				l_debug = LOG_DEBUG;
				m_debug |= DEBUG_ALTQ;
			}
			rval = 1;
		}
	}
	return (rval);
}

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
				LOG(LOG_ERR, 0, "no such interface");
				return (0);
			}
			if (strncmp(ifinfo->qdisc->qname, qname,
				    strlen(ifinfo->qdisc->qname)) != 0) {
				LOG(LOG_ERR, 0,
				    "qname doesn't match the interface");
				return (0);
			}
			return (*qp->class_parser)(ifname, class_name,
						   parent_name, argc, argv);
		}
	return (0);
}

/*
 * read the config file
 */
int
qcmd_config(void)
{
	FILE	*fp;
	int	i, rval;

	if (if_namelist != NULL)
		if_freenameindex(if_namelist);
	if_namelist = if_nameindex();
	curifname[0] = '\0';

	LOG(LOG_INFO, 0, "ALTQ config file is %s", altqconfigfile);

	fp = fopen(altqconfigfile, "r");
	if (fp == NULL) {
		LOG(LOG_ERR, errno, "can't open %s", altqconfigfile, 0);
		return (QOPERR_INVAL);
	}
	line_no = 0;
	while (rval)
		rval = do_command(fp);

	if (!feof(fp)) {
		LOG(LOG_ERR, 0, "Error in %s, line %d.  config failed.",
		    altqconfigfile, line_no);
		(void) qcmd_destroyall();
		rval = QOPERR_INVAL;
	} else
		rval = 0;

	(void)fclose(fp);
	line_no = 0;
	return (rval);
}

static int 
next_word(char **cpp, char *b)
{
	char	*cp;
	int	i;

	cp = *cpp;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	for (i = 0; i < MAX_WORD - 1; i++) {
		if (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\0')
			break;
		*b++ = *cp++;
	}
	*b = '\0';
	*cpp = cp;
	return (i);
}

char *
cur_ifname(void)
{
	return (curifname);
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
	char w[MAX_WORD], *ocp;
	struct if_nameindex *ifnp;

	ocp = *cpp;
	if (next_word(&ocp, w) && if_namelist != NULL)
		for (ifnp = if_namelist; ifnp->if_name != NULL; ifnp++)
			if (strcmp(w, ifnp->if_name) == 0) {
				/* if_name found. advance the word pointer */
				*cpp = ocp;
				strlcpy(curifname, w, sizeof(curifname));
				*ifnamep = curifname;
				return (1);
			}

	/* this is not interface name. use one in the context. */
	if (curifname[0] == '\0')
		return (0);
	*ifnamep = curifname;
	return (1);
}

/* set address and netmask in network byte order */
static int
get_addr(char **cpp, struct in_addr *addr, struct in_addr *mask)
{
	char w[MAX_WORD], *ocp;
	struct in_addr tmp;

	addr->s_addr = 0;
	mask->s_addr = 0xffffffff;

	if (!next_word(cpp, w))
		return (0);

	if (inet_aton((char *)w, &tmp) != 1) {
		/* try gethostbyname */
		struct hostent *h;

		if ((h = gethostbyname(w)) == NULL ||
		    h->h_addrtype != AF_INET || h->h_length != 4)
			return (0);
		bcopy(h->h_addr, &tmp, (size_t)h->h_length);
	}
	addr->s_addr = tmp.s_addr;

	/* check if netmask option is present */
	ocp = *cpp;
	if (next_word(&ocp, w) && EQUAL(w, "netmask")) {
		if (!next_word(&ocp, w))
			return (0);
		if (inet_aton((char *)w, (struct in_addr *)&tmp) != 1)
			return (0);

		mask->s_addr = tmp.s_addr;
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
get_fltr_opts(char **cpp, char *fltr_name, size_t len, int *ruleno)
{
	char w[MAX_WORD], *ocp;

	ocp = *cpp;
	while (next_word(&ocp, w)) {
		if (EQUAL(w, "name")) {
			if (!next_word(&ocp, w))
				return (0);
			strlcpy(fltr_name, w, len);
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
	char	w[MAX_WORD], *ap, *cp = cmdbuf;
	char	*ifname, *argv[MAX_ARGS], qdisc_name[MAX_WORD];
	int     argc;
    
	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name");
		return (0);
	}

	/* create argment list & look for scheduling discipline options. */
	sprintf(qdisc_name, "null");
	argc = 0;
	ap = w;
	while (next_word(&cp, ap)) {
		if (is_qdisc_name(ap))
			strcpy(qdisc_name, ap);

		argv[argc] = ap;
		ap += strlen(ap) + 1;
		argc++;
		if (argc >= MAX_ARGS) {
			LOG(LOG_ERR, 0, "too many args");
			return (0);
		}
	}

	return qdisc_interface_parser(qdisc_name, ifname, argc, argv);
}


static int
class_parser(char *cmdbuf)
{
	char	w[MAX_WORD], *cp = cmdbuf;
	char 	*ifname, qdisc_name[MAX_WORD];
	char	class_name[MAX_WORD], parent_name[MAX_WORD];
	char	*clname = class_name;
	char	*parent = NULL;
	char	*argv[MAX_ARGS], *ap;
	int	argc, rval;

	/* get scheduling class */
	if (!next_word(&cp, qdisc_name)) {
		LOG(LOG_ERR, 0, "missing discipline");
		return (0);
	}
	if (!is_qdisc_name(qdisc_name)) {
		LOG(LOG_ERR, 0, "unknown discipline '%s'", qdisc_name);
		return (0);
	}

	/* get interface name */
	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name");
		return (0);
	}

	/* get class name */
	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0, "missing class name");
		return (0);
	}

	/* get parent name */
	if (!next_word(&cp, parent_name)) {
		LOG(LOG_ERR, 0, "missing parent class");
		return (0);
	}
	if (!EQUAL(parent_name, "null") && !EQUAL(parent_name, "NULL"))
		parent = parent_name;
	else
		parent = NULL;

	ap = w;
	argc = 0;
	while (next_word(&cp, ap)) {
		argv[argc] = ap;
		ap += strlen(ap) + 1;
		argc++;
		if (argc >= MAX_ARGS) {
			LOG(LOG_ERR, 0, "too many args");
			return (0);
		}
	}

	return qdisc_class_parser(qdisc_name, ifname, clname, parent,
				  argc, argv);
}

static int
filter_parser(char *cmdbuf)
{
	char 	w[MAX_WORD], *cp = cmdbuf;
	char 	*ifname, class_name[MAX_WORD], fltr_name[MAX_WORD];
	char	*flname = NULL;
	struct flow_filter	sfilt;
	int	protocol;
	u_char	tos, tosmask;
	int	ruleno;
	int	dontwarn = 0;
	int	error;

	memset(&sfilt, 0, sizeof(sfilt));
	sfilt.ff_flow.fi_family = AF_INET;

	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name in filter command");
		return (0);
	}

	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0, "missing class name in filter command");
		return (0);
	}

	fltr_name[0] = '\0';
	ruleno = 0;
	if (!get_fltr_opts(&cp, &fltr_name[0], sizeof(fltr_name), &ruleno)) {
		LOG(LOG_ERR, 0, "bad filter option");
		return (0);
	}
	if (fltr_name[0] != '\0')
		flname = fltr_name;
	sfilt.ff_ruleno = ruleno;

	/* get filter destination Address */
	if (!get_addr(&cp, &sfilt.ff_flow.fi_dst, &sfilt.ff_mask.mask_dst)) {
		LOG(LOG_ERR, 0, "bad filter destination address");
		return (0);
	}

	/* get filter destination port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter destination port");
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow.fi_dport)) {
		LOG(LOG_ERR, 0, "bad filter destination port");
		return (0);
	}

	/* get filter source address */
	if (!get_addr(&cp, &sfilt.ff_flow.fi_src, &sfilt.ff_mask.mask_src)) {
		LOG(LOG_ERR, 0, "bad filter source address");
		return (0);
	}

	/* get filter source port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter source port");
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow.fi_sport)) {
		LOG(LOG_ERR, 0, "bad filter source port");
		return (0);
	}

	/* get filter protocol id */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter protocol");
		return (0);
	}
	if (!get_proto(w, &protocol)) {
		LOG(LOG_ERR, 0, "bad protocol");
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
		    "can't add filter to class '%s' on interface '%s'",
		    class_name, ifname);
		return (0);
	}
	return (1);
}

#ifdef INET6
static int
filter6_parser(char *cmdbuf)
{
	char 	w[MAX_WORD], *cp = cmdbuf;
	char 	*ifname, class_name[MAX_WORD], fltr_name[MAX_WORD];
	char	*flname = NULL;
	struct flow_filter6	sfilt;
	int	protocol;
	u_char	tclass, tclassmask;
	int	ruleno;
	int	dontwarn = 0;
	int	ret;

	memset(&sfilt, 0, sizeof(sfilt));
	sfilt.ff_flow6.fi6_family = AF_INET6;

	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name");
		return (0);
	}

	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0, "missing class name");
		return (0);
	}

	fltr_name[0] = '\0';
	ruleno = 0;
	if (!get_fltr_opts(&cp, &fltr_name[0], sizeof(fltr_name), &ruleno)) {
		LOG(LOG_ERR, 0, "bad filter option");
		return (0);
	}
	if (fltr_name[0] != '\0')
		flname = fltr_name;
	sfilt.ff_ruleno = ruleno;

	/* get filter destination address */
	if (!get_ip6addr(&cp, &sfilt.ff_flow6.fi6_dst,
			 &sfilt.ff_mask6.mask6_dst)) {
		LOG(LOG_ERR, 0, "bad destination address");
		return (0);
	}

	/* get filter destination port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter destination port");
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow6.fi6_dport)) {
		LOG(LOG_ERR, 0, "bad filter destination port");
		return (0);
	}

	/* get filter source address */
	if (!get_ip6addr(&cp, &sfilt.ff_flow6.fi6_src,
			 &sfilt.ff_mask6.mask6_src)) {
		LOG(LOG_ERR, 0, "bad source address");
		return (0);
	}

	/* get filter source port */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter source port");
		return (0);
	}
	if (!get_port(w, &sfilt.ff_flow6.fi6_sport)) {
		LOG(LOG_ERR, 0, "bad filter source port");
		return (0);
	}

	/* get filter protocol id */
	if (!next_word(&cp, w)) {
		LOG(LOG_ERR, 0, "missing filter protocol");
		return (0);
	}
	if (!get_proto(w, &protocol)) {
		LOG(LOG_ERR, 0, "bad protocol");
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
		    "can't add filter to class '%s' on interface '%s'",
		    class_name, ifname);
		return (0);
	}

	return (1);
}

static int
get_ip6addr(char **cpp, struct in6_addr *addr, struct in6_addr *mask)
{
	char w[MAX_WORD], *prefix;
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

	if (inet_pton(AF_INET6, w, addr) != 1)
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
	char	w[MAX_WORD], *cp = cmdbuf;
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
	char	*ifname, class_name[MAX_WORD], filter_name[MAX_WORD];
	int	ret;

	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name");
		return (0);
	}

	if (!next_word(&cp, class_name)) {
		LOG(LOG_ERR, 0, "missing class name");
		return (0);
	}

	/* check if filter is specified */
	if (next_word(&cp, filter_name)) {
		ret = qcmd_delete_filter(ifname, class_name, filter_name);
		if (ret) {
			LOG(LOG_ERR, 0,
			    "can't delete filter '%s' on interface '%s'",
			    filter_name, ifname);
			return (0);
		}
		return (1);
	}

	ret = qcmd_delete_class(ifname, class_name);
	if (ret) {
		LOG(LOG_ERR, 0,
		    "can't delete class '%s' on interface '%s'",
		    class_name, ifname);
		return (0);
	}

	return (1);
}

static int
red_parser(char *cmdbuf)
{
	char	w[MAX_WORD], *cp = cmdbuf;
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
		LOG(LOG_ERR, 0, "can't set red default parameters");
		return (0);
	}

	return (1);

 bad:
	LOG(LOG_ERR, 0, "bad red parameter");
	return (0);
}

static int
rio_parser(char *cmdbuf)
{
	char	w[MAX_WORD], *cp = cmdbuf;
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
		LOG(LOG_ERR, 0, "can't set rio default parameters");
		return (0);
	}

	return (1);

 bad:
	LOG(LOG_ERR, 0, "bad rio parameter");
	return (0);
}

static int
conditioner_parser(char *cmdbuf)
{
	char	cdnr_name[MAX_WORD], *cp = cmdbuf;
	char	*ifname;
	struct tc_action action[MAX_ACTIONS];

	if (!get_ifname(&cp, &ifname)) {
		LOG(LOG_ERR, 0, "missing interface name");
		return (0);
	}

	/* get conditioner name */
	if (!next_word(&cp, cdnr_name)) {
		LOG(LOG_ERR, 0, "missing cdnr name");
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
	char	type[MAX_WORD], w[MAX_WORD];
	int	depth, i;
	struct tb_profile profile[2];

	/*
	 * find a possibly nested pair of '<' and '>',
	 * make them pointed by 'start' and 'end'.
	 */
	start = strchr(*cpp, '<');
	if (start == NULL) {
		LOG(LOG_ERR, 0, "conditioner action missing");
		return (0);
	}
	depth = 1;
	cp = start + 1;
	do {
		end = strpbrk(cp, "<>");
		if (end == NULL) {
			LOG(LOG_ERR, 0,
			    "conditioner action delimiter mismatch");
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
		LOG(LOG_ERR, 0, "missing conditioner action type");
		return (0);
	}

	/*
	 * action type specific process
	 */
	if (EQUAL(type, "conditioner")) {
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0,
			    "missing conditioner name");
			return (0);
		}
		action->tca_code = TCACODE_HANDLE;
		action->tca_handle = cdnr_name2handle(ifname, w);
		if (action->tca_handle == CDNR_NULL_HANDLE) {
			LOG(LOG_ERR, 0,
			    "wrong conditioner name %s", w);
			return (0);
		}
	} else if (EQUAL(type, "pass")) {
		action->tca_code = TCACODE_PASS;
	} else if (EQUAL(type, "drop")) {
		action->tca_code = TCACODE_DROP;
	} else if (EQUAL(type, "mark")) {
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing dscp");
			return (0);
		}
		action->tca_code = TCACODE_MARK;
		action->tca_dscp = (u_int8_t)strtol(w, NULL, 0);
	} else if (EQUAL(type, "tbmeter")) {
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing tb profile");
			return (0);
		}
		profile[0].rate = atobps(w);
		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing tb profile");
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
				LOG(LOG_ERR, 0, "missing tb profile");
				return (0);
			}
			profile[i].rate = atobps(w);
			if (!next_word(&cp, w)) {
				LOG(LOG_ERR, 0, "missing tb profile");
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
			LOG(LOG_ERR, 0, "missing cmtd rate");
			return (0);
		}
		cmtd_rate = atobps(w);

		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing peak rate");
			return (0);
		}
		peak_rate = atobps(w);

		if (!next_word(&cp, w)) {
			LOG(LOG_ERR, 0, "missing avg interval");
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
		LOG(LOG_ERR, 0, "unkown action type %s");
		return (0);
	}

	*end = '>';	/* restore the end delimiter */

	return (1);
}
