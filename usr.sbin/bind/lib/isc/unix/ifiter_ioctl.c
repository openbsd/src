/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: ifiter_ioctl.c,v 1.19.2.2 2001/12/12 00:16:47 marka Exp $ */

/*
 * Obtain the list of network interfaces using the SIOCGLIFCONF ioctl.
 * See netintro(4).
 */
#ifndef SIOCGLIFCONF
#define SIOCGLIFCONF SIOCGIFCONF
#define lifc_len ifc_len
#define lifc_buf ifc_buf
#define lifc_req ifc_req
#define lifconf ifconf
#else
#define ISC_HAVE_LIFC_FAMILY 1
#define ISC_HAVE_LIFC_FLAGS 1
#endif
#ifndef SIOCGLIFADDR
#define SIOCGLIFADDR SIOCGIFADDR
#define SIOCGLIFFLAGS SIOCGIFFLAGS
#define SIOCGLIFDSTADDR SIOCGIFDSTADDR
#define SIOCGLIFNETMASK SIOCGIFNETMASK
#define lifr_addr ifr_addr
#define lifr_name ifr_name
#define	lifr_dstaddr ifr_dstaddr
#define lifr_flags ifr_flags
#define ss_family sa_family
#define lifreq ifreq
#endif


#define IFITER_MAGIC		ISC_MAGIC('I', 'F', 'I', 'T')
#define VALID_IFITER(t)		ISC_MAGIC_VALID(t, IFITER_MAGIC)

struct isc_interfaceiter {
	unsigned int		magic;		/* Magic number. */
	isc_mem_t		*mctx;
	int			socket;
	struct lifconf 		ifc;
	void			*buf;		/* Buffer for sysctl data. */
	unsigned int		bufsize;	/* Bytes allocated. */
	unsigned int		pos;		/* Current offset in
						   SIOCGLIFCONF data */
	isc_interface_t		current;	/* Current interface data. */
	isc_result_t		result;		/* Last result code. */
};


/*
 * Size of buffer for SIOCGLIFCONF, in bytes.  We assume no sane system
 * will have more than a megabyte of interface configuration data.
 */
#define IFCONF_BUFSIZE_INITIAL	4096
#define IFCONF_BUFSIZE_MAX	1048576

isc_result_t
isc_interfaceiter_create(isc_mem_t *mctx, isc_interfaceiter_t **iterp) {
	isc_interfaceiter_t *iter;
	isc_result_t result;
	char strbuf[ISC_STRERRORSIZE];

	REQUIRE(mctx != NULL);
	REQUIRE(iterp != NULL);
	REQUIRE(*iterp == NULL);

	iter = isc_mem_get(mctx, sizeof(*iter));
	if (iter == NULL)
		return (ISC_R_NOMEMORY);

	iter->mctx = mctx;
	iter->buf = NULL;

	/*
	 * Create an unbound datagram socket to do the SIOCGLIFADDR ioctl on.
	 */
	if ((iter->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 isc_msgcat_get(isc_msgcat,
						ISC_MSGSET_IFITERIOCTL,
						ISC_MSG_MAKESCANSOCKET,
						"making interface "
						"scan socket: %s"),
				 strbuf);
		result = ISC_R_UNEXPECTED;
		goto socket_failure;
	}

	/*
	 * Get the interface configuration, allocating more memory if
	 * necessary.
	 */
	iter->bufsize = IFCONF_BUFSIZE_INITIAL;

	for (;;) {
		iter->buf = isc_mem_get(mctx, iter->bufsize);
		if (iter->buf == NULL) {
			result = ISC_R_NOMEMORY;
			goto alloc_failure;
		}

		memset(&iter->ifc.lifc_len, 0, sizeof(iter->ifc.lifc_len));
#ifdef ISC_HAVE_LIFC_FAMILY
		iter->ifc.lifc_family = AF_UNSPEC;
#endif
#ifdef ISC_HAVE_LIFC_FLAGS
		iter->ifc.lifc_flags = 0;
#endif
		iter->ifc.lifc_len = iter->bufsize;
		iter->ifc.lifc_buf = iter->buf;
		/*
		 * Ignore the HP/UX warning about "interger overflow during
		 * conversion".  It comes from its own macro definition,
		 * and is really hard to shut up.
		 */
		if (ioctl(iter->socket, SIOCGLIFCONF, (char *)&iter->ifc)
		    == -1) {
			if (errno != EINVAL) {
				isc__strerror(errno, strbuf, sizeof(strbuf));
				UNEXPECTED_ERROR(__FILE__, __LINE__,
						 isc_msgcat_get(isc_msgcat,
							ISC_MSGSET_IFITERIOCTL,
							ISC_MSG_GETIFCONFIG,
							"get interface "
							"configuration: %s"),
						 strbuf);
				result = ISC_R_UNEXPECTED;
				goto ioctl_failure;
			}
			/*
			 * EINVAL.  Retry with a bigger buffer.
			 */
		} else {
			/*
			 * The ioctl succeeded.
			 * Some OS's just return what will fit rather
			 * than set EINVAL if the buffer is too small
			 * to fit all the interfaces in.  If
			 * ifc.lifc_len is too near to the end of the
			 * buffer we will grow it just in case and
			 * retry.
			 */
			if (iter->ifc.lifc_len + 2 * sizeof(struct lifreq)
			    < iter->bufsize)
				break;
		}
		if (iter->bufsize >= IFCONF_BUFSIZE_MAX) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 isc_msgcat_get(isc_msgcat,
							ISC_MSGSET_IFITERIOCTL,
							ISC_MSG_BUFFERMAX,
							"get interface "
							"configuration: "
							"maximum buffer "
							"size exceeded"));
			result = ISC_R_UNEXPECTED;
			goto ioctl_failure;
		}
		isc_mem_put(mctx, iter->buf, iter->bufsize);

		iter->bufsize *= 2;
	}

	/*
	 * A newly created iterator has an undefined position
	 * until isc_interfaceiter_first() is called.
	 */
	iter->pos = (unsigned int) -1;
	iter->result = ISC_R_FAILURE;

	iter->magic = IFITER_MAGIC;
	*iterp = iter;
	return (ISC_R_SUCCESS);

 ioctl_failure:
	isc_mem_put(mctx, iter->buf, iter->bufsize);

 alloc_failure:
	(void) close(iter->socket);

 socket_failure:
	isc_mem_put(mctx, iter, sizeof *iter);
	return (result);
}

