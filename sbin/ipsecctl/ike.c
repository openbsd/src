/*	$OpenBSD: ike.c,v 1.5 2005/10/16 19:52:19 hshoexer Exp $	*/
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
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipsecctl.h"

static void	ike_section_peer(struct ipsec_addr *, FILE *);
static void	ike_section_ids(struct ipsec_addr *, struct ipsec_auth *,
		    FILE *);
static void	ike_section_ipsec(struct ipsec_addr *, struct ipsec_addr *,
		    struct ipsec_addr *, FILE *);
static int	ike_section_qm(struct ipsec_addr *, struct ipsec_addr *,
		    u_int8_t, struct ipsec_transforms *, FILE *);
static int	ike_section_mm(struct ipsec_addr *, struct ipsec_transforms *,
		    FILE *);
static void	ike_section_qmids(struct ipsec_addr *, struct ipsec_addr *,
		    FILE *);
static int	ike_connect(u_int8_t, struct ipsec_addr *, struct ipsec_addr *,
		    FILE *);
static int	ike_gen_config(struct ipsec_rule *, FILE *);
static int	ike_delete_config(struct ipsec_rule *, FILE *);

int		ike_print_config(struct ipsec_rule *, int);
int		ike_ipsec_establish(int, struct ipsec_rule *);

#define	SET	"C set "
#define	ADD	"C add "
#define	DELETE	"C rms "

static void
ike_section_peer(struct ipsec_addr *peer, FILE *fd)
{
	fprintf(fd, SET "[Phase 1]:%s=peer-%s force\n", peer->name, peer->name);
	fprintf(fd, SET "[peer-%s]:Phase=1 force\n", peer->name);
	fprintf(fd, SET "[peer-%s]:Address=%s force\n", peer->name, peer->name);
}

static void
ike_section_ids(struct ipsec_addr *peer, struct ipsec_auth *auth, FILE *fd)
{
	if (auth == NULL)
		return;

	if (auth->srcid) {
		fprintf(fd, SET "[peer-%s]:ID=%s-ID force\n", peer->name,
		    "local");
		fprintf(fd, SET "[%s-ID]:ID-type=FQDN force\n", "local");
		fprintf(fd, SET "[%s-ID]:Name=%s force\n", "local",
		    auth->srcid);
	}
	if (auth->dstid) {
		fprintf(fd, SET "[peer-%s]:Remote-ID=%s-ID force\n",
		    peer->name, peer->name);
		fprintf(fd, SET "[%s-ID]:ID-type=FQDN force\n", peer->name);
		fprintf(fd, SET "[%s-ID]:Name=%s force\n", peer->name,
		    auth->dstid);
	}
}

static void
ike_section_ipsec(struct ipsec_addr *src, struct ipsec_addr *dst, struct
    ipsec_addr *peer, FILE *fd)
{
	fprintf(fd, SET "[IPsec-%s-%s]:Phase=2 force\n", src->name, dst->name);
	fprintf(fd, SET "[IPsec-%s-%s]:ISAKMP-peer=peer-%s force\n", src->name,
	    dst->name, peer->name);
	fprintf(fd, SET "[IPsec-%s-%s]:Configuration=qm-%s-%s force\n",
	    src->name, dst->name, src->name, dst->name);
	fprintf(fd, SET "[IPsec-%s-%s]:Local-ID=lid-%s force\n", src->name,
	    dst->name, src->name);
	fprintf(fd, SET "[IPsec-%s-%s]:Remote-ID=rid-%s force\n", src->name,
	    dst->name, dst->name);
}

static int
ike_section_qm(struct ipsec_addr *src, struct ipsec_addr *dst, u_int8_t proto,
    struct ipsec_transforms *qmxfs, FILE *fd)
{
	fprintf(fd, SET "[qm-%s-%s]:EXCHANGE_TYPE=QUICK_MODE force\n",
	    src->name, dst->name);
	fprintf(fd, SET "[qm-%s-%s]:Suites=QM-", src->name, dst->name);
	
	switch (proto) {
	case IPSEC_ESP:
		fprintf(fd, "ESP");
		break;
	default:
		warnx("illegal protocol %d", proto);
		return (-1);
	};
	fprintf(fd, "-");

	if (qmxfs->encxf) {
		switch (qmxfs->encxf->id) {
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
			warnx("illegal transform %s", qmxfs->encxf->name);
			return (-1);
		};
	} else
		fprintf(fd, "AES");
	fprintf(fd, "-");

	if (qmxfs->authxf) {
		switch (qmxfs->authxf->id) {
		case AUTHXF_HMAC_MD5:
		case AUTHXF_MD5:
			fprintf(fd, "MD5");
			break;
		case AUTHXF_HMAC_SHA1:
		case AUTHXF_SHA1:
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
			warnx("illegal transform %s", qmxfs->authxf->name);
			return (-1);
		};
	} else
		fprintf(fd, "SHA2-256");
	fprintf(fd, "-PFS-SUITE force\n");

	return (0);
}

static int
ike_section_mm(struct ipsec_addr *peer, struct ipsec_transforms *mmxfs,
    FILE *fd)
{
	if (!(mmxfs->authxf || mmxfs->encxf))
		return (0);

	fprintf(fd, SET "[peer-%s]:Configuration=mm-%s force\n", peer->name,
	    peer->name);
	fprintf(fd, SET "[mm-%s]:EXCHANGE_TYPE=ID_PROT force\n", peer->name);
	fprintf(fd, ADD "[mm-%s]:Transforms=", peer->name);

	if (mmxfs->encxf) {
		switch (mmxfs->encxf->id) {
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
			warnx("illegal transform %s", mmxfs->encxf->name);
			return (-1);
		};
	} else
		fprintf(fd, "AES");
	fprintf(fd, "-");

	if (mmxfs->authxf) {
		switch (mmxfs->authxf->id) {
		case AUTHXF_HMAC_MD5:
		case AUTHXF_MD5:
			fprintf(fd, "MD5");
			break;
		case AUTHXF_HMAC_SHA1:
		case AUTHXF_SHA1:
			fprintf(fd, "SHA");
			break;
		default:
			warnx("illegal transform %s", mmxfs->authxf->name);
			return (-1);
		};
	} else
		fprintf(fd, "SHA");

	fprintf(fd, "-RSA_SIG\n");

	return (0);
}

