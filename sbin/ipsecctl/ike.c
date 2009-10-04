/*	$OpenBSD: ike.c,v 1.67 2009/10/04 11:39:32 jsing Exp $	*/
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

char *ike_id_types[] = {
	"", "", "IPV4_ADDR", "IPV6_ADDR", "FQDN", "USER_FQDN"
};

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
	if (r->peer)
		fprintf(fd, SET "[Phase 1]:%s=%s force\n", r->peer->name,
		    r->p1name);
	else
		fprintf(fd, SET "[Phase 1]:Default=%s force\n", r->p1name);
	fprintf(fd, SET "[%s]:Phase=1 force\n", r->p1name);
	if (r->peer)
		fprintf(fd, SET "[%s]:Address=%s force\n", r->p1name,
		    r->peer->name);
	if (r->local)
		fprintf(fd, SET "[%s]:Local-address=%s force\n", r->p1name,
		    r->local->name);
	if (r->ikeauth->type == IKE_AUTH_PSK)
		fprintf(fd, SET "[%s]:Authentication=%s force\n", r->p1name,
		    r->ikeauth->string);
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
		r->auth->srcid_type = ID_FQDN;
	}
	if (r->auth->srcid) {
		fprintf(fd, SET "[%s]:ID=id-%s force\n", r->p1name,
		    r->auth->srcid);
		fprintf(fd, SET "[id-%s]:ID-type=%s force\n", r->auth->srcid,
		    ike_id_types[r->auth->srcid_type]);
		if (r->auth->srcid_type == ID_IPV4 ||
		    r->auth->srcid_type == ID_IPV6)
			fprintf(fd, SET "[id-%s]:Address=%s force\n",
			    r->auth->srcid, r->auth->srcid);
		else
			fprintf(fd, SET "[id-%s]:Name=%s force\n",
			    r->auth->srcid, r->auth->srcid);
	}
	if (r->auth->dstid) {
		fprintf(fd, SET "[%s]:Remote-ID=id-%s force\n", r->p1name,
		    r->auth->dstid);
		fprintf(fd, SET "[id-%s]:ID-type=%s force\n", r->auth->dstid,
		    ike_id_types[r->auth->dstid_type]);
		if (r->auth->dstid_type == ID_IPV4 ||
		    r->auth->dstid_type == ID_IPV6)
			fprintf(fd, SET "[id-%s]:Address=%s force\n",
			    r->auth->dstid, r->auth->dstid);
		else
			fprintf(fd, SET "[id-%s]:Name=%s force\n",
			    r->auth->dstid, r->auth->dstid);
	}
}

static void
ike_section_ipsec(struct ipsec_rule *r, FILE *fd)
{
	fprintf(fd, SET "[%s]:Phase=2 force\n", r->p2name);
	fprintf(fd, SET "[%s]:ISAKMP-peer=%s force\n", r->p2name, r->p1name);
	fprintf(fd, SET "[%s]:Configuration=phase2-%s force\n", r->p2name,
	    r->p2name);
	fprintf(fd, SET "[%s]:Local-ID=%s force\n", r->p2name, r->p2lid);
	if (r->p2nid)
		fprintf(fd, SET "[%s]:NAT-ID=%s force\n", r->p2name, r->p2nid);
	fprintf(fd, SET "[%s]:Remote-ID=%s force\n", r->p2name, r->p2rid);

	if (r->tag)
		fprintf(fd, SET "[%s]:PF-Tag=%s force\n", r->p2name, r->tag);
}

