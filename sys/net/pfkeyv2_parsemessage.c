/*
%%% copyright-nrl-97
This software is Copyright 1997-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

%%% copyright-cmetz-97
This software is Copyright 1997-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <net/route.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>

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
#define BITMAP_SUPPORTED               (1 << SADB_EXT_SUPPORTED)
#define BITMAP_SPIRANGE                (1 << SADB_EXT_SPIRANGE)
#define BITMAP_LIFETIME (BITMAP_LIFETIME_CURRENT | BITMAP_LIFETIME_HARD | BITMAP_LIFETIME_SOFT)
#define BITMAP_ADDRESS (BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_ADDRESS_PROXY)
#define BITMAP_KEY      (BITMAP_KEY_AUTH | BITMAP_KEY_ENCRYPT)
#define BITMAP_IDENTITY (BITMAP_IDENTITY_SRC | BITMAP_IDENTITY_DST)
#define BITMAP_MSG                     1
#define BITMAP_X_SRC_MASK              (1 << SADB_EXT_X_SRC_MASK)
#define BITMAP_X_DST_MASK              (1 << SADB_EXT_X_DST_MASK)
#define BITMAP_X_PROTOCOL              (1 << SADB_EXT_X_PROTOCOL)
#define BITMAP_X_SA2                   (1 << SADB_EXT_X_SA2)
#define BITMAP_X_SRC_FLOW              (1 << SADB_EXT_X_SRC_FLOW)
#define BITMAP_X_DST_FLOW              (1 << SADB_EXT_X_DST_FLOW)
#define BITMAP_X_DST2                  (1 << SADB_EXT_X_DST2)

uint32_t sadb_exts_allowed_in[SADB_MAX+1] =
{
  /* RESERVED */
  ~0,
  /* GETSPI */
  BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_SPIRANGE,
  /* UPDATE */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_KEY | BITMAP_IDENTITY,
  /* ADD */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_KEY | BITMAP_IDENTITY,
  /* DELETE */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* GET */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* ACQUIRE */
  BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_IDENTITY | BITMAP_PROPOSAL,
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
  BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_SA | BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_PROTOCOL | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW,
  /* X_DELFLOW */
  BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_PROTOCOL | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW | BITMAP_SA,
  /* X_GRPSPIS */
  BITMAP_SA | BITMAP_X_SA2 | BITMAP_X_DST2 | BITMAP_ADDRESS_DST | BITMAP_X_PROTOCOL
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
  BITMAP_ADDRESS_DST | BITMAP_SA | BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW,
  /* X_DELFLOW */
  BITMAP_SA | BITMAP_X_SRC_MASK | BITMAP_X_DST_MASK | BITMAP_X_SRC_FLOW | BITMAP_X_DST_FLOW,
  /* X_GRPSPIS */
  BITMAP_SA | BITMAP_X_SA2 | BITMAP_X_DST2 | BITMAP_ADDRESS_DST | BITMAP_X_PROTOCOL
};

uint32_t sadb_exts_allowed_out[SADB_MAX+1] =
{
  /* RESERVED */
  ~0,
  /* GETSPI */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* UPDATE */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_IDENTITY,
  /* ADD */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_IDENTITY,
  /* DELETE */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* GET */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_KEY | BITMAP_IDENTITY,
  /* ACQUIRE */
  BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST | BITMAP_IDENTITY | BITMAP_PROPOSAL,
  /* REGISTER */
  BITMAP_SUPPORTED,
  /* EXPIRE */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS,
  /* FLUSH */
  0,
  /* DUMP */
  BITMAP_SA | BITMAP_LIFETIME | BITMAP_ADDRESS | BITMAP_IDENTITY,
  /* X_PROMISC */
  0,
  /* X_ADDFLOW */
  0,
  /* X_DELFLOW */
  0,
  /* X_GRPSPIS */
  0
};