static void
ike_section_qmids(struct ipsec_addr *src, struct ipsec_addr *dst, FILE *fd)
{
	char *mask, *network, *p;

	if (src->netaddress) {
		mask = inet_ntoa(src->v4mask.mask);
		if ((network = strdup(src->name)) == NULL)
			err(1, "ike_section_qmids: strdup");
		if ((p = strrchr(network, '/')) != NULL)
			*p = '\0';

		fprintf(fd, SET "[lid-%s]:ID-type=IPV4_ADDR_SUBNET force\n",
		    src->name);
		fprintf(fd, SET "[lid-%s]:Network=%s force\n", src->name,
		    network);
		fprintf(fd, SET "[lid-%s]:Netmask=%s force\n", src->name, mask);

		free(network);
	} else {
		fprintf(fd, SET "[lid-%s]:ID-type=IPV4_ADDR force\n",
		    src->name);
		fprintf(fd, SET "[lid-%s]:Address=%s force\n", src->name,
		    src->name);
	}
	if (dst->netaddress) {
		mask = inet_ntoa(dst->v4mask.mask);
		if ((network = strdup(dst->name)) == NULL)
			err(1, "ike_section_qmids: strdup");
		if ((p = strrchr(network, '/')) != NULL)
			*p = '\0';

		fprintf(fd, SET "[rid-%s]:ID-type=IPV4_ADDR_SUBNET force\n",
		    dst->name);
		fprintf(fd, SET "[rid-%s]:Network=%s force\n", dst->name,
		    network);
		fprintf(fd, SET "[rid-%s]:Netmask=%s force\n", dst->name, mask);
		
		free(network);
	} else {
		fprintf(fd, SET "[rid-%s]:ID-type=IPV4_ADDR force\n",
		    dst->name);
		fprintf(fd, SET "[rid-%s]:Address=%s force\n", dst->name,
		    dst->name);
	}
}

static int
ike_connect(u_int8_t mode, struct ipsec_addr *src, struct ipsec_addr *dst,
    FILE *fd)
{
	switch (mode) {
	case IKE_ACTIVE:
		fprintf(fd, ADD "[Phase 2]:Connections=IPsec-%s-%s\n",
		    src->name, dst->name);
		fprintf(fd, "t IPsec-%s-%s\n", src->name, dst->name);
		fprintf(fd, "c IPsec-%s-%s\n", src->name, dst->name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, ADD "[Phase 2]:Passive-Connections=IPsec-%s-%s\n",
		    src->name, dst->name);
		break;
	default:
		return (-1);
	}
	return (0);
}

static int
ike_gen_config(struct ipsec_rule *r, FILE *fd)
{
	ike_section_peer(r->peer, fd);
	if (ike_section_mm(r->peer, r->mmxfs, fd) == -1)
		return (-1);
	ike_section_ids(r->peer, r->auth, fd);
	ike_section_ipsec(r->src, r->dst, r->peer, fd);
	if (ike_section_qm(r->src, r->dst, r->proto, r->qmxfs, fd) == -1)
		return (-1);
	ike_section_qmids(r->src, r->dst, fd);

	if (ike_connect(r->ikemode, r->src, r->dst, fd) == -1)
		return (-1);

	return (0);
}

static int
ike_delete_config(struct ipsec_rule *r, FILE *fd)
{
	switch (r->ikemode) {
	case IKE_ACTIVE:
		fprintf(fd, "t IPsec-%s-%s\n", r->src->name, r->dst->name);
		break;
	case IKE_PASSIVE:
		fprintf(fd, DELETE "[Phase 2]\n");
		fprintf(fd, "t IPsec-%s-%s\n", r->src->name, r->dst->name);
		break;
	default:
		return (-1);
	}

	fprintf(fd, DELETE "[peer-%s]\n", r->peer->name);
	if (r->auth) {
		if (r->auth->srcid)
			fprintf(fd, DELETE "[%s-ID]\n", r->auth->srcid);
		if (r->auth->dstid)
			fprintf(fd, DELETE "[%s-ID]\n", r->auth->dstid);
	}
	fprintf(fd, DELETE "[IPsec-%s-%s]\n", r->src->name, r->dst->name);
	fprintf(fd, DELETE "[qm-%s-%s]\n", r->src->name, r->dst->name);
	fprintf(fd, DELETE "[mm-%s]\n", r->peer->name);
	fprintf(fd, DELETE "[lid-%s]\n", r->src->name);
	fprintf(fd, DELETE "[rid-%s]\n", r->dst->name);

	return (0);
}

int
ike_print_config(struct ipsec_rule *r, int opts)
{
	return (ike_gen_config(r, stdout));
}

int
ike_ipsec_establish(int action, struct ipsec_rule *r)
{
	FILE	*fd;
	int	 ret = 0;

	if ((fd = fopen("/var/run/isakmpd.fifo", "w")) == NULL)
		errx(1, "fopen");

	switch (action) {
	case ACTION_ADD:
		ret = ike_gen_config(r, fd);
		break;
	case ACTION_DELETE:
		ret = ike_delete_config(r, fd);
		break;
	default:
		ret = -1;
	}

	fclose(fd);
	return (ret);
}
