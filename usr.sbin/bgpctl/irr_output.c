/*	$OpenBSD: irr_output.c,v 1.17 2015/04/25 15:28:18 phessler Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bgpd.h"
#include "irrfilter.h"

int	 process_policies(FILE *, struct policy_head *);
void	 policy_prettyprint(FILE *, struct policy_item *);
void	 policy_torule(FILE *, struct policy_item *);
char	*action_torule(char *);
void	 print_rule(FILE *, struct policy_item *, char *, struct irr_prefix *);

#define allowed_in_address(x) \
	(isalnum((unsigned char)x) || x == '.' || x == ':' || x == '-')

int
write_filters(char *outpath)
{
	struct router	*r;
	char		*fn;
	int		 fd, ret = 0;
	u_int		 i;
	FILE		*fh;

	while ((r = TAILQ_FIRST(&router_head)) != NULL) {
		TAILQ_REMOVE(&router_head, r, entry);

		if (r->address != NULL && r->address[0] != '\0') {
			for (i = 0; i < strlen(r->address); i++)
				if (!allowed_in_address(r->address[i]))
					errx(1, "router address \"%s\" contains"
					    " illegal character \"%c\"",
					    r->address, r->address[i]);
			if (asprintf(&fn, "%s/bgpd-%s.filter",
			    outpath, r->address) == -1)
				err(1, "write_filters asprintf");
		} else
			if (asprintf(&fn, "%s/bgpd.filter",
			    outpath) == -1)
				err(1, "write_filters asprintf");

		fd = open(fn, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
		if (fd == -1)
			err(1, "error opening %s", fn);
		if ((fh = fdopen(fd, "w")) == NULL)
			err(1, "fdopen %s", fn);

		if (process_policies(fh, &r->policy_h) == -1)
			ret = -1;

		fclose(fh);
		close(fd);
		free(fn);
		free(r->address);
		free(r);
	}

	return (ret);
}

int
process_policies(FILE *fh, struct policy_head *head)
{
	struct policy_item	*pi;

	while ((pi = TAILQ_FIRST(head)) != NULL) {
		TAILQ_REMOVE(head, pi, entry);

		policy_prettyprint(fh, pi);
		policy_torule(fh, pi);

		free(pi->peer_addr);
		free(pi->action);
		free(pi->filter);
		free(pi);
	}

	return (0);
}

void
policy_prettyprint(FILE *fh, struct policy_item *pi)
{
	if (pi->dir == IMPORT)
		fprintf(fh, "# import: from ");
	else
		fprintf(fh, "# export: to ");
	fprintf(fh, "AS%u ", pi->peer_as);
	if (pi->peer_addr)
		fprintf(fh, "%s ", pi->peer_addr);
	if (pi->action)
		fprintf(fh, "action %s ", pi->action);
	fprintf(fh, "%s %s\n", pi->dir == IMPORT ? "accept" : "announce",
	    pi->filter);
}

void
policy_torule(FILE *fh, struct policy_item *pi)
{
	struct as_set		*ass;
	struct prefix_set	*pfxs;
	char			*srcas;
	u_int			 i, j;

	if (pi->filter == NULL || !strcasecmp(pi->filter, "any"))
		print_rule(fh, pi, NULL, NULL);
	else {
		ass = asset_expand(pi->filter);

		for (i = 0; i < ass->n_as; i++) {
			pfxs = prefixset_get(ass->as[i]);

			/* ass->as[i] format and len have been checked before */
			if (strlen(ass->as[i]) < 3)
				errx(1, "%s not AS...", ass->as[i]);
			srcas = ass->as[i] + 2;
			for (j = 0; j < pfxs->prefixcnt; j++)
				print_rule(fh, pi, srcas, pfxs->prefix[j]);
		}
	}
}

/* XXX should really be parsed earlier! */
char *
action_torule(char *s)
{
	int		 cnt = 0;
	char		*key, *val, *pre, *tmp;
	static char	 abuf[8192];
	char		 ebuf[2048];

	if ((tmp = strdup(s)) == NULL)
		err(1, "foo");
	abuf[0] = '\0';
	while ((val = strsep(&tmp, ";")) != NULL && *val) {
		key = strsep(&val, "=");
		if (key == NULL || val == NULL)
			err(1, "format error in action spec\n");

		EATWS(key);
		EATWS(val);

		if (cnt++ == 0)
			pre = " set {";
		else
			pre = ",";

		if (!strcmp(key, "pref"))
			snprintf(ebuf, sizeof(ebuf),
			    "%s localpref %s", pre, val);
		else if (!strcmp(key, "med"))
			snprintf(ebuf, sizeof(ebuf),
			    "%s med %s", pre, val);
		else
			warnx("unknown action key \"%s\"", key);

		strlcat(abuf, ebuf, sizeof(abuf));
	}
	if (cnt > 0)
		strlcat(abuf, " }", sizeof(abuf));

	free(tmp);
	return (abuf);
}

void
print_rule(FILE *fh, struct policy_item *pi, char *sourceas,
    struct irr_prefix *prefix)
{
	char	 peer[PEER_DESCR_LEN];
	char	*action = "";
	char	*dir;
	char	*srcas[2] = { "", "" };
	char	 pbuf[8 + NI_MAXHOST + 4 + 14 + 3];
	size_t	 offset;

	if (pi->dir == IMPORT)
		dir = "from";
	else
		dir = "to";

	if (pi->peer_addr)
		snprintf(peer, PEER_DESCR_LEN, "%s", pi->peer_addr);
	else
		snprintf(peer, PEER_DESCR_LEN, "AS %s", log_as(pi->peer_as));

	if (pi->action)
		action = action_torule(pi->action);

	pbuf[0] = '\0';
	if (prefix != NULL) {
		strlcpy(pbuf, " prefix ", sizeof(pbuf));
		offset = strlen(pbuf);
		if (inet_ntop(prefix->af, &prefix->addr, pbuf + offset,
		    sizeof(pbuf) - offset) == NULL)
			err(1, "print_rule inet_ntop");
		offset = strlen(pbuf);
		if (snprintf(pbuf + offset, sizeof(pbuf) - offset,
		    "/%u", prefix->len) == -1)
			err(1, "print_rule snprintf");

		if (prefix->maxlen > prefix->len) {
			offset = strlen(pbuf);
			if (snprintf(pbuf + offset, sizeof(pbuf) - offset,
			    " prefixlen <= %u", prefix->maxlen) == -1)
				err(1, "print_rule snprintf");
		}

		if (pi->dir == IMPORT) {
			srcas[0] = " source-as ";
			srcas[1] = sourceas;
		}
	}

	fprintf(fh, "allow quick %s %s%s%s%s%s\n", dir, peer,
	    srcas[0], srcas[1], pbuf, action);
}