uint32_t sadb_exts_required_out[SADB_MAX+1] =
{
  /* RESERVED */
  0,
  /* GETSPI */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* UPDATE */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* ADD */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* DELETE */
  BITMAP_SA | BITMAP_ADDRESS_DST,
  /* GET */
  BITMAP_SA | BITMAP_LIFETIME_CURRENT | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* ACQUIRE */
  0,
  /* REGISTER */
  BITMAP_SUPPORTED,
  /* EXPIRE */
  BITMAP_SA | BITMAP_ADDRESS_SRC | BITMAP_ADDRESS_DST,
  /* FLUSH */
  0,
  /* DUMP */
  0,
  /* X_PROMISC */
  0,
  /* X_ADDFLOW */
  0,
  /* X_DELFLOW */
  0,
  /* X_GRPSPIS */
  0
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

  if (left < sizeof(struct sadb_msg))
    return EINVAL;

  headers[0] = p;

  if (sadb_msg->sadb_msg_len * sizeof(uint64_t) != left)
    return EINVAL;

  p += sizeof(struct sadb_msg);
  left -= sizeof(struct sadb_msg);

  if (sadb_msg->sadb_msg_reserved)
    return EINVAL;

  if (sadb_msg->sadb_msg_type > SADB_MAX)
    return EINVAL;

  if (!sadb_msg->sadb_msg_type)
    return EINVAL;

  if (sadb_msg->sadb_msg_pid != curproc->p_pid)
    return EINVAL;

  if (sadb_msg->sadb_msg_errno) {
    if (left)
      return EINVAL;

    return 0;
  }

  if (sadb_msg->sadb_msg_type == SADB_X_PROMISC)
    return 0;

  allow = sadb_exts_allowed_in[sadb_msg->sadb_msg_type];

  while (left > 0) {
    sadb_ext = (struct sadb_ext *)p;
    if (left < sizeof(struct sadb_ext))
      return EINVAL;

    i = sadb_ext->sadb_ext_len * sizeof(uint64_t);
    if (left < i)
      return EINVAL;

    if (sadb_ext->sadb_ext_type > SADB_EXT_MAX)
      return EINVAL;

    if (!sadb_ext->sadb_ext_type)
      return EINVAL;

    if (!(allow & (1 << sadb_ext->sadb_ext_type)))
      return EINVAL;

    if (headers[sadb_ext->sadb_ext_type])
      return EINVAL;

    seen |= (1 << sadb_ext->sadb_ext_type);

    switch (sadb_ext->sadb_ext_type) {
      case SADB_EXT_X_SA2:
      case SADB_EXT_SA:
	{
	  struct sadb_sa *sadb_sa = (struct sadb_sa *)p;

	  if (i != sizeof(struct sadb_sa))
	    return EINVAL;

	  if (sadb_sa->sadb_sa_state > SADB_SASTATE_MAX)
	    return EINVAL;

	  if (sadb_sa->sadb_sa_state == SADB_SASTATE_DEAD)
	    return EINVAL;

	  if (sadb_sa->sadb_sa_encrypt > SADB_EALG_MAX)
	    return EINVAL;

	  if (sadb_sa->sadb_sa_auth > SADB_AALG_MAX)
	    return EINVAL;
	}
	break;
      case SADB_EXT_X_PROTOCOL:
	if (i != sizeof(struct sadb_protocol))
	    return EINVAL;
	break;
      case SADB_EXT_LIFETIME_CURRENT:
      case SADB_EXT_LIFETIME_HARD:
      case SADB_EXT_LIFETIME_SOFT:
	{
	  if (i != sizeof(struct sadb_lifetime))
	    return EINVAL;
	}
	break;
      case SADB_EXT_ADDRESS_SRC:
      case SADB_EXT_ADDRESS_DST:
      case SADB_EXT_X_DST2:
      case SADB_EXT_X_SRC_MASK:
      case SADB_EXT_X_DST_MASK:
      case SADB_EXT_X_SRC_FLOW:
      case SADB_EXT_X_DST_FLOW:
      case SADB_EXT_ADDRESS_PROXY:
	{
	  struct sadb_address *sadb_address = (struct sadb_address *)p;
	  struct sockaddr *sa = (struct sockaddr *)(p + sizeof(struct sadb_address));

	  if (i < sizeof(struct sadb_address) + sizeof(struct sockaddr))
	    return EINVAL;

	  if (sadb_address->sadb_address_reserved)
	    return EINVAL;

#if SALEN
	  if (sa->sa_len && (i != sizeof(struct sadb_address) + sa->sa_len))
	    return EINVAL;
#endif /* SALEN */

	  switch(sa->sa_family) {
	    case AF_INET:
	      if (sizeof(struct sadb_address) + sizeof(struct sockaddr_in)
		  != i)
		return EINVAL;
#if SALEN
	      if (sa->sa_len != sizeof(struct sockaddr_in))
		return EINVAL;
#endif /* SALEN */

	      /* Only check the right pieces */
	      switch (sadb_ext->sadb_ext_type)
	      {
		  case SADB_EXT_X_SRC_MASK:
		  case SADB_EXT_X_DST_MASK:
		  case SADB_EXT_X_SRC_FLOW:
		  case SADB_EXT_X_DST_FLOW:
		      break;
		      
		  default:
		      if (((struct sockaddr_in *)sa)->sin_port)
			return EINVAL;
		      break;
	      }

	      {
		char zero[sizeof(((struct sockaddr_in *)sa)->sin_zero)];
		bzero(zero, sizeof(zero));

		if (bcmp(&((struct sockaddr_in *)sa)->sin_zero, zero,
			 sizeof(zero)))
		  return EINVAL;
	      }
	      break;
#if INET6
	    case AF_INET6:
	      if (i != sizeof(struct sadb_address) +
		  sizeof(struct sockaddr_in6))
		return EINVAL;

	      if (sa->sa_len != sizeof(struct sockaddr_in6))
		return EINVAL;

	      if (((struct sockaddr_in6 *)sa)->sin6_port)
		return EINVAL;

	      if (((struct sockaddr_in6 *)sa)->sin6_flowinfo)
		return EINVAL;

	      break;
#endif /* INET6 */
	    default:
	      return EINVAL;
	  }
	}
	break;
      case SADB_EXT_KEY_AUTH:
      case SADB_EXT_KEY_ENCRYPT:
	{
	  struct sadb_key *sadb_key = (struct sadb_key *)p;

	  if (i < sizeof(struct sadb_key))
	    return EINVAL;

	  if (!sadb_key->sadb_key_bits)
	    return EINVAL;

	  if (((sadb_key->sadb_key_bits + 63) / 64) * sizeof(uint64_t) !=
	      i - sizeof(struct sadb_key))
	    return EINVAL;

	  if (sadb_key->sadb_key_reserved)
	    return EINVAL;
	}
	break;
      case SADB_EXT_IDENTITY_SRC:
      case SADB_EXT_IDENTITY_DST:
	{
	  struct sadb_ident *sadb_ident = (struct sadb_ident *)p;

	  if (i < sizeof(struct sadb_ident))
	    return EINVAL;

	  if (sadb_ident->sadb_ident_type > SADB_IDENTTYPE_MAX)
	    return EINVAL;

	  if (sadb_ident->sadb_ident_reserved)
	    return EINVAL;

	  if (i > sizeof(struct sadb_ident)) {
	    char *c = (char *)(p + sizeof(struct sadb_ident));
	    int j;

	    if (*(char *)(p + i - 1))
	      return EINVAL;

	    j = ((strlen(c) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t)-1)) +
		sizeof(struct sadb_ident);

	    if (i != j)
	      return EINVAL;
	  }
	}
	break;
      case SADB_EXT_SENSITIVITY:
	{
	  struct sadb_sens *sadb_sens = (struct sadb_sens *)p;

	  if (i < sizeof(struct sadb_sens))
	    return EINVAL;

	  if (i != (sadb_sens->sadb_sens_sens_len +
		    sadb_sens->sadb_sens_integ_len) * sizeof(uint64_t) +
	      sizeof(struct sadb_sens))
	    return EINVAL;
	}
	break;
      case SADB_EXT_PROPOSAL:
	{
	  struct sadb_prop *sadb_prop = (struct sadb_prop *)p;

	  if (i < sizeof(struct sadb_prop))
	    return EINVAL;

	  if (sadb_prop->sadb_prop_reserved)
	    return EINVAL;

	  if ((i - sizeof(struct sadb_prop)) % sizeof(struct sadb_comb))
	    return EINVAL;

	  {
	    struct sadb_comb *sadb_comb = (struct sadb_comb *)(p + sizeof(struct sadb_prop));
	    int j;

	    for (j = 0;
		 j < (i - sizeof(struct sadb_prop))/sizeof(struct sadb_comb);
		 j++) {
	      if (sadb_comb->sadb_comb_auth > SADB_AALG_MAX)
		return EINVAL;

	      if (sadb_comb->sadb_comb_encrypt > SADB_EALG_MAX)
		return EINVAL;

	      if (sadb_comb->sadb_comb_reserved)
		return EINVAL;

	    }
	  }
	}
        break;
      case SADB_EXT_SUPPORTED:
	{
	  struct sadb_supported *sadb_supported = (struct sadb_supported *)p;
	  int j;

	  if (i < sizeof(struct sadb_supported))
	    return EINVAL;

	  if (sadb_supported->sadb_supported_reserved)
	    return EINVAL;

	  if (i != ((sadb_supported->sadb_supported_nauth +
		     sadb_supported->sadb_supported_nencrypt) *
		    sizeof(struct sadb_alg)) + sizeof(struct sadb_supported))
	    return EINVAL;

	  {
	    struct sadb_alg *sadb_alg = (struct sadb_alg *)(p + sizeof(struct sadb_supported));
	    for (j = 0; j < sadb_supported->sadb_supported_nauth; j++) {
	      if (sadb_alg->sadb_alg_type > SADB_AALG_MAX)
		return EINVAL;

	      if (sadb_alg->sadb_alg_reserved)
		return EINVAL;

	      sadb_alg++;
	    }
	    for (j = 0; j < sadb_supported->sadb_supported_nencrypt; j++) {
	      if (sadb_alg->sadb_alg_type > SADB_EALG_MAX)
		return EINVAL;

	      if (sadb_alg->sadb_alg_reserved)
		return EINVAL;

	      sadb_alg++;
	    }
	  }
	}
	break;
      case SADB_EXT_SPIRANGE:
	{
	  struct sadb_spirange *sadb_spirange = (struct sadb_spirange *)p;

	  if (i != sizeof(struct sadb_spirange))
	    return EINVAL;

	  if (sadb_spirange->sadb_spirange_min >
	      sadb_spirange->sadb_spirange_max)
	    return EINVAL;
	}
	break;
      default:
	  return EINVAL;
    }
  
    headers[sadb_ext->sadb_ext_type] = p;
    p += i;
    left -= i;
  }

  if (left)
    return EINVAL;

  {
    uint32_t required;

    required = sadb_exts_required_in[sadb_msg->sadb_msg_type];

    if ((seen & required) != required)
      return EINVAL;
  }

  switch(((struct sadb_msg *)headers[0])->sadb_msg_type) {
    case SADB_UPDATE:
      if (((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_state !=
	  SADB_SASTATE_MATURE)
	return EINVAL;
      break;
    case SADB_ADD:
      if (((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_state !=
	  SADB_SASTATE_MATURE)
	return EINVAL;
      break;
  }

  return 0;
}
