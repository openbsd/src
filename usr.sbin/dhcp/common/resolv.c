/* resolv.c

   Parser for /etc/resolv.conf file. */

/*
 * Copyright (c) 1995, 1996, 1997 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: resolv.c,v 1.1 1998/08/18 03:43:27 deraadt Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include "dhctoken.h"

struct name_server *name_servers;
struct domain_search_list *domains;
char path_resolv_conf [] = _PATH_RESOLV_CONF;

void read_resolv_conf (parse_time)
	TIME parse_time;
{
	FILE *cfile;
	char *val;
	int token;
	int declaration = 0;
	struct name_server *sp, *sl, *ns;
	struct domain_search_list *dp, *dl, *nd;
	struct iaddr *iaddr;

	new_parse (path_resolv_conf);

	eol_token = 1;
	if ((cfile = fopen (path_resolv_conf, "r")) == NULL) {
		warn ("Can't open %s: %m", path_resolv_conf);
		return;
	}

	do {
		token = next_token (&val, cfile);
		if (token == EOF)
			break;
		else if (token == EOL)
			continue;
		else if (token == DOMAIN || token == SEARCH) {
			do {
				struct domain_search_list *nd, **dp;
				char *dn;

				dn = parse_host_name (cfile);
				if (!dn)
					break;

				dp = &domains;
				for (nd = domains; nd; nd = nd -> next) {
					dp = &nd -> next;
					if (!strcmp (nd -> domain, dn))
						break;
				}
				if (!nd) {
					nd = new_domain_search_list
						("read_resolv_conf");
					if (!nd)
						error ("No memory for %s", dn);
					nd -> next =
						(struct domain_search_list *)0;
					*dp = nd;
					nd -> domain = dn;
					dn = (char *)0;
				}
				nd -> rcdate = parse_time;
				token = peek_token (&val, cfile);
			} while (token != EOL);
			if (token != EOL) {
				parse_warn ("junk after domain declaration");
				skip_to_semi (cfile);
			}
			token = next_token (&val, cfile);
		} else if (token == NAMESERVER) {
			struct name_server *ns, **sp;
			struct iaddr iaddr;

			parse_ip_addr (cfile, &iaddr);

			sp = &name_servers;
			for (ns = name_servers; ns; ns = ns -> next) {
				sp = &ns -> next;
				if (!memcmp (&ns -> addr.sin_addr,
					     iaddr.iabuf, iaddr.len))
					break;
			}
			if (!ns) {
				ns = new_name_server ("read_resolv_conf");
				if (!ns)
					error ("No memory for nameserver %s",
					       piaddr (iaddr));
				ns -> next = (struct name_server *)0;
				*sp = ns;
				memcpy (&ns -> addr.sin_addr,
					iaddr.iabuf, iaddr.len);
#ifdef HAVE_SA_LEN
				ns -> addr.sin_len = sizeof ns -> addr;
#endif
				ns -> addr.sin_family = AF_INET;
				ns -> addr.sin_port = htons (53);
				memset (ns -> addr.sin_zero, 0,
					sizeof ns -> addr.sin_zero);
			}
			ns -> rcdate = parse_time;
			skip_to_semi (cfile);
		}
	} while (1);
	token = next_token (&val, cfile); /* Clear the peek buffer */

	/* Lose servers that are no longer in /etc/resolv.conf. */
	sl = (struct name_server *)0;
	for (sp = name_servers; sp; sp = ns) {
		ns = sp -> next;
		if (sp -> rcdate != parse_time) {
			if (sl)
				sl -> next = sp -> next;
			else
				name_servers = sp -> next;
			free_name_server (sp, "pick_name_server");
		} else
			sl = sp;
	}

	/* Lose domains that are no longer in /etc/resolv.conf. */
	dl = (struct domain_search_list *)0;
	for (dp = domains; dp; dp = nd) {
		nd = dp -> next;
		if (dp -> rcdate != parse_time) {
			if (dl)
				dl -> next = dp -> next;
			else
				domains = dp -> next;
			free_domain_search_list (dp, "pick_name_server");
		} else
			dl = dp;
	}
	eol_token = 0;
}

/* Pick a name server from the /etc/resolv.conf file. */

struct sockaddr_in *pick_name_server ()
{
	FILE *rc;
	static TIME rcdate;
	struct stat st;

	/* Check /etc/resolv.conf and reload it if it's changed. */
	if (cur_time > rcdate) {
		if (stat (path_resolv_conf, &st) < 0) {
			warn ("Can't stat %s", path_resolv_conf);
			return (struct sockaddr_in *)0;
		}
		if (st.st_mtime > rcdate) {
			char rcbuf [512];
			char *s, *t, *u;
			rcdate = cur_time + 1;
			
			read_resolv_conf (rcdate);
		}
	}

	if (name_servers)
		return &name_servers -> addr;
	return (struct sockaddr_in *)0;
}
