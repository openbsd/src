/*	$OpenBSD: pfkeyv2_parsemessage.c,v 1.35 2003/02/16 19:54:20 jason Exp $	*/

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 * 
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Craig Metz. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>

extern int encdebug;

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#define BITMAP_SA                      (1 << SADB_EXT_SA)
#define BITMAP_LIFETIME_CURRENT        (1 << SADB_EXT_LIFETIME_CURRENT)
#define BITMAP_LIFETIME_HARD           (1 << SADB_EXT_LIFETIME_HARD)
#define BITMAP_LIFETIME_SOFT           (1 << SADB_EXT_LIFETIME_SOFT)
#define BITMAP_ADDRESS_SRC             (1 << SADB_EXT_ADDRESS_SRC)
#define BITMAP_ADDRESS_DST             (1 << SADB_EXT_ADDRESS_DST)
#define BITMAP_ADDRESS_PROXY           (1 << SADB_EXT_ADDRESS_PROXY)
#define BITMAP_KEY_AUTH                (1 << SADB_EXT_KEY_AUTH)
#define BITMAP_KEY_ENCRYPT             (1 << SADB_EXT_KEY_ENCRYPT)
#define BITMAP_IDENTITY_SRC            (1 << SADB_EXT_IDENTITY_SRC)
#define BITMAP_IDENTITY_DST            (1 << SADB_EXT_IDENTITY_DST)
#define BITMAP_SENSITIVITY             (1 << SADB_EXT_SENSITIVITY)
#define BITMAP_PROPOSAL                (1 << SADB_EXT_PROPOSAL)
#define BITMAP_SUPPORTED_AUTH          (1 << SADB_EXT_SUPPORTED_AUTH)
#define BITMAP_SUPPORTED_ENCRYPT       (1 << SADB_EXT_SUPPORTED_ENCRYPT)
#define BITMAP_SPIRANGE                (1 << SADB_EXT_SPIRANGE)
#define BITMAP_LIFETIME (BITMAP_LIFETIME_CURRENT | BITMAP_LIFETIME_HARD | BITMAP_LIFETIME_SOFT)
#define BITMAP_ADDRESS (BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_ADDRESS_PROXY)
#define BITMAP_KEY      (BITMAP_KEY_AUTH | BITMAP_KEY_ENCRYPT)
#define BITMAP_IDENTITY (BITMAP_IDENTITY_SRC | BITMAP_IDENTITY_DST)
#define BITMAP_MSG                     1
#define BITMAP_X_SRC_MASK              (1 << SADB_X_EXT_SRC_MASK)
#define BITMAP_X_DST_MASK              (1 << SADB_X_EXT_DST_MASK)
#define BITMAP_X_PROTOCOL              (1 << SADB_X_EXT_PROTOCOL)
#define BITMAP_X_SRC_FLOW              (1 << SADB_X_EXT_SRC_FLOW)
#define BITMAP_X_DST_FLOW              (1 << SADB_X_EXT_DST_FLOW)
#define BITMAP_X_FLOW_TYPE             (1 << SADB_X_EXT_FLOW_TYPE)
#define BITMAP_X_SA2                   (1 << SADB_X_EXT_SA2)
#define BITMAP_X_DST2                  (1 << SADB_X_EXT_DST2)
#define BITMAP_X_POLICY                (1 << SADB_X_EXT_POLICY)
#define BITMAP_X_LOCAL_CREDENTIALS     (1 << SADB_X_EXT_LOCAL_CREDENTIALS)
#define BITMAP_X_REMOTE_CREDENTIALS    (1 << SADB_X_EXT_REMOTE_CREDENTIALS)
#define BITMAP_X_LOCAL_AUTH            (1 << SADB_X_EXT_LOCAL_AUTH)
#define BITMAP_X_REMOTE_AUTH           (1 << SADB_X_EXT_REMOTE_AUTH)
#define BITMAP_X_CREDENTIALS           (BITMAP_X_LOCAL_CREDENTIALS | BITMAP_X_REMOTE_CREDENTIALS | BITMAP_X_LOCAL_AUTH | BITMAP_X_REMOTE_AUTH)
#define BITMAP_X_FLOW                  (BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_PROTOCOL | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE)
#define BITMAP_X_SUPPORTED_COMP        (1 << SADB_X_EXT_SUPPORTED_COMP)

