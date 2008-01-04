/*	$OpenBSD: ike.c,v 1.62 2008/01/04 11:06:02 hshoexer Exp $	*/
/*
 * Copyright (c) 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipsecctl.h"

static void	ike_section_general(struct ipsec_rule *, FILE *);
static void	ike_section_peer(struct ipsec_rule *, FILE *);
static void	ike_section_ids(struct ipsec_rule *, FILE *);
static void	ike_section_ipsec(struct ipsec_rule *, FILE *);
static int	ike_section_p1(struct ipsec_rule *, FILE *);
static int	ike_section_p2(struct ipsec_rule *, FILE *);
static void	ike_section_p2ids(struct ipsec_rule *, FILE *);
static int	ike_connect(struct ipsec_rule *, FILE *);
static int	ike_gen_config(struct ipsec_rule *, FILE *);
static int	ike_delete_config(struct ipsec_rule *, FILE *);
static void	ike_setup_ids(struct ipsec_rule *);

int		ike_print_config(struct ipsec_rule *, int);
int		ike_ipsec_establish(int, struct ipsec_rule *);

#define	SET	"C set "
#define	ADD	"C add "
#define	DELETE	"C rms "
#define	RMV	"C rmv "

#define ISAKMPD_FIFO	"/var/run/isakmpd.fifo"

#define CONF_DFLT_DYNAMIC_DPD_CHECK_INTERVAL	5
#define CONF_DFLT_DYNAMIC_CHECK_INTERVAL	30

char *ike_id_types[] = { "", "", "FQDN", "USER_FQDN" };

static void
ike_section_general(struct ipsec_rule *r, FILE *fd)
{
	if (r->ikemode == IKE_DYNAMIC) {
		fprintf(fd, SET "[General]:Check-interval=%d force\n",
		    CONF_DFLT_DYNAMIC_CHECK_INTERVAL);
		fprintf(fd, SET "[General]:DPD-check-interval=%d force\n",
		    CONF_DFLT_DYNAMIC_DPD_CHECK_INTERVAL);
	}
	if (r->p1life && r->p1life->lifetime != -1)
		fprintf(fd, SET "[General]:Default-phase-1-lifetime=%d force\n",
		    r->p1life->lifetime);
	if (r->p2life && r->p2life->lifetime != -1)
		fprintf(fd, SET "[General]:Default-phase-2-lifetime=%d force\n",
		    r->p2life->lifetime);
}

static void
ike_section_peer(struct ipsec_rule *r, FILE *fd)
{
	if (r->peer) {
		fprintf(fd, SET "[Phase 1]:%s=peer-%s force\n", r->peer->name,
		    r->peer->name);
		fprintf(fd, SET "[peer-%s]:Phase=1 force\n", r->peer->name);
		fprintf(fd, SET "[peer-%s]:Address=%s force\n", r->peer->name,
		    r->peer->name);
		if (r->local)
			fprintf(fd, SET "[peer-%s]:Local-address=%s force\n",
			    r->peer->name, r->local->name);
		if (r->ikeauth->type == IKE_AUTH_PSK)
			fprintf(fd, SET "[peer-%s]:Authentication=%s force\n",
			    r->peer->name, r->ikeauth->string);
	} else {
		fprintf(fd, SET "[Phase 1]:Default=peer-default force\n");
		fprintf(fd, SET "[peer-default]:Phase=1 force\n");
		if (r->local)
			fprintf(fd, SET
			    "[peer-default]:Local-address=%s force\n",
			    r->local->name);
		if (r->ikeauth->type == IKE_AUTH_PSK)
			fprintf(fd, SET
			    "[peer-default]:Authentication=%s force\n",
			    r->ikeauth->string);
	}
}

static void
ike_section_ids(struct ipsec_rule *r, FILE *fd)
{
	char myname[MAXHOSTNAMELEN];

	if (r->auth == NULL)
		return;

	if (r->ikemode == IKE_DYNAMIC && r->auth->srcid == NULL) {
		if (gethostname(myname, sizeof(myname)) == -1)
			err(1, "ike_section_ids: gethostname");
		if ((r->auth->srcid = strdup(myname)) == NULL)
			err(1, "ike_section_ids: strdup");
	}
	if (r->auth->srcid) {
		if (r->peer)
			fprintf(fd, SET "[peer-%s]:ID=%s-ID force\n",
			    r->peer->name, r->auth->srcid);
		else
			fprintf(fd, SET "[peer-default]:ID=%s-ID force\n",
			    r->auth->srcid);

		fprintf(fd, SET "[%s-ID]:ID-type=%s force\n", r->auth->srcid,
		    ike_id_types[r->auth->srcid_type]);
		fprintf(fd, SET "[%s-ID]:Name=%s force\n", r->auth->srcid,
		    r->auth->srcid);
	}
	if (r->auth->dstid) {
		if (r->peer) {
			fprintf(fd, SET "[peer-%s]:Remote-ID=%s-ID force\n",
			    r->peer->name, r->peer->name);
			fprintf(fd, SET "[%s-ID]:ID-type=%s force\n",
			    r->peer->name, ike_id_types[r->auth->dstid_type]);
			fprintf(fd, SET "[%s-ID]:Name=%s force\n", r->peer->name,
			    r->auth->dstid);
		} else {
			fprintf(fd, SET
			    "[peer-default]:Remote-ID=default-ID force\n");
			fprintf(fd, SET "[default-ID]:ID-type=%s force\n",
			    ike_id_types[r->auth->dstid_type]);
			fprintf(fd, SET "[default-ID]:Name=%s force\n",
			    r->auth->dstid);
		}
	}
}

static void
ike_section_ipsec(struct ipsec_rule *r, FILE *fd)
{
	fprintf(fd, SET "[IPsec-%s]:Phase=2 force\n", r->p2name);

	if (r->peer)
		fprintf(fd, SET "[IPsec-%s]:ISAKMP-peer=peer-%s force\n",
		    r->p2name, r->peer->name);
	else
		fprintf(fd, SET
		    "[IPsec-%s]:ISAKMP-peer=peer-default force\n", r->p2name);

	fprintf(fd, SET "[IPsec-%s]:Configuration=qm-%s force\n", r->p2name,
	    r->p2name);
	fprintf(fd, SET "[IPsec-%s]:Local-ID=lid-%s force\n", r->p2name,
	    r->p2lid);
	fprintf(fd, SET "[IPsec-%s]:Remote-ID=rid-%s force\n", r->p2name,
	    r->p2rid);

	if (r->tag)
		fprintf(fd, SET "[IPsec-%s]:PF-Tag=%s force\n", r->p2name,
		    r->tag);
}

static int
ike_section_p2(struct ipsec_rule *r, FILE *fd)
{
	char	*tag, *exchange_type, *sprefix;

	switch (r->p2ie) {
	case IKE_QM:
		tag = "qm";
		exchange_type = "QUICK_MODE";
		sprefix = "QM";
		break;
	default:
		warnx("illegal phase 2 ike mode %d", r->p2ie);
		return (-1);
	}

	fprintf(fd, SET "[%s-%s]:EXCHANGE_TYPE=%s force\n", tag, r->p2name,
	    exchange_type);
	fprintf(fd, SET "[%s-%s]:Suites=%s-", tag, r->p2name, sprefix);

	switch (r->satype) {
	case IPSEC_ESP:
		fprintf(fd, "ESP");
		break;
	case IPSEC_AH:
		fprintf(fd, "AH");
		break;
	default:
		warnx("illegal satype %d", r->satype);
		return (-1);
	}
	fprintf(fd, "-");

	switch (r->tmode) {
	case IPSEC_TUNNEL:
		break;
	case IPSEC_TRANSPORT:
		fprintf(fd, "TRP-");
		break;
	default:
		warnx("illegal encapsulation mode %d", r->tmode);
		return (-1);
	}

	if (r->p2xfs && r->p2xfs->encxf) {
		if (r->satype == IPSEC_ESP) {
			switch (r->p2xfs->encxf->id) {
			case ENCXF_3DES_CBC:
				fprintf(fd, "3DES");
				break;
			case ENCXF_DES_CBC:
				fprintf(fd, "DES");
				break;
			case ENCXF_AES:
				fprintf(fd, "AES");
				break;
			case ENCXF_AESCTR:
				fprintf(fd, "AESCTR");
				break;
			case ENCXF_BLOWFISH:
				fprintf(fd, "BLF");
				break;
			case ENCXF_CAST128:
				fprintf(fd, "CAST");
				break;
			case ENCXF_NULL:
				fprintf(fd, "NULL");
				break;
			default:
				warnx("illegal transform %s",
				    r->p2xfs->encxf->name);
				return (-1);
			}
			fprintf(fd, "-");
		} else {
			warnx("illegal transform %s", r->p2xfs->encxf->name);
			return (-1);
		}
	} else if (r->satype == IPSEC_ESP)
		fprintf(fd, "AES-");

	if (r->p2xfs && r->p2xfs->authxf) {
		switch (r->p2xfs->authxf->id) {
		case AUTHXF_HMAC_MD5:
			fprintf(fd, "MD5");
			break;
		case AUTHXF_HMAC_SHA1:
			fprintf(fd, "SHA");
			break;
		case AUTHXF_HMAC_RIPEMD160:
			fprintf(fd, "RIPEMD");
			break;
		case AUTHXF_HMAC_SHA2_256:
			fprintf(fd, "SHA2-256");
			break;
		case AUTHXF_HMAC_SHA2_384:
			fprintf(fd, "SHA2-384");
			break;
		case AUTHXF_HMAC_SHA2_512:
			fprintf(fd, "SHA2-512");
			break;
		default:
			warnx("illegal transform %s", r->p2xfs->authxf->name);
			return (-1);
		}
	} else
		fprintf(fd, "SHA2-256");

	if (r->p2xfs && r->p2xfs->groupxf) {
		switch (r->p2xfs->groupxf->id) {
		case GROUPXF_NONE:
			break;
		case GROUPXF_768:
			fprintf(fd, "-PFS-GRP1");
			break;
		case GROUPXF_1024:
			fprintf(fd, "-PFS-GRP2");
			break;
		case GROUPXF_1536:
			fprintf(fd, "-PFS-GRP5");
			break;
		case GROUPXF_2048:
			fprintf(fd, "-PFS-GRP14");
			break;
		case GROUPXF_3072:
			fprintf(fd, "-PFS-GRP15");
			break;
		case GROUPXF_4096:
			fprintf(fd, "-PFS-GRP16");
			break;
		case GROUPXF_6144:
			fprintf(fd, "-PFS-GRP17");
			break;
		case GROUPXF_8192:
			fprintf(fd, "-PFS-GRP18");
			break;
		default:
			warnx("illegal group %s", r->p2xfs->groupxf->name);
			return (-1);
		};
	} else
		fprintf(fd, "-PFS");
	fprintf(fd, "-SUITE force\n");

	return (0);
}

static int
ike_section_p1(struct ipsec_rule *r, FILE *fd)
{
	char *tag, *exchange_type;

	switch (r->p1ie) {
	case IKE_MM:
		tag = "mm";
		exchange_type = "ID_PROT";
		break;
	case IKE_AM:
		tag = "am";
		exchange_type = "AGGRESSIVE";
		break;
	default:
		warnx("illegal phase 2 ike mode %d", r->p1ie);
		return (-1);
	}

	if (r->peer) {
		fprintf(fd, SET "[peer-%s]:Configuration=%s-%s force\n",
		    r->peer->name, tag, r->peer->name);
		fprintf(fd, SET "[%s-%s]:EXCHANGE_TYPE=%s force\n",
		    tag, r->peer->name, exchange_type);
		fprintf(fd, ADD "[%s-%s]:Transforms=", tag, r->peer->name);
	} else {
		fprintf(fd, SET
		    "[peer-default]:Configuration=%s-default force\n", tag);
		fprintf(fd, SET "[%s-default]:EXCHANGE_TYPE=%s force\n",
		    tag, exchange_type);
		fprintf(fd, ADD "[%s-default]:Transforms=", tag);
	}

	if (r->p1xfs && r->p1xfs->encxf) {
		switch (r->p1xfs->encxf->id) {
		case ENCXF_3DES_CBC:
			fprintf(fd, "3DES");
			break;
		case ENCXF_DES_CBC:
			fprintf(fd, "DES");
			break;
		case ENCXF_AES:
			fprintf(fd, "AES");
			break;
		case ENCXF_BLOWFISH:
			fprintf(fd, "BLF");
			break;
		case ENCXF_CAST128:
			fprintf(fd, "CAST");
			break;
		default:
			warnx("illegal transform %s", r->p1xfs->encxf->name);
			return (-1);
		}
	} else
		fprintf(fd, "AES");
	fprintf(fd, "-");

	if (r->p1xfs && r->p1xfs->authxf) {
		switch (r->p1xfs->authxf->id) {
		case AUTHXF_HMAC_MD5:
			fprintf(fd, "MD5");
			break;
		case AUTHXF_HMAC_SHA1:
			fprintf(fd, "SHA");
			break;
		case AUTHXF_HMAC_SHA2_256:
			fprintf(fd, "SHA2-256");
			break;
		case AUTHXF_HMAC_SHA2_384:
			fprintf(fd, "SHA2-384");
			break;
		case AUTHXF_HMAC_SHA2_512:
			fprintf(fd, "SHA2-512");
			break;
		default:
			warnx("illegal transform %s", r->p1xfs->authxf->name);
			return (-1);
		}
	} else
		fprintf(fd, "SHA");

	if (r->p1xfs && r->p1xfs->groupxf) {
		switch (r->p1xfs->groupxf->id) {
		case GROUPXF_768:
			fprintf(fd, "-GRP1");
			break;
		case GROUPXF_1024:
			fprintf(fd, "-GRP2");
			break;
		case GROUPXF_1536:
			fprintf(fd, "-GRP5");
			break;
		case GROUPXF_2048:
			fprintf(fd, "-GRP14");
			break;
		case GROUPXF_3072:
			fprintf(fd, "-GRP15");
			break;
		case GROUPXF_4096:
			fprintf(fd, "-GRP16");
			break;
		case GROUPXF_6144:
			fprintf(fd, "-GRP17");
			break;
		case GROUPXF_8192:
			fprintf(fd, "-GRP18");
			break;
		default:
			warnx("illegal group %s", r->p1xfs->groupxf->name);
			return (-1);
		};
	}

	if (r->ikeauth->type == IKE_AUTH_RSA)
		fprintf(fd, "-RSA_SIG");
	fprintf(fd, " force\n");

	return (0);
}

static void
ike_section_p2ids(struct ipsec_rule *r, FILE *fd)
{
	char mask[NI_MAXHOST], *network, *p;
	struct sockaddr_storage sas;
	struct sockaddr *sa = (struct sockaddr *)&sas;
	struct ipsec_addr_wrap *src = r->src;
	struct ipsec_addr_wrap *dst = r->dst;

	if (src->netaddress) {
		bzero(&sas, sizeof(struct sockaddr_storage));
		bzero(mask, sizeof(mask));
		sa->sa_family = src->af;
		switch (src->af) {
		case AF_INET:
			sa->sa_len = sizeof(struct sockaddr_in);
			bcopy(&src->mask.ipa,
			    &((struct sockaddr_in *)(sa))->sin_addr,
			    sizeof(struct in6_addr));
			break;
		case AF_INET6:
			sa->sa_len = sizeof(struct sockaddr_in6);
			bcopy(&src->mask.ipa,
			    &((struct sockaddr_in6 *)sa)->sin6_addr,
			    sizeof(struct in6_addr));
			break;
		}
		if (getnameinfo(sa, sa->sa_len, mask, sizeof(mask), NULL, 0,
		    NI_NUMERICHOST))
			errx(1, "could not get a numeric mask");

		if ((network = strdup(src->name)) == NULL)
			err(1, "ike_section_p2ids: strdup");
		if ((p = strrchr(network, '/')) != NULL)
			*p = '\0';

		fprintf(fd, SET "[lid-%s]:ID-type=IPV%d_ADDR_SUBNET force\n",
		    r->p2lid, ((src->af == AF_INET) ? 4 : 6));
		fprintf(fd, SET "[lid-%s]:Network=%s force\n", r->p2lid,
		    network);
		fprintf(fd, SET "[lid-%s]:Netmask=%s force\n", r->p2lid, mask);

		free(network);
	} else {
		fprintf(fd, SET "[lid-%s]:ID-type=IPV%d_ADDR force\n",
		    r->p2lid, ((src->af == AF_INET) ? 4 : 6));
		if ((p = strrchr(src->name, '/')) != NULL)
			*p = '\0';
		fprintf(fd, SET "[lid-%s]:Address=%s force\n", r->p2lid,
		    src->name);
	}
	if (dst->netaddress) {
		bzero(&sas, sizeof(struct sockaddr_storage));
		bzero(mask, sizeof(mask));
		sa->sa_family = dst->af;
		switch (dst->af) {
		case AF_INET:
			sa->sa_len = sizeof(struct sockaddr_in);
			bcopy(&dst->mask.ipa,
			    &((struct sockaddr_in *)(sa))->sin_addr,
			    sizeof(struct in6_addr));
			break;
		case AF_INET6:
			sa->sa_len = sizeof(struct sockaddr_in6);
			bcopy(&dst->mask.ipa,
			    &((struct sockaddr_in6 *)(sa))->sin6_addr,
			    sizeof(struct in6_addr));
			break;
		}
		if (getnameinfo(sa, sa->sa_len, mask, sizeof(mask), NULL, 0,
		    NI_NUMERICHOST))
			errx(1, "could not get a numeric mask");

		if ((network = strdup(dst->name)) == NULL)
			err(1, "ike_section_p2ids: strdup");
		if ((p = strrchr(network, '/')) != NULL)
			*p = '\0';

		fprintf(fd, SET "[rid-%s]:ID-type=IPV%d_ADDR_SUBNET force\n",
		    r->p2rid, ((dst->af == AF_INET) ? 4 : 6));
		fprintf(fd, SET "[rid-%s]:Network=%s force\n", r->p2rid,
		    network);
		fprintf(fd, SET "[rid-%s]:Netmask=%s force\n", r->p2rid, mask);

		free(network);
	} else {
		fprintf(fd, SET "[rid-%s]:ID-type=IPV%d_ADDR force\n",
		    r->p2rid, ((dst->af == AF_INET) ? 4 : 6));
		if ((p = strrchr(dst->name, '/')) != NULL)
			*p = '\0';
		fprintf(fd, SET "[rid-%s]:Address=%s force\n", r->p2rid,
		    dst->name);
	}
	if (r->proto) {
		fprintf(fd, SET "[lid-%s]:Protocol=%d force\n",
		    r->p2lid, r->proto);
		fprintf(fd, SET "[rid-%s]:Protocol=%d force\n",
		    r->p2rid, r->proto);
	}
	if (r->sport)
		fprintf(fd, SET "[lid-%s]:Port=%d force\n", r->p2lid,
		    ntohs(r->sport));
	if (r->dport)
		fprintf(fd, SET "[rid-%s]:Port=%d force\n", r->p2rid,
		    ntohs(r->dport));
}

static int
ike_connect(struct ipsec_rule *r, FILE *fd)
{
	switch (r->ikemode) {
	case IKE_ACTIVE:
	case IKE_DYNAMIC:
		fprintf(fd, ADD "[Phase 2]:Connections=IPsec-%s\n", r->p2name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, ADD "[Phase 2]:Passive-Connections=IPsec-%s\n",
		    r->p2name);
		break;
	default:
		return (-1);
	}
	return (0);
}

static int
ike_gen_config(struct ipsec_rule *r, FILE *fd)
{
	ike_setup_ids(r);
	ike_section_general(r, fd);
	ike_section_peer(r, fd);
	if (ike_section_p1(r, fd) == -1) {
		return (-1);
	}
	ike_section_ids(r, fd);
	ike_section_ipsec(r, fd);
	if (ike_section_p2(r, fd) == -1) {
		return (-1);
	}
	ike_section_p2ids(r, fd);

	if (ike_connect(r, fd) == -1)
		return (-1);
	return (0);
}

static int
ike_delete_config(struct ipsec_rule *r, FILE *fd)
{
	ike_setup_ids(r);
#if 0
	switch (r->ikemode) {
	case IKE_ACTIVE:
	case IKE_DYNAMIC:
		fprintf(fd, "t IPsec-%s\n", r->p2name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, DELETE "[Phase 2]\n");
		fprintf(fd, "t IPsec-%s\n", r->p2name);
		break;
	default:
		return (-1);
	}

	if (r->peer) {
		fprintf(fd, DELETE "[peer-%s]\n", r->peer->name);
		fprintf(fd, DELETE "[mm-%s]\n", r->peer->name);
		fprintf(fd, DELETE "[am-%s]\n", r->peer->name);
	}
	if (r->auth) {
		if (r->auth->srcid)
			fprintf(fd, DELETE "[%s-ID]\n", r->auth->srcid);
		if (r->auth->dstid)
			fprintf(fd, DELETE "[%s-ID]\n", r->auth->dstid);
	}
	fprintf(fd, DELETE "[IPsec-%s]\n", r->p2name);
	fprintf(fd, DELETE "[qm-%s]\n", r->p2name);
	fprintf(fd, DELETE "[lid-%s]\n", r->p2lid);
	fprintf(fd, DELETE "[rid-%s]\n", r->p2rid);
#else
	fprintf(fd, "t IPsec-%s\n", r->p2name);
	switch (r->ikemode) {
	case IKE_ACTIVE:
	case IKE_DYNAMIC:
		fprintf(fd, RMV "[Phase 2]:Connections=IPsec-%s\n", r->p2name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, RMV "[Phase 2]:Passive-Connections=IPsec-%s\n",
		    r->p2name);
		break;
	default:
		return (-1);
	}
	fprintf(fd, DELETE "[IPsec-%s]\n", r->p2name);
	fprintf(fd, DELETE "[qm-%s]\n", r->p2name);
#endif

	return (0);
}

static void
ike_setup_ids(struct ipsec_rule *r)
{
	if (r->proto) {
		if (asprintf(&r->p2lid, "%s:%d-%d", r->src->name,
		     ntohs(r->sport), r->proto) == -1)
			err(1, "ike_setup_ids");
		if (asprintf(&r->p2rid, "%s:%d-%d", r->dst->name,
		     ntohs(r->dport), r->proto) == -1)
			err(1, "ike_setup_ids");
	} else {
		if (r->sport) {
			if (asprintf(&r->p2lid, "%s:%d", r->src->name,
			     ntohs(r->sport)) == -1)
				err(1, "ike_setup_ids");
		} else {
			if ((r->p2lid = strdup(r->src->name)) == NULL)
				err(1, "ike_setup_ids");
		}
		if (r->dport) {
			if (asprintf(&r->p2rid, "%s:%d", r->dst->name,
			     ntohs(r->dport)) == -1)
				err(1, "ike_setup_ids");
		} else {
			if ((r->p2rid = strdup(r->dst->name)) == NULL)
				err(1, "ike_setup_ids");
		}
	}
	if (asprintf(&r->p2name, "%s-%s", r->p2lid, r->p2rid) == -1)
		err(1, "ike_setup_ids");
}

int
ike_print_config(struct ipsec_rule *r, int opts)
{
	if (opts & IPSECCTL_OPT_DELETE)
		return (ike_delete_config(r, stdout));
	else
		return (ike_gen_config(r, stdout));
}

int
ike_ipsec_establish(int action, struct ipsec_rule *r)
{
	struct stat	 sb;
	FILE		*fdp;
	int		 fd, ret = 0;

	if ((fd = open(ISAKMPD_FIFO, O_WRONLY)) == -1)
		err(1, "ike_ipsec_establish: open(%s)", ISAKMPD_FIFO);
	if (fstat(fd, &sb) == -1)
		err(1, "ike_ipsec_establish: fstat(%s)", ISAKMPD_FIFO);
	if (!S_ISFIFO(sb.st_mode))
		errx(1, "ike_ipsec_establish: %s not a fifo", ISAKMPD_FIFO);
	if ((fdp = fdopen(fd, "w")) == NULL)
		err(1, "ike_ipsec_establish: fdopen(%s)", ISAKMPD_FIFO);

	switch (action) {
	case ACTION_ADD:
		ret = ike_gen_config(r, fdp);
		break;
	case ACTION_DELETE:
		ret = ike_delete_config(r, fdp);
		break;
	default:
		ret = -1;
	}

	fclose(fdp);
	return (ret);
}