static int
ike_section_p2(struct ipsec_rule *r, FILE *fd)
{
	char	*exchange_type, *sprefix;

	switch (r->p2ie) {
	case IKE_QM:
		exchange_type = "QUICK_MODE";
		sprefix = "QM";
		break;
	default:
		warnx("illegal phase 2 ike mode %d", r->p2ie);
		return (-1);
	}

	fprintf(fd, SET "[phase2-%s]:EXCHANGE_TYPE=%s force\n", r->p2name,
	    exchange_type);
	fprintf(fd, SET "[phase2-%s]:Suites=%s-", r->p2name, sprefix);

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
			case ENCXF_AES_128:
				fprintf(fd, "AES-128");
				break;
			case ENCXF_AES_192:
				fprintf(fd, "AES-192");
				break;
			case ENCXF_AES_256:
				fprintf(fd, "AES-256");
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
	char *exchange_type;

	switch (r->p1ie) {
	case IKE_MM:
		exchange_type = "ID_PROT";
		break;
	case IKE_AM:
		exchange_type = "AGGRESSIVE";
		break;
	default:
		warnx("illegal phase 2 ike mode %d", r->p1ie);
		return (-1);
	}

	fprintf(fd, SET "[%s]:Configuration=phase1-%s force\n", r->p1name,
	    r->p1name);
	fprintf(fd, SET "[phase1-%s]:EXCHANGE_TYPE=%s force\n", r->p1name,
	    exchange_type);
	fprintf(fd, ADD "[phase1-%s]:Transforms=", r->p1name);

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
		case ENCXF_AES_128:
			fprintf(fd, "AES-128");
			break;
		case ENCXF_AES_192:
			fprintf(fd, "AES-192");
			break;
		case ENCXF_AES_256:
			fprintf(fd, "AES-256");
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
ike_section_p2ids_net(struct ipsec_addr *iamask, sa_family_t af, char *name,
    char *p2xid, FILE *fd)
{
	char mask[NI_MAXHOST], *network, *p;
	struct sockaddr_storage sas;
	struct sockaddr *sa = (struct sockaddr *)&sas;

	bzero(&sas, sizeof(struct sockaddr_storage));
	bzero(mask, sizeof(mask));
	sa->sa_family = af;
	switch (af) {
	case AF_INET:
		sa->sa_len = sizeof(struct sockaddr_in);
		bcopy(&iamask->ipa,
		    &((struct sockaddr_in *)(sa))->sin_addr,
		    sizeof(struct in6_addr));
		break;
	case AF_INET6:
		sa->sa_len = sizeof(struct sockaddr_in6);
		bcopy(&iamask->ipa,
		    &((struct sockaddr_in6 *)(sa))->sin6_addr,
		    sizeof(struct in6_addr));
		break;
	}
	if (getnameinfo(sa, sa->sa_len, mask, sizeof(mask), NULL, 0,
	    NI_NUMERICHOST))
		errx(1, "could not get a numeric mask");

	if ((network = strdup(name)) == NULL)
		err(1, "ike_section_p2ids: strdup");
	if ((p = strrchr(network, '/')) != NULL)
		*p = '\0';

	fprintf(fd, SET "[%s]:ID-type=IPV%d_ADDR_SUBNET force\n",
	    p2xid, ((af == AF_INET) ? 4 : 6));
	fprintf(fd, SET "[%s]:Network=%s force\n", p2xid, network);
	fprintf(fd, SET "[%s]:Netmask=%s force\n", p2xid, mask);

	free(network);
}

static void
ike_section_p2ids(struct ipsec_rule *r, FILE *fd)
{
	char *p;
	struct ipsec_addr_wrap *src = r->src;
	struct ipsec_addr_wrap *dst = r->dst;

	if (src->netaddress) {
		ike_section_p2ids_net(&src->mask, src->af, src->name,
		    r->p2lid, fd);
	} else {
		fprintf(fd, SET "[%s]:ID-type=IPV%d_ADDR force\n",
		    r->p2lid, ((src->af == AF_INET) ? 4 : 6));
		if ((p = strrchr(src->name, '/')) != NULL)
			*p = '\0';
		fprintf(fd, SET "[%s]:Address=%s force\n", r->p2lid,
		    src->name);
	}

	if (src->srcnat && src->srcnat->netaddress) {
		ike_section_p2ids_net(&src->srcnat->mask, src->af, src->srcnat->name,
		    r->p2nid, fd);
	} else if (src->srcnat) {
		fprintf(fd, SET "[%s]:ID-type=IPV%d_ADDR force\n",
		    r->p2nid, ((src->af == AF_INET) ? 4 : 6));
		if ((p = strrchr(src->srcnat->name, '/')) != NULL)
			*p = '\0';
		fprintf(fd, SET "[%s]:Address=%s force\n", r->p2nid,
		    src->srcnat->name);
	}

	if (dst->netaddress) {
		ike_section_p2ids_net(&dst->mask, dst->af, dst->name,
		    r->p2rid, fd);
	} else {
		fprintf(fd, SET "[%s]:ID-type=IPV%d_ADDR force\n",
		    r->p2rid, ((dst->af == AF_INET) ? 4 : 6));
		if ((p = strrchr(dst->name, '/')) != NULL)
			*p = '\0';
		fprintf(fd, SET "[%s]:Address=%s force\n", r->p2rid,
		    dst->name);
	}
	if (r->proto) {
		fprintf(fd, SET "[%s]:Protocol=%d force\n",
		    r->p2lid, r->proto);
		fprintf(fd, SET "[%s]:Protocol=%d force\n",
		    r->p2rid, r->proto);
	}
	if (r->sport)
		fprintf(fd, SET "[%s]:Port=%d force\n", r->p2lid,
		    ntohs(r->sport));
	if (r->dport)
		fprintf(fd, SET "[%s]:Port=%d force\n", r->p2rid,
		    ntohs(r->dport));
}

static int
ike_connect(struct ipsec_rule *r, FILE *fd)
{
	switch (r->ikemode) {
	case IKE_ACTIVE:
	case IKE_DYNAMIC:
		fprintf(fd, ADD "[Phase 2]:Connections=%s\n", r->p2name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, ADD "[Phase 2]:Passive-Connections=%s\n",
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
		fprintf(fd, "t %s\n", r->p2name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, DELETE "[Phase 2]\n");
		fprintf(fd, "t %s\n", r->p2name);
		break;
	default:
		return (-1);
	}

	if (r->peer) {
		fprintf(fd, DELETE "[%s]\n", r->p1name);
		fprintf(fd, DELETE "[phase1-%s]\n", r->p1name);
	}
	if (r->auth) {
		if (r->auth->srcid)
			fprintf(fd, DELETE "[%s-ID]\n", r->auth->srcid);
		if (r->auth->dstid)
			fprintf(fd, DELETE "[%s-ID]\n", r->auth->dstid);
	}
	fprintf(fd, DELETE "[%s]\n", r->p2name);
	fprintf(fd, DELETE "[phase2-%s]\n", r->p2name);
	fprintf(fd, DELETE "[%s]\n", r->p2lid);
	fprintf(fd, DELETE "[%s]\n", r->p2rid);
#else
	fprintf(fd, "t %s\n", r->p2name);
	switch (r->ikemode) {
	case IKE_ACTIVE:
	case IKE_DYNAMIC:
		fprintf(fd, RMV "[Phase 2]:Connections=%s\n", r->p2name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, RMV "[Phase 2]:Passive-Connections=%s\n",
		    r->p2name);
		break;
	default:
		return (-1);
	}
	fprintf(fd, DELETE "[%s]\n", r->p2name);
	fprintf(fd, DELETE "[phase2-%s]\n", r->p2name);
#endif

	return (0);
}

static void
ike_setup_ids(struct ipsec_rule *r)
{
	char sproto[10], ssport[10], sdport[10];

	/* phase 1 name is peer and local address */
	if (r->peer) {
		if (r->local) {
			/* peer-dstaddr-local-srcaddr */
			if (asprintf(&r->p1name, "peer-%s-local-%s",
			    r->peer->name, r->local->name) == -1)
				err(1, "ike_setup_ids");
		} else
			/* peer-dstaddr */
			if (asprintf(&r->p1name, "peer-%s",
			    r->peer->name) == -1)
				err(1, "ike_setup_ids");
	} else
		if ((r->p1name = strdup("peer-default")) == NULL)
			err(1, "ike_setup_ids");

	/* Phase 2 name is from and to network, protocol, port*/
	sproto[0] = ssport[0] = sdport[0] = 0;
	if (r->proto)
		snprintf(sproto, sizeof sproto, "=%u", r->proto);
	if (r->sport)
		snprintf(ssport, sizeof ssport, ":%u", ntohs(r->sport));
	if (r->dport)
		snprintf(sdport, sizeof sdport, ":%u", ntohs(r->dport));
	/* from-network/masklen=proto:port */
	if (asprintf(&r->p2lid, "from-%s%s%s", r->src->name, sproto, ssport)
	    == -1)
		err(1, "ike_setup_ids");
	/* to-network/masklen=proto:port */
	if (asprintf(&r->p2rid, "to-%s%s%s", r->dst->name, sproto, sdport)
	    == -1)
		err(1, "ike_setup_ids");
	/* from-network/masklen=proto:port-to-network/masklen=proto:port */
	if (asprintf(&r->p2name, "%s-%s", r->p2lid , r->p2rid) == -1)
		err(1, "ike_setup_ids");
	/* nat-network/masklen=proto:port */
	if (r->src->srcnat && r->src->srcnat->name) {
		if (asprintf(&r->p2nid, "nat-%s%s%s", r->src->srcnat->name, sproto,
		    ssport) == -1)
			err(1, "ike_setup_ids");
	}
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