uint32_t sadb_exts_allowed_in[SADB_MAX+1] =
{
	/* RESERVED */
	~0,
	/* GETSPI */
	BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_SPIRANGE,
	/* UPDATE */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_KEY | BITMAP_IDENTITY | BITMAP_X_CREDENTIALS | BITMAP_X_FLOW,
	/* ADD */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_KEY | BITMAP_IDENTITY | BITMAP_X_CREDENTIALS | BITMAP_X_FLOW,
	/* DELETE */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* GET */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* ACQUIRE */
	BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_IDENTITY | BITMAP_PROPOSAL | BITMAP_X_CREDENTIALS,
	/* REGISTER */
	0,
	/* EXPIRE */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* FLUSH */
	0,
	/* DUMP */
	0,
	/* X_PROMISC */
	0,
	/* X_ADDFLOW */
	BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_IDENTITY_SRC | BITMAP_IDENTITY_DST | BITMAP_X_FLOW,
	/* X_DELFLOW */
	BITMAP_X_FLOW,
	/* X_GRPSPIS */
	BITMAP_SA | BITMAP_X_SA2 | BITMAP_X_DST2 | BITMAP_ADDRESS_DST | BITMAP_X_PROTOCOL,
	/* X_ASKPOLICY */
	BITMAP_X_POLICY,
};

uint32_t sadb_exts_required_in[SADB_MAX+1] =
{
	/* RESERVED */
	0,
	/* GETSPI */
	BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_SPIRANGE,
	/* UPDATE */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* ADD */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* DELETE */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* GET */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* ACQUIRE */
	0,
	/* REGISTER */
	0,
	/* EXPIRE */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* FLUSH */
	0,
	/* DUMP */
	0,
	/* X_PROMISC */
	0,
	/* X_ADDFLOW */
	BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE,
	/* X_DELFLOW */
	BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE,
	/* X_GRPSPIS */
	BITMAP_SA | BITMAP_X_SA2 | BITMAP_X_DST2 | BITMAP_ADDRESS_DST | BITMAP_X_PROTOCOL,
	/* X_ASKPOLICY */
	BITMAP_X_POLICY,
};

uint32_t sadb_exts_allowed_out[SADB_MAX+1] =
{
	/* RESERVED */
	~0,
	/* GETSPI */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* UPDATE */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_IDENTITY | BITMAP_X_CREDENTIALS | BITMAP_X_FLOW,
	/* ADD */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_IDENTITY | BITMAP_X_CREDENTIALS | BITMAP_X_FLOW,
	/* DELETE */
	BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
	/* GET */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_KEY | BITMAP_IDENTITY,
	/* ACQUIRE */
	BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_IDENTITY | BITMAP_PROPOSAL | BITMAP_X_CREDENTIALS,
	/* REGISTER */
	BITMAP_SUPPORTED_AUTH | BITMAP_SUPPORTED_ENCRYPT | BITMAP_X_SUPPORTED_COMP,
	/* EXPIRE */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS,
	/* FLUSH */
	0,
	/* DUMP */
	BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_IDENTITY,
	/* X_PROMISC */
	0,
	/* X_ADDFLOW */
	BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_PROTOCOL | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE | BITMAP_IDENTITY_SRC | BITMAP_IDENTITY_DST,
	/* X_DELFLOW */
	BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_PROTOCOL | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE,
	/* X_GRPSPIS */
	BITMAP_SA | BITMAP_X_SA2 | BITMAP_X_DST2 | BITMAP_ADDRESS_DST | BITMAP_X_PROTOCOL,
	/* X_ASKPOLICY */
	BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_FLOW_TYPE | BITMAP_X_POLICY,
};

