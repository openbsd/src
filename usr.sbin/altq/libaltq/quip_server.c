/*	$OpenBSD: quip_server.c,v 1.1.1.1 2001/06/27 18:23:31 kjc Exp $	*/
/*	$KAME: quip_server.c,v 1.2 2000/10/18 09:15:21 kjc Exp $	*/
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <altq/altq.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>

#include "altq_qop.h"
#include "quip_server.h"

extern LIST_HEAD(qop_iflist, ifinfo)	qop_iflist;

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

static int next_word(char **cpp, char *b);

static int query_list(const char *cmd, const char *arg, char *msg);
static int query_handle2name(const char *cmd, const char *arg, char *msg);
static int query_qdisc(const char *cmd, const char *arg, char *msg);
static int query_filterspec(const char *cmd, const char *arg, char *msg);

int
quip_input(FILE *fp)
{
	char request[256], result[256], body[8192], w[256], *cp, *query;
	int n = 0;

	while (1) {
		if (fgets(request, 128, fp) == NULL)  /* EOF */
			return (-1);
		/* skip preceding blank lines */
		if (request[0] == '\n')
			continue;
		break;
	}

	/* remove trailing newline and white space */
	if ((cp = strrchr(request, '\n')) != NULL) {
		*cp-- = '\0';
		while (*cp == ' ' || *cp == '\t')
			*cp-- = '\0';
	}

	body[0] = '\0';
	cp = request;
	if (!next_word(&cp, w)) {
		sprintf(result, "400 Bad request\n");
		goto done;
	}
	if (EQUAL(w, "GET")) {
		if (!next_word(&cp, w)) {
			sprintf(result, "400 Bad request\n");
			goto done;
		}
		if ((query = strchr(w, '?')) != NULL) {
			/* request has a query string */
			*query = '\0';
			query++;
		}

		if (EQUAL(w, "list")) {
			n = query_list(w, query, body);
		} else if (EQUAL(w, "handle-to-name")) {
			n = query_handle2name(w, query, body);
		} else if (EQUAL(w, "qdisc")) {
			n = query_qdisc(w, query, body);
		} else if (EQUAL(w, "filter")) {
			n = query_filterspec(w, query, body);
		} else {
			sprintf(result, "400 Bad request\n");
			goto done;
		}
	} else {
		sprintf(result, "400 Bad request\n");
		goto done;
	}

	if (n == 0) {
		sprintf(result, "204 No content\n");
	} else if (n < 0) {
		sprintf(result, "400 Bad request\n");
	} else {
		sprintf(result, "200 OK\nContent-Length:%d\n", n);
	}
	
  done:
	/* send a result line and a blank line */
	if (fputs ("QUIP/1.0 ", fp) != 0 ||
	    fputs(result, fp) != 0 || fputs("\n", fp) != 0)
		return (-1);

	/* send message body */
	if (fputs(body, fp) != 0)
		return (-1);
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
	int		L;

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
 * expand_classname creates a long class name.
 *   <ifname>:/<root_name>/../<parent_name>/<class_name>
 */
static int
expand_classname(struct classinfo *clinfo, char *name)
{
	struct classinfo *ci = clinfo;
	char buf[2][256], *b0, *b1, *tmp;

	b0 = buf[0]; b1 = buf[1];
	b1[0] = '\0';
	while (ci != NULL) {
		strcpy(b0, "/");
		strcat(b0, ci->clname);
		strcat(b0, b1);

		ci = ci->parent;
		tmp = b0; b0 = b1; b1 = tmp;
	}
	sprintf(b0, "%s:", clinfo->ifinfo->ifname);
	strcat(b0, b1);
	strcpy(name, b0);
	return (strlen(name));
}

/*
 * expand_filtername creates a long filter name.
 *   <ifname>:/<root_name>/../<parent_name>/<class_name>:<fltr_name>
 */
static int
expand_filtername(struct fltrinfo *fltrinfo, char *name)
{
	int len;

	len = expand_classname(fltrinfo->clinfo, name);
	sprintf(name + len, ":%s", fltrinfo->flname);
	return (len + strlen(name + len));
}

static int
query_handle2name(const char *cmd, const char *arg, char *msg)
{
	struct ifinfo *ifinfo;
	struct classinfo *clinfo;
	struct fltrinfo *fltrinfo;
	char *ifname, *class_field, *fltr_field, buf[256], *cp;
	u_long handle;
	int len, size;

	strcpy(buf, arg);
	cp = buf;
	ifname = strsep(&cp, ":");
	class_field = strsep(&cp, ":");
	fltr_field = cp;

	if (fltr_field != NULL) {
		if (sscanf(fltr_field, "%lx", &handle) != 1)
			return (-1);
		if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
			return (-1);
		if ((fltrinfo = flhandle2fltrinfo(ifinfo, handle)) == NULL)
			return (-1);

		len = expand_filtername(fltrinfo, msg);
	} else {
		if (sscanf(class_field, "%lx", &handle) != 1)
			return (-1);
		if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
			return (-1);
		if ((clinfo = clhandle2clinfo(ifinfo, handle)) == NULL)
				return (-1);

		len = expand_classname(clinfo, msg);
	}
	size = len + sprintf(msg + len, "\n");
	return (size);
}

static int
query_qdisc(const char *cmd, const char *arg, char *msg)
{
	struct ifinfo *ifinfo;
	int size;

	if ((ifinfo = ifname2ifinfo(arg)) == NULL)
		return (-1);

	size = sprintf(msg, "%s\nbandwidth:%.2fMbps\nstatus:%s\n",
		ifinfo->qdisc->qname, (double)ifinfo->bandwidth/1000000,
		(ifinfo->enabled ? "enabled" : "disabled"));
	return (size);
}

static int
query_filterspec(const char *cmd, const char *arg, char *msg)
{
	struct ifinfo *ifinfo;
	struct fltrinfo *fltrinfo;
	struct flow_filter *filt;
	char *ifname, *class_field, *fltr_field, buf[256], *cp;
	u_long handle;
	int size;

	strcpy(buf, arg);
	cp = buf;
	ifname = strsep(&cp, ":");
	class_field = strsep(&cp, ":");
	fltr_field = cp;

	if (fltr_field == NULL)
		return (-1);
	if (sscanf(fltr_field, "%lx", &handle) != 1)
		return (-1);

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		return (-1);
	if ((fltrinfo = flhandle2fltrinfo(ifinfo, handle)) == NULL)
		return (-1);

	filt = &fltrinfo->fltr;
	
	if (filt->ff_flow.fi_family == AF_INET) {
		char src[128], dst[128], smask[128], dmask[128], tos[128];

		if (filt->ff_flow.fi_dst.s_addr == 0) {
			sprintf(dst, "0");
			dmask[0] = '\0';
		} else {
			sprintf(dst, "%s", inet_ntoa(filt->ff_flow.fi_dst));
			
			if (filt->ff_mask.mask_dst.s_addr == 0xffffffff)
				dmask[0] = '\0';
			else
				sprintf(dmask, " mask %#x",
					ntoh32(filt->ff_mask.mask_dst.s_addr));
		}
		if (filt->ff_flow.fi_src.s_addr == 0) {
			sprintf(src, "0");
			smask[0] = '\0';
		} else {
			sprintf(src, "%s", inet_ntoa(filt->ff_flow.fi_src));
			
			if (filt->ff_mask.mask_src.s_addr == 0xffffffff)
				smask[0] = '\0';
			else
				sprintf(smask, " mask %#x",
					ntoh32(filt->ff_mask.mask_src.s_addr));
		}
		if (filt->ff_flow.fi_tos == 0)
			tos[0] = '\0';
		else
			sprintf(tos, " tos %#x tosmask %#x",
				filt->ff_flow.fi_tos,
				filt->ff_mask.mask_tos);

		size = sprintf(msg, "inet %s%s %d %s%s %d %d%s\n", 
			       dst, dmask,
			       ntoh16(filt->ff_flow.fi_dport),
			       src, smask,
			       ntoh16(filt->ff_flow.fi_sport),
			       filt->ff_flow.fi_proto, tos);
	}
#ifdef INET6
	else if (filt->ff_flow.fi_family == AF_INET6) {
		struct flow_filter6 *filt6;
		char dst6[INET6_ADDRSTRLEN], dmask6[INET6_ADDRSTRLEN];
		char src6[INET6_ADDRSTRLEN], smask6[INET6_ADDRSTRLEN];
		char tclass6[128];
		const struct in6_addr mask128 = 
		{{{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }}};

		filt6 = (struct flow_filter6 *)&fltrinfo->fltr;
		if (IN6_IS_ADDR_UNSPECIFIED(&filt6->ff_flow6.fi6_dst)) {
			sprintf(dst6, "0");
			dmask6[0] = '\0';
		} else {
			inet_ntop(AF_INET6, &filt6->ff_flow6.fi6_dst,
				  dst6, sizeof(dst6));
			if (IN6_ARE_ADDR_EQUAL(&mask128,
					       &filt6->ff_mask6.mask6_dst))
				dmask6[0] = '\0';
			else {
				sprintf(dmask6, " mask ");
				inet_ntop(AF_INET6, &filt6->ff_mask6.mask6_dst,
					  dmask6 + 6, sizeof(dmask6) -6);
			}
		}

		if (IN6_IS_ADDR_UNSPECIFIED(&filt6->ff_flow6.fi6_src)) {
			sprintf(src6, "0");
			smask6[0] = '\0';
		} else {
			inet_ntop(AF_INET6, &filt6->ff_flow6.fi6_src,
				  src6, sizeof(src6));
			if (IN6_ARE_ADDR_EQUAL(&mask128,
					       &filt6->ff_mask6.mask6_src))
				smask6[0] = '\0';
			else {
				sprintf(smask6, " mask ");
				inet_ntop(AF_INET6, &filt6->ff_mask6.mask6_src,
					  smask6 + 6, sizeof(smask6) -6);
			}
		}
		if (filt6->ff_flow6.fi6_tclass == 0)
			tclass6[0] = '\0';
		else
			sprintf(tclass6, " tclass %#x tclassmask %#x",
				filt6->ff_flow6.fi6_tclass,
				filt6->ff_mask6.mask6_tclass);

		size = sprintf(msg, "inet6 %s%s %d %s%s %d %d%s\n", 
			       dst6, dmask6,
			       ntoh16(filt6->ff_flow6.fi6_dport),
			       src6, smask6,
			       ntoh16(filt6->ff_flow6.fi6_sport),
			       filt6->ff_flow6.fi6_proto, tclass6);
	}
#endif /* INET6 */

	return (size);
}


/*
 * string_match compares 2 strings and returns 1 when s1 matches s2.
 *	s1: possibly includes wildcards, "*".
 *	s2: must be a full string (should not include "*").
 */
static int
string_match(const char *s1, const char *s2)
{
	char *ap, *next, sub[256];
	int prefixlen, sublen;

	/* if there's no wild card, compare full string */
	if ((ap = strchr(s1, '*')) == NULL)
		return (strcmp(s1, s2) == 0);

	/* compare string prefix */
	prefixlen = ap - s1;
	if (strncmp(s1, s2, prefixlen) != 0)
		return (0);
	s2 += prefixlen;

	/*
	 * if there is another wildcard in the rest of the string,
	 * compare the substring between the 2 wildcards.
	 */
	while ((next = strchr(ap + 1, '*')) != NULL) {
		sublen = next - ap - 1;
		strncpy(sub, ap+1, sublen);
		sub[sublen] = '\0';
		if ((s2 = strstr(s2, sub)) == NULL)
			return (0);

		s2 += sublen;
		ap = next;
	}

	/* no more wildcard, compare the rest of the string */
	return (strcmp(ap+1, s2+strlen(s2)-strlen(ap+1)) == 0);
}

static int
query_list(const char *cmd, const char *arg, char *msg)
{
	char tmp[256], *cp;
	struct ifinfo *ifinfo;
	struct classinfo *clinfo;
	struct fltrinfo *fltrinfo;
	int print_if, print_class, print_fltr, size = 0;

	if (arg == NULL) {
		/* no arg, print all */
		print_if = print_class = print_fltr = 1;
	} else {
		print_if = print_class = print_fltr = 0;
		if ((cp = strchr(arg, ':')) == NULL)
			print_if = 1;
		else if (strchr(cp+1, ':') == NULL)
			print_class = 1;
		else
			print_fltr = 1;
	}

	cp = msg;
	LIST_FOREACH(ifinfo, &qop_iflist, next) {
		if (print_if) {
			strcpy(tmp, ifinfo->ifname);
			if (arg == NULL || string_match(arg, tmp))
				cp += sprintf(cp, "%#010x\t%s\n",
					      ifinfo->ifindex, tmp);
		}
		if (!print_class && !print_fltr)
			continue;
		for (clinfo = get_rootclass(ifinfo);
			  clinfo != NULL; clinfo = get_nextclass(clinfo)) {
			if (print_class) {
				expand_classname(clinfo, tmp);
				if (arg == NULL || string_match(arg, tmp))
					cp += sprintf(cp, "%#010lx\t%s\n",
						      clinfo->handle, tmp);
			}
			if (!print_fltr)
				continue;
			LIST_FOREACH(fltrinfo, &clinfo->fltrlist, next) {
				expand_filtername(fltrinfo, tmp);
				if (arg == NULL || string_match(arg, tmp))
					cp += sprintf(cp, "%#010lx\t%s\n",
						      fltrinfo->handle, tmp);
			}
		}
	}
	size = cp - msg;
	return (size);
}

