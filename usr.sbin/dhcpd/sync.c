/*	$OpenBSD: sync.c,v 1.10 2010/12/23 17:38:04 claudio Exp $	*/

/*
 * Copyright (c) 2008 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2006, 2007 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/queue.h>


#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sha1.h>

#include <netdb.h>

#include <openssl/hmac.h>

#include "dhcpd.h"
#include "sync.h"

int sync_debug;

u_int32_t sync_counter;
int syncfd = -1;
int sendmcast;

struct sockaddr_in sync_in;
struct sockaddr_in sync_out;
static char *sync_key;

struct sync_host {
	LIST_ENTRY(sync_host)	h_entry;

	char			*h_name;
	struct sockaddr_in	sh_addr;
};
LIST_HEAD(synchosts, sync_host) sync_hosts = LIST_HEAD_INITIALIZER(sync_hosts);

void	 sync_send(struct iovec *, int);

int
sync_addhost(const char *name, u_short port)
{
	struct addrinfo hints, *res, *res0;
	struct sync_host *shost;
	struct sockaddr_in *addr = NULL;

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(name, NULL, &hints, &res0) != 0)
		return (EINVAL);
	for (res = res0; res != NULL; res = res->ai_next) {
		if (addr == NULL && res->ai_family == AF_INET) {
			addr = (struct sockaddr_in *)res->ai_addr;
			break;
		}
	}
	if (addr == NULL) {
		freeaddrinfo(res0);
		return (EINVAL);
	}
	if ((shost = (struct sync_host *)
	    calloc(1, sizeof(struct sync_host))) == NULL) {
		freeaddrinfo(res0);
		return (ENOMEM);
	}
	shost->h_name = strdup(name);
	if (shost->h_name == NULL) {
		free(shost);
		freeaddrinfo(res0);
		return (ENOMEM);
	}

	shost->sh_addr.sin_family = AF_INET;
	shost->sh_addr.sin_port = htons(port);
	shost->sh_addr.sin_addr.s_addr = addr->sin_addr.s_addr;
	freeaddrinfo(res0);

	LIST_INSERT_HEAD(&sync_hosts, shost, h_entry);

	if (sync_debug)
		note("added dhcp sync host %s (address %s, port %d)\n",
		    shost->h_name, inet_ntoa(shost->sh_addr.sin_addr), port);

	return (0);
}

int
sync_init(const char *iface, const char *baddr, u_short port)
{
	int one = 1;
	u_int8_t ttl;
	struct ifreq ifr;
	struct ip_mreq mreq;
	struct sockaddr_in *addr;
	char ifnam[IFNAMSIZ], *ttlstr;
	const char *errstr;
	struct in_addr ina;

	if (iface != NULL)
		sendmcast++;

	bzero(&ina, sizeof(ina));
	if (baddr != NULL) {
		if (inet_pton(AF_INET, baddr, &ina) != 1) {
			ina.s_addr = htonl(INADDR_ANY);
			if (iface == NULL)
				iface = baddr;
			else if (iface != NULL && strcmp(baddr, iface) != 0) {
				fprintf(stderr, "multicast interface does "
				    "not match");
				return (-1);
			}
		}
	}

	sync_key = SHA1File(DHCP_SYNC_KEY, NULL);
	if (sync_key == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "failed to open sync key: %s\n",
			    strerror(errno));
			return (-1);
		}
		/* Use empty key by default */
		sync_key = "";
	}

	syncfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (syncfd == -1)
		return (-1);

	if (setsockopt(syncfd, SOL_SOCKET, SO_REUSEADDR, &one,
	    sizeof(one)) == -1)
		goto fail;

	bzero(&sync_out, sizeof(sync_out));
	sync_out.sin_family = AF_INET;
	sync_out.sin_len = sizeof(sync_out);
	sync_out.sin_addr.s_addr = ina.s_addr;
	if (baddr == NULL && iface == NULL)
		sync_out.sin_port = 0;
	else
		sync_out.sin_port = htons(port);

	if (bind(syncfd, (struct sockaddr *)&sync_out, sizeof(sync_out)) == -1)
		goto fail;

	/* Don't use multicast messages */
	if (iface == NULL)
		return (syncfd);

	strlcpy(ifnam, iface, sizeof(ifnam));
	ttl = DHCP_SYNC_MCASTTTL;
	if ((ttlstr = strchr(ifnam, ':')) != NULL) {
		*ttlstr++ = '\0';
		ttl = (u_int8_t)strtonum(ttlstr, 1, UINT8_MAX, &errstr);
		if (errstr) {
			fprintf(stderr, "invalid multicast ttl %s: %s",
			    ttlstr, errstr);
			goto fail;
		}
	}

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifnam, sizeof(ifr.ifr_name));
	if (ioctl(syncfd, SIOCGIFADDR, &ifr) == -1)
		goto fail;

	bzero(&sync_in, sizeof(sync_in));
	addr = (struct sockaddr_in *)&ifr.ifr_addr;
	sync_in.sin_family = AF_INET;
	sync_in.sin_len = sizeof(sync_in);
	sync_in.sin_addr.s_addr = addr->sin_addr.s_addr;
	sync_in.sin_port = htons(port);

	bzero(&mreq, sizeof(mreq));
	sync_out.sin_addr.s_addr = inet_addr(DHCP_SYNC_MCASTADDR);
	mreq.imr_multiaddr.s_addr = inet_addr(DHCP_SYNC_MCASTADDR);
	mreq.imr_interface.s_addr = sync_in.sin_addr.s_addr;

	if (setsockopt(syncfd, IPPROTO_IP,
	    IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
		fprintf(stderr, "failed to add multicast membership to %s: %s",
		    DHCP_SYNC_MCASTADDR, strerror(errno));
		goto fail;
	}
	if (setsockopt(syncfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
	    sizeof(ttl)) == -1) {
		fprintf(stderr, "failed to set multicast ttl to "
		    "%u: %s\n", ttl, strerror(errno));
		setsockopt(syncfd, IPPROTO_IP,
		    IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
		goto fail;
	}

	if (sync_debug)
		syslog_r(LOG_DEBUG, &sdata, "using multicast dhcp sync %smode "
		    "(ttl %u, group %s, port %d)\n",
		    sendmcast ? "" : "receive ",
		    ttl, inet_ntoa(sync_out.sin_addr), port);

	return (syncfd);

 fail:
	close(syncfd);
	return (-1);
}

void
sync_recv(void)
{
	struct dhcp_synchdr *hdr;
	struct sockaddr_in addr;
	struct dhcp_synctlv_hdr *tlv;
	struct dhcp_synctlv_lease *lv;
	struct lease	*lease;
	u_int8_t buf[DHCP_SYNC_MAXSIZE];
	u_int8_t hmac[2][DHCP_SYNC_HMAC_LEN];
	struct lease l, *lp;
	u_int8_t *p;
	socklen_t addr_len;
	ssize_t len;
	u_int hmac_len;

	bzero(&addr, sizeof(addr));
	bzero(buf, sizeof(buf));

	addr_len = sizeof(addr);
	if ((len = recvfrom(syncfd, buf, sizeof(buf), 0,
	    (struct sockaddr *)&addr, &addr_len)) < 1)
		return;
	if (addr.sin_addr.s_addr != htonl(INADDR_ANY) &&
	    bcmp(&sync_in.sin_addr, &addr.sin_addr,
	    sizeof(addr.sin_addr)) == 0)
		return;

	/* Ignore invalid or truncated packets */
	hdr = (struct dhcp_synchdr *)buf;
	if (len < sizeof(struct dhcp_synchdr) ||
	    hdr->sh_version != DHCP_SYNC_VERSION ||
	    hdr->sh_af != AF_INET ||
	    len < ntohs(hdr->sh_length))
		goto trunc;
	len = ntohs(hdr->sh_length);

	/* Compute and validate HMAC */
	bcopy(hdr->sh_hmac, hmac[0], DHCP_SYNC_HMAC_LEN);
	bzero(hdr->sh_hmac, DHCP_SYNC_HMAC_LEN);
	HMAC(EVP_sha1(), sync_key, strlen(sync_key), buf, len,
	    hmac[1], &hmac_len);
	if (bcmp(hmac[0], hmac[1], DHCP_SYNC_HMAC_LEN) != 0)
		goto trunc;

	if (sync_debug)
		note("%s(sync): received packet of %d bytes\n",
		    inet_ntoa(addr.sin_addr), (int)len);

	p = (u_int8_t *)(hdr + 1);
	while (len) {
		tlv = (struct dhcp_synctlv_hdr *)p;

		if (len < sizeof(struct dhcp_synctlv_hdr) ||
		    len < ntohs(tlv->st_length))
			goto trunc;

		switch (ntohs(tlv->st_type)) {
		case DHCP_SYNC_LEASE:
			lv = (struct dhcp_synctlv_lease *)tlv;
			if (sizeof(*lv) > ntohs(tlv->st_length))
				goto trunc;
			if ((lease = find_lease_by_hw_addr(
				    lv->hardware_addr.haddr,
				    lv->hardware_addr.hlen)) == NULL) {
				if ((lease = find_lease_by_hw_addr(
					    lv->hardware_addr.haddr,
					    lv->hardware_addr.hlen)) == NULL) {
					lp = &l;
					memset(lp, 0, sizeof(*lp));
				} else
					lp = lease;
			} else
				lp = lease;

			lp = &l;
			memset(lp, 0, sizeof(*lp));
			lp->timestamp = ntohl(lv->timestamp);
			lp->starts = ntohl(lv->starts);
			lp->ends = ntohl(lv->ends);
			memcpy(&lp->ip_addr, &lv->ip_addr,
			    sizeof(lp->ip_addr));
			memcpy(&lp->hardware_addr, &lv->hardware_addr,
			    sizeof(lp->hardware_addr));
			note("DHCP_SYNC_LEASE from %s for hw %s -> ip %s, "
			    "start %d, end %d",
			    inet_ntoa(addr.sin_addr),
			    print_hw_addr(lp->hardware_addr.htype,
			    lp->hardware_addr.hlen, lp->hardware_addr.haddr),
			    piaddr(lp->ip_addr), lp->starts, lp->ends);
			/* now whack the lease in there */
			if (lease == NULL) {
				enter_lease(lp);
				write_leases();
			}
			else if (lease->ends < lp->ends)
				supersede_lease(lease, lp, 1);
			else if (lease->ends > lp->ends)
				/*
				 * our partner sent us a lease
				 * that is older than what we have,
				 * so re-educate them with what we
				 * know is newer.
				 */
				sync_lease(lease);
			break;
		case DHCP_SYNC_END:
			goto done;
		default:
			printf("invalid type: %d\n", ntohs(tlv->st_type));
			goto trunc;
		}
		len -= ntohs(tlv->st_length);
		p = ((u_int8_t *)tlv) + ntohs(tlv->st_length);
	}

 done:
	return;

 trunc:
	if (sync_debug)
		note("%s(sync): truncated or invalid packet\n",
		    inet_ntoa(addr.sin_addr));
}

void
sync_send(struct iovec *iov, int iovlen)
{
	struct sync_host *shost;
	struct msghdr msg;
	
	if (syncfd == -1)
		return;

	/* setup buffer */
	bzero(&msg, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = iovlen;

	if (sendmcast) {
		if (sync_debug)
			note("sending multicast sync message\n");
		msg.msg_name = &sync_out;
		msg.msg_namelen = sizeof(sync_out);
		if (sendmsg(syncfd, &msg, 0) == -1)
			warning("sending multicast sync message failed: %m");
	}

	LIST_FOREACH(shost, &sync_hosts, h_entry) {
		if (sync_debug)
			note("sending sync message to %s (%s)\n",
			    shost->h_name, inet_ntoa(shost->sh_addr.sin_addr));
		msg.msg_name = &shost->sh_addr;
		msg.msg_namelen = sizeof(shost->sh_addr);
		if (sendmsg(syncfd, &msg, 0) == -1)
			warning("sending sync message failed: %m");
	}
}

void
sync_lease(struct lease *lease)
{
	struct iovec iov[4];
	struct dhcp_synchdr hdr;
	struct dhcp_synctlv_lease ld;
	struct dhcp_synctlv_hdr end;
	char pad[DHCP_ALIGNBYTES];
	u_int16_t leaselen, padlen;
	int i = 0;
	HMAC_CTX ctx;
	u_int hmac_len;

	if (sync_key == NULL)
		return;

	bzero(&hdr, sizeof(hdr));
	bzero(&ld, sizeof(ld));
	bzero(&pad, sizeof(pad));

	HMAC_CTX_init(&ctx);
	HMAC_Init(&ctx, sync_key, strlen(sync_key), EVP_sha1());

	leaselen = sizeof(ld);
	padlen = DHCP_ALIGN(leaselen) - leaselen;

	/* Add DHCP sync packet header */
	hdr.sh_version = DHCP_SYNC_VERSION;
	hdr.sh_af = AF_INET;
	hdr.sh_counter = sync_counter++;
	hdr.sh_length = htons(sizeof(hdr) + sizeof(ld) + sizeof(end));
	iov[i].iov_base = &hdr;
	iov[i].iov_len = sizeof(hdr);
	HMAC_Update(&ctx, iov[i].iov_base, iov[i].iov_len);
	i++;

	/* Add single DHCP sync address entry */
	ld.type = htons(DHCP_SYNC_LEASE);
	ld.length = htons(leaselen + padlen);
	ld.timestamp = htonl(lease->timestamp);
	ld.starts = htonl(lease->starts);
	ld.ends =  htonl(lease->ends);
	memcpy(&ld.ip_addr, &lease->ip_addr, sizeof(ld.ip_addr));
	memcpy(&ld.hardware_addr, &lease->hardware_addr,
	    sizeof(ld.hardware_addr));
	note("sending DHCP_SYNC_LEASE for hw %s -> ip %s, start %d, end %d",
	    print_hw_addr(ld.hardware_addr.htype, ld.hardware_addr.hlen,
	    ld.hardware_addr.haddr), piaddr(lease->ip_addr), ntohl(ld.starts),
	    ntohl(ld.ends));
	iov[i].iov_base = &ld;
	iov[i].iov_len = sizeof(ld);
	HMAC_Update(&ctx, iov[i].iov_base, iov[i].iov_len);
	i++;

	iov[i].iov_base = pad;
	iov[i].iov_len = padlen;
	HMAC_Update(&ctx, iov[i].iov_base, iov[i].iov_len);
	i++;

	/* Add end marker */
	end.st_type = htons(DHCP_SYNC_END);
	end.st_length = htons(sizeof(end));
	iov[i].iov_base = &end;
	iov[i].iov_len = sizeof(end);
	HMAC_Update(&ctx, iov[i].iov_base, iov[i].iov_len);
	i++;

	HMAC_Final(&ctx, hdr.sh_hmac, &hmac_len);

	/* Send message to the target hosts */
	sync_send(iov, i);
	HMAC_CTX_cleanup(&ctx);
}