uint32_t sadb_exts_required_out[SADB_MAX+1] =
{
	/* RESERVED */
	0,
	/* GETSPI */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* UPDATE */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* ADD */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* DELETE */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* GET */
	BITMAP_SA | BITMAP_LIFETIME_CURRENT | BITMAP_ADDRESS_DST,
	/* ACQUIRE */
	0,
	/* REGISTER */
	BITMAP_SUPPORTED_AUTH | BITMAP_SUPPORTED_ENCRYPT | BITMAP_X_SUPPORTED_COMP,
	/* EXPIRE */
	BITMAP_SA | BITMAP_ADDRESS_DST,
	/* FLUSH */
	0,
	/* DUMP */
	0,
	/* X_PROMISC */
	0,
	/* X_ADDFLOW */
	BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE,
	/* X_DELFLOW */
	BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_FLOW_TYPE,
	/* X_GRPSPIS */
	BITMAP_SA | BITMAP_X_SA2 | BITMAP_X_DST2 | BITMAP_ADDRESS_DST | BITMAP_X_PROTOCOL,
	/* X_REPPOLICY */
	BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_FLOW_TYPE,
};

int pfkeyv2_parsemessage(void *, int, void **);

#define RETURN_EINVAL(line) goto einval;

int
pfkeyv2_parsemessage(void *p, int len, void **headers)
{
	struct sadb_ext *sadb_ext;
	int i, left = len;
	uint32_t allow, seen = 1;
	struct sadb_msg *sadb_msg = (struct sadb_msg *) p;

	bzero(headers, (SADB_EXT_MAX + 1) * sizeof(void *));

	if (left < sizeof(struct sadb_msg)) {
		DPRINTF(("pfkeyv2_parsemessage: message too short\n"));
		return (EINVAL);
	}

	headers[0] = p;

	if (sadb_msg->sadb_msg_len * sizeof(uint64_t) != left) {
		DPRINTF(("pfkeyv2_parsemessage: length not a multiple of 64\n"));
		return (EINVAL);
	}

	p += sizeof(struct sadb_msg);
	left -= sizeof(struct sadb_msg);

	if (sadb_msg->sadb_msg_reserved) {
		DPRINTF(("pfkeyv2_parsemessage: message header reserved "
		    "field set\n"));
		return (EINVAL);
	}

	if (sadb_msg->sadb_msg_type > SADB_MAX) {
		DPRINTF(("pfkeyv2_parsemessage: message type > %d\n",
		    SADB_MAX));
		return (EINVAL);
	}

	if (!sadb_msg->sadb_msg_type) {
		DPRINTF(("pfkeyv2_parsemessage: message type unset\n"));
		return (EINVAL);
	}

	if (sadb_msg->sadb_msg_pid != curproc->p_pid) {
		DPRINTF(("pfkeyv2_parsemessage: bad PID value\n"));
		return (EINVAL);
	}

	if (sadb_msg->sadb_msg_errno) {
		if (left) {
			DPRINTF(("pfkeyv2_parsemessage: too-large error message\n"));
			return (EINVAL);
		}
		return (0);
	}

	if (sadb_msg->sadb_msg_type == SADB_X_PROMISC) {
		DPRINTF(("pfkeyv2_parsemessage: message type promiscuous\n"));
		return (0);
	}

	allow = sadb_exts_allowed_in[sadb_msg->sadb_msg_type];

	while (left > 0) {
		sadb_ext = (struct sadb_ext *)p;
		if (left < sizeof(struct sadb_ext)) {
			DPRINTF(("pfkeyv2_parsemessage: extension header too "
			    "short\n"));
			return (EINVAL);
		}

		i = sadb_ext->sadb_ext_len * sizeof(uint64_t);
		if (left < i) {
			DPRINTF(("pfkeyv2_parsemessage: extension header "
			    "exceeds message length\n"));
			return (EINVAL);
		}

		if (sadb_ext->sadb_ext_type > SADB_EXT_MAX) {
			DPRINTF(("pfkeyv2_parsemessage: unknown extension "
			    "header %d\n", sadb_ext->sadb_ext_type));
			return (EINVAL);
		}

		if (!sadb_ext->sadb_ext_type) {
			DPRINTF(("pfkeyv2_parsemessage: unset extension "
			    "header\n"));
			return (EINVAL);
		}

		if (!(allow & (1 << sadb_ext->sadb_ext_type))) {
			DPRINTF(("pfkeyv2_parsemessage: extension header %d "
			    "not permitted on message type %d\n",
			    sadb_ext->sadb_ext_type, sadb_msg->sadb_msg_type));
			return (EINVAL);
		}

		if (headers[sadb_ext->sadb_ext_type]) {
			DPRINTF(("pfkeyv2_parsemessage: duplicate extension "
			    "header %d\n", sadb_ext->sadb_ext_type));
			return (EINVAL);
		}

		seen |= (1 << sadb_ext->sadb_ext_type);

		switch (sadb_ext->sadb_ext_type) {
		case SADB_EXT_SA:
		case SADB_X_EXT_SA2:
		{
			struct sadb_sa *sadb_sa = (struct sadb_sa *)p;

			if (i != sizeof(struct sadb_sa)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length for SA extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_sa->sadb_sa_state > SADB_SASTATE_MAX) {
				DPRINTF(("pfkeyv2_parsemessage: unknown SA "
				    "state %d in SA extension header %d\n",
				    sadb_sa->sadb_sa_state,
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_sa->sadb_sa_state == SADB_SASTATE_DEAD) {
				DPRINTF(("pfkeyv2_parsemessage: cannot set SA "
				    "state to dead, SA extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_sa->sadb_sa_encrypt > SADB_EALG_MAX) {
				DPRINTF(("pfkeyv2_parsemessage: unknown "
				    "encryption algorithm %d in SA extension "
				    "header %d\n", sadb_sa->sadb_sa_encrypt,
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_sa->sadb_sa_auth > SADB_AALG_MAX) {
				DPRINTF(("pfkeyv2_parsemessage: unknown "
				    "authentication algorithm %d in SA "
				    "extension header %d\n",
				    sadb_sa->sadb_sa_auth,
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_sa->sadb_sa_replay > 32) {
				DPRINTF(("pfkeyv2_parsemessage: unsupported "
				    "replay window size %d in SA extension "
				    "header %d\n", sadb_sa->sadb_sa_replay,
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
		}
		break;
		case SADB_X_EXT_PROTOCOL:
		case SADB_X_EXT_FLOW_TYPE:
			if (i != sizeof(struct sadb_protocol)) {
				DPRINTF(("pfkeyv2_parsemessage: bad "
				    "PROTOCOL/FLOW header length in extension "
				    "header %d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
			break;
		case SADB_X_EXT_POLICY:
			if (i != sizeof(struct sadb_x_policy)) {
				DPRINTF(("pfkeyv2_parsemessage: bad POLICY "
				    "header length\n"));
				return (EINVAL);
			}
			break;
		case SADB_EXT_LIFETIME_CURRENT:
		case SADB_EXT_LIFETIME_HARD:
		case SADB_EXT_LIFETIME_SOFT:
			if (i != sizeof(struct sadb_lifetime)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length for LIFETIME extension header "
				    "%d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
			break;
		case SADB_EXT_ADDRESS_SRC:
		case SADB_EXT_ADDRESS_DST:
		case SADB_X_EXT_SRC_MASK:
		case SADB_X_EXT_DST_MASK:
		case SADB_X_EXT_SRC_FLOW:
		case SADB_X_EXT_DST_FLOW:
		case SADB_X_EXT_DST2:
		case SADB_EXT_ADDRESS_PROXY:
		{
			struct sadb_address *sadb_address =
			    (struct sadb_address *)p;
			struct sockaddr *sa = (struct sockaddr *)(p +
			    sizeof(struct sadb_address));

			if (i < sizeof(struct sadb_address) +
			    sizeof(struct sockaddr)) {
				DPRINTF(("pfkeyv2_parsemessage: bad ADDRESS "
				    "extension header %d length\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_address->sadb_address_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: ADDRESS "
				    "extension header %d reserved field set\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
			if (sa->sa_len &&
			    (i != sizeof(struct sadb_address) +
			    PADUP(sa->sa_len))) {
				DPRINTF(("pfkeyv2_parsemessage: bad sockaddr "
				    "length field in ADDRESS extension "
				    "header %d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			switch(sa->sa_family) {
			case AF_INET:
				if (sizeof(struct sadb_address) +
				    PADUP(sizeof(struct sockaddr_in)) != i) {
					DPRINTF(("pfkeyv2_parsemessage: "
					    "invalid ADDRESS extension header "
					    "%d length\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}

				if (sa->sa_len != sizeof(struct sockaddr_in)) {
					DPRINTF(("pfkeyv2_parsemessage: bad "
					    "sockaddr_in length in ADDRESS "
					    "extension header %d\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}

				/* Only check the right pieces */
				switch (sadb_ext->sadb_ext_type)
				{
				case SADB_X_EXT_SRC_MASK:
				case SADB_X_EXT_DST_MASK:
				case SADB_X_EXT_SRC_FLOW:
				case SADB_X_EXT_DST_FLOW:
					break;
		      
				default:
					if (((struct sockaddr_in *)sa)->sin_port) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": port field set in "
						    "sockaddr_in of ADDRESS "
						    "extension header %d\n",
						    sadb_ext->sadb_ext_type));
						return (EINVAL);
					}
					break;
				}

				{
					char zero[sizeof(((struct sockaddr_in *)sa)->sin_zero)];
					bzero(zero, sizeof(zero));

					if (bcmp(&((struct sockaddr_in *)sa)->sin_zero, zero, sizeof(zero))) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": reserved sockaddr_in "
						    "field non-zero'ed in "
						    "ADDRESS extension header "
						    "%d\n",
						    sadb_ext->sadb_ext_type));
						return (EINVAL);
					}
				}
				break;
#if INET6
			case AF_INET6:
				if (i != sizeof(struct sadb_address) +
				    PADUP(sizeof(struct sockaddr_in6))) {
					DPRINTF(("pfkeyv2_parsemessage: "
					    "invalid sockaddr_in6 length in "
					    "ADDRESS extension header %d\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}

				if (sa->sa_len !=
				    sizeof(struct sockaddr_in6)) {
					DPRINTF(("pfkeyv2_parsemessage: bad "
					    "sockaddr_in6 length in ADDRESS "
					    "extension header %d\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}

				if (((struct sockaddr_in6 *)sa)->sin6_flowinfo) {
					DPRINTF(("pfkeyv2_parsemessage: "
					    "flowinfo field set in "
					    "sockaddr_in6 of ADDRESS "
					    "extension header %d\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}

				/* Only check the right pieces */
				switch (sadb_ext->sadb_ext_type)
				{
				case SADB_X_EXT_SRC_MASK:
				case SADB_X_EXT_DST_MASK:
				case SADB_X_EXT_SRC_FLOW:
				case SADB_X_EXT_DST_FLOW:
					break;
		      
				default:
					if (((struct sockaddr_in6 *)sa)->sin6_port) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": port field set in "
						    "sockaddr_in6 of ADDRESS "
						    "extension header %d\n",
						    sadb_ext->sadb_ext_type));
						return (EINVAL);
					}
					break;
				}
				break;
#endif /* INET6 */
			default:
				DPRINTF(("pfkeyv2_parsemessage: unknown "
				    "address family %d in ADDRESS extension "
				    "header %d\n",
				    sa->sa_family, sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
		}
		break;
		case SADB_EXT_KEY_AUTH:
		case SADB_EXT_KEY_ENCRYPT:
		{
			struct sadb_key *sadb_key = (struct sadb_key *)p;

			if (i < sizeof(struct sadb_key)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length in KEY extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (!sadb_key->sadb_key_bits) {
				DPRINTF(("pfkeyv2_parsemessage: key length "
				    "unset in KEY extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (((sadb_key->sadb_key_bits + 63) / 64) * sizeof(uint64_t) != i - sizeof(struct sadb_key)) {
				DPRINTF(("pfkeyv2_parsemessage: invalid key "
				    "length in KEY extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_key->sadb_key_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: reserved field"
				    " set in KEY extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
		}
		break;
		case SADB_X_EXT_LOCAL_AUTH:
		case SADB_X_EXT_REMOTE_AUTH:
		{
			struct sadb_x_cred *sadb_cred =
			    (struct sadb_x_cred *)p;

			if (i < sizeof(struct sadb_x_cred)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length for AUTH extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_cred->sadb_x_cred_type > SADB_X_AUTHTYPE_MAX) {
				DPRINTF(("pfkeyv2_parsemessage: unknown auth "
				    "type %d in AUTH extension header %d\n",
				    sadb_cred->sadb_x_cred_type, 
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_cred->sadb_x_cred_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: reserved field"
				    " set in AUTH extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
		}
		break;
		case SADB_X_EXT_LOCAL_CREDENTIALS:
		case SADB_X_EXT_REMOTE_CREDENTIALS:
		{
			struct sadb_x_cred *sadb_cred =
			    (struct sadb_x_cred *)p;

			if (i < sizeof(struct sadb_x_cred)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length of CREDENTIALS extension header "
				    "%d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_cred->sadb_x_cred_type > SADB_X_CREDTYPE_MAX) {
				DPRINTF(("pfkeyv2_parsemessage: unknown "
				    "credential type %d in CREDENTIALS "
				    "extension header %d\n",
				    sadb_cred->sadb_x_cred_type,
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_cred->sadb_x_cred_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: reserved "
				    "field set in CREDENTIALS extension "
				    "header %d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}
		}
		break;
		case SADB_EXT_IDENTITY_SRC:
		case SADB_EXT_IDENTITY_DST:
		{
			struct sadb_ident *sadb_ident = (struct sadb_ident *)p;

			if (i < sizeof(struct sadb_ident)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length of IDENTITY extension header %d\n",
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_ident->sadb_ident_type > SADB_IDENTTYPE_MAX) {
				DPRINTF(("pfkeyv2_parsemessage: unknown "
				    "identity type %d in IDENTITY extension "
				    "header %d\n",
				    sadb_ident->sadb_ident_type,
				    sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_ident->sadb_ident_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: reserved "
				    "field set in IDENTITY extension header "
				    "%d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (i > sizeof(struct sadb_ident)) {
				char *c =
				    (char *)(p + sizeof(struct sadb_ident));
				int j;

				if (*(char *)(p + i - 1)) {
					DPRINTF(("pfkeyv2_parsemessage: non "
					    "NUL-terminated identity in "
					    "IDENTITY extension header %d\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}

				j = PADUP(strlen(c) + 1) +
				    sizeof(struct sadb_ident);

				if (i != j) {
					DPRINTF(("pfkeyv2_parsemessage: actual"
					    " identity length does not match "
					    "expected length in identity "
					    "extension header %d\n",
					    sadb_ext->sadb_ext_type));
					return (EINVAL);
				}
			}
		}
		break;
		case SADB_EXT_SENSITIVITY:
		{
			struct sadb_sens *sadb_sens = (struct sadb_sens *)p;

			if (i < sizeof(struct sadb_sens)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length for SENSITIVITY extension "
				    "header\n"));
				return (EINVAL);
			}

			if (i != (sadb_sens->sadb_sens_sens_len +
			    sadb_sens->sadb_sens_integ_len) *
			    sizeof(uint64_t) +
			    sizeof(struct sadb_sens)) {
				DPRINTF(("pfkeyv2_parsemessage: bad payload "
				    "length for SENSITIVITY extension "
				    "header\n"));
				return (EINVAL);
			}
		}
		break;
		case SADB_EXT_PROPOSAL:
		{
			struct sadb_prop *sadb_prop = (struct sadb_prop *)p;

			if (i < sizeof(struct sadb_prop)) {
				DPRINTF(("pfkeyv2_parsemessage: bad PROPOSAL "
				    "header length\n"));
				return (EINVAL);
			}

			if (sadb_prop->sadb_prop_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: reserved field"
				    "set in PROPOSAL extension header\n"));
				return (EINVAL);
			}

			if ((i - sizeof(struct sadb_prop)) %
			    sizeof(struct sadb_comb)) {
				DPRINTF(("pfkeyv2_parsemessage: bad proposal "
				    "length\n"));
				return (EINVAL);
			}

			{
				struct sadb_comb *sadb_comb =
				    (struct sadb_comb *)(p +
					sizeof(struct sadb_prop));
				int j;

				for (j = 0;
				     j < (i - sizeof(struct sadb_prop))/sizeof(struct sadb_comb);
				     j++) {
					if (sadb_comb->sadb_comb_auth >
					    SADB_AALG_MAX) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": unknown authentication "
						    "algorithm %d in "
						    "PROPOSAL\n",
						    sadb_comb->sadb_comb_auth));
						return (EINVAL);
					}

					if (sadb_comb->sadb_comb_encrypt >
					    SADB_EALG_MAX) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": unknown encryption "
						    "algorithm %d in "
						    "PROPOSAL\n",
						    sadb_comb->sadb_comb_encrypt));
						return (EINVAL);
					}

					if (sadb_comb->sadb_comb_reserved) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": reserved field set in "
						    "COMB header\n"));
						return (EINVAL);
					}
				}
			}
		}
		break;
		case SADB_EXT_SUPPORTED_AUTH:
		case SADB_EXT_SUPPORTED_ENCRYPT:
		case SADB_X_EXT_SUPPORTED_COMP:
		{
			struct sadb_supported *sadb_supported =
			    (struct sadb_supported *)p;
			int j;

			if (i < sizeof(struct sadb_supported)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length for SUPPORTED extension header "
				    "%d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			if (sadb_supported->sadb_supported_reserved) {
				DPRINTF(("pfkeyv2_parsemessage: reserved "
				    "field set in SUPPORTED extension "
				    "header %d\n", sadb_ext->sadb_ext_type));
				return (EINVAL);
			}

			{
				struct sadb_alg *sadb_alg =
				    (struct sadb_alg *)(p +
					sizeof(struct sadb_supported));
				int max_alg;

				max_alg = sadb_ext->sadb_ext_type == SADB_EXT_SUPPORTED_AUTH ?
				    SADB_AALG_MAX : SADB_EXT_SUPPORTED_ENCRYPT ?
					SADB_EALG_MAX : SADB_X_CALG_MAX;

				for (j = 0; j < sadb_supported->sadb_supported_len - 1; j++) {
					if (sadb_alg->sadb_alg_id > max_alg) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": unknown algorithm %d "
						    "in SUPPORTED extension "
						    "header %d\n",
						    sadb_alg->sadb_alg_id,
						    sadb_ext->sadb_ext_type));
						return (EINVAL);
					}

					if (sadb_alg->sadb_alg_reserved) {
						DPRINTF(("pfkeyv2_parsemessage"
						    ": reserved field set in "
						    "supported algorithms "
						    "header inside SUPPORTED "
						    "extension header %d\n",
						    sadb_ext->sadb_ext_type));
						return (EINVAL);
					}

					sadb_alg++;
				}
			}
		}
		break;
		case SADB_EXT_SPIRANGE:
		{
			struct sadb_spirange *sadb_spirange =
			    (struct sadb_spirange *)p;

			if (i != sizeof(struct sadb_spirange)) {
				DPRINTF(("pfkeyv2_parsemessage: bad header "
				    "length of SPIRANGE extension header\n"));
				return (EINVAL);
			}

			if (sadb_spirange->sadb_spirange_min >
			    sadb_spirange->sadb_spirange_max) {
				DPRINTF(("pfkeyv2_parsemessage: bad SPI "
				    "range\n"));
				return (EINVAL);
			}
		}
		break;
		default:
			DPRINTF(("pfkeyv2_parsemessage: unknown extension "
			    "header type %d\n",
			    sadb_ext->sadb_ext_type));
			return (EINVAL);
		}
  
		headers[sadb_ext->sadb_ext_type] = p;
		p += i;
		left -= i;
	}

	if (left) {
		DPRINTF(("pfkeyv2_parsemessage: message too long\n"));
		return (EINVAL);
	}

	{
		uint32_t required;

		required = sadb_exts_required_in[sadb_msg->sadb_msg_type];

		if ((seen & required) != required) {
			DPRINTF(("pfkeyv2_parsemessage: required fields "
			    "missing\n"));
			return (EINVAL);
		}
	}

	switch(((struct sadb_msg *)headers[0])->sadb_msg_type) {
	case SADB_UPDATE:
		if (((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_state !=
		    SADB_SASTATE_MATURE) {
			DPRINTF(("pfkeyv2_parsemessage: updating non-mature "
			    "SA prohibited\n"));
			return (EINVAL);
		}
		break;
	case SADB_ADD:
		if (((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_state !=
		    SADB_SASTATE_MATURE) {
			DPRINTF(("pfkeyv2_parsemessage: adding non-mature "
			    "SA prohibited\n"));
			return (EINVAL);
		}
		break;
	}

	return (0);
}