/*
 * Get information about the current interface to iter->current.
 * If successful, return ISC_R_SUCCESS.
 * If the interface has an unsupported address family, or if
 * some operation on it fails, return ISC_R_IGNORE to make
 * the higher-level iterator code ignore it.
 */

static isc_result_t
internal_current(isc_interfaceiter_t *iter) {
	struct lifreq *ifrp;
	struct lifreq lifreq;
	int family;
	char strbuf[ISC_STRERRORSIZE];

	REQUIRE(VALID_IFITER(iter));
	REQUIRE (iter->pos < (unsigned int) iter->ifc.lifc_len);

	ifrp = (struct lifreq *)((char *) iter->ifc.lifc_req + iter->pos);

	memset(&lifreq, 0, sizeof lifreq);
	memcpy(&lifreq, ifrp, sizeof lifreq);

	family = lifreq.lifr_addr.ss_family;
	if (family != AF_INET)
		return (ISC_R_IGNORE);

	memset(&iter->current, 0, sizeof(iter->current));
	iter->current.af = family;

	INSIST(sizeof(lifreq.lifr_name) <= sizeof(iter->current.name));
	memset(iter->current.name, 0, sizeof(iter->current.name));
	memcpy(iter->current.name, lifreq.lifr_name, sizeof(lifreq.lifr_name));

	get_addr(family, &iter->current.address,
		 (struct sockaddr *)&lifreq.lifr_addr);

	/*
	 * If the interface does not have a address ignore it.
	 */
	switch (family) {
	case AF_INET:
		if (iter->current.address.type.in.s_addr == htonl(INADDR_ANY))
			return (ISC_R_IGNORE);
		break;
	case AF_INET6:
		if (memcmp(&iter->current.address.type.in6, &in6addr_any,
			   sizeof(in6addr_any)) == 0)
			return (ISC_R_IGNORE);
		break;
	}

	/*
	 * Get interface flags.
	 */

	iter->current.flags = 0;

	/*
	 * Ignore the HP/UX warning about "interger overflow during
	 * conversion.  It comes from its own macro definition,
	 * and is really hard to shut up.
	 */
	if (ioctl(iter->socket, SIOCGLIFFLAGS, (char *) &lifreq) < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "%s: getting interface flags: %s",
				 lifreq.lifr_name, strbuf);
		return (ISC_R_IGNORE);
	}

	if ((lifreq.lifr_flags & IFF_UP) != 0)
		iter->current.flags |= INTERFACE_F_UP;

	if ((lifreq.lifr_flags & IFF_POINTOPOINT) != 0)
		iter->current.flags |= INTERFACE_F_POINTTOPOINT;

	if ((lifreq.lifr_flags & IFF_LOOPBACK) != 0)
		iter->current.flags |= INTERFACE_F_LOOPBACK;

	/*
	 * If the interface is point-to-point, get the destination address.
	 */
	if ((iter->current.flags & INTERFACE_F_POINTTOPOINT) != 0) {
		/*
		 * Ignore the HP/UX warning about "interger overflow during
		 * conversion.  It comes from its own macro definition,
		 * and is really hard to shut up.
		 */
		if (ioctl(iter->socket, SIOCGLIFDSTADDR, (char *)&lifreq)
		    < 0) {
			isc__strerror(errno, strbuf, sizeof(strbuf));
			UNEXPECTED_ERROR(__FILE__, __LINE__,
				isc_msgcat_get(isc_msgcat,
					       ISC_MSGSET_IFITERIOCTL,
					       ISC_MSG_GETDESTADDR,
					       "%s: getting "
					       "destination address: %s"),
					 lifreq.lifr_name, strbuf);
			return (ISC_R_IGNORE);
		}
		get_addr(family, &iter->current.dstaddress,
			 (struct sockaddr *)&lifreq.lifr_dstaddr);
	}

	/*
	 * Get the network mask.
	 */
	memset(&lifreq, 0, sizeof lifreq);
	memcpy(&lifreq, ifrp, sizeof lifreq);
	switch (family) {
	case AF_INET:
		/*
		 * Ignore the HP/UX warning about "interger overflow during
		 * conversion.  It comes from its own macro definition,
		 * and is really hard to shut up.
		 */
		if (ioctl(iter->socket, SIOCGLIFNETMASK, (char *)&lifreq)
		    < 0) {
			isc__strerror(errno, strbuf, sizeof(strbuf));
			UNEXPECTED_ERROR(__FILE__, __LINE__,
				isc_msgcat_get(isc_msgcat,
					       ISC_MSGSET_IFITERIOCTL,
					       ISC_MSG_GETNETMASK,
					       "%s: getting netmask: %s"),
					 lifreq.lifr_name, strbuf);
			return (ISC_R_IGNORE);
		}
		get_addr(family, &iter->current.netmask,
			 (struct sockaddr *)&lifreq.lifr_addr);
		break;
	case AF_INET6: {
#ifdef lifr_addrlen
		int i, bits;

		/*
		 * Netmask already zeroed.
		 */
		iter->current.netmask.family = family;
		for (i = 0 ; i < lifreq.lifr_addrlen; i += 8) {
			bits = lifreq.lifr_addrlen - i;
			bits = (bits < 8 ) ? (8-bits) : 0;
			iter->current.netmask.type.in6.s6_addr[i/8] =
				 (~0 << bits) &0xff;
		}
#endif
		break;
	}
	}

	return (ISC_R_SUCCESS);
}

/*
 * Step the iterator to the next interface.  Unlike
 * isc_interfaceiter_next(), this may leave the iterator
 * positioned on an interface that will ultimately
 * be ignored.  Return ISC_R_NOMORE if there are no more
 * interfaces, otherwise ISC_R_SUCCESS.
 */
static isc_result_t
internal_next(isc_interfaceiter_t *iter) {
	struct lifreq *ifrp;

	REQUIRE (iter->pos < (unsigned int) iter->ifc.lifc_len);

	ifrp = (struct lifreq *)((char *) iter->ifc.lifc_req + iter->pos);

#ifdef ISC_PLATFORM_HAVESALEN
	if (ifrp->lifr_addr.sa_len > sizeof(struct sockaddr))
		iter->pos += sizeof(ifrp->lifr_name) + ifrp->lifr_addr.sa_len;
	else
#endif
		iter->pos += sizeof *ifrp;

	if (iter->pos >= (unsigned int) iter->ifc.lifc_len)
		return (ISC_R_NOMORE);

	return (ISC_R_SUCCESS);
}

static void
internal_destroy(isc_interfaceiter_t *iter) {
	(void) close(iter->socket);
}
