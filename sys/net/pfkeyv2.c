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
#include <netinet/ip_ipsp.h>

#define PFKEYV2_PROTOCOL 2
#define GETSPI_TRIES 10

struct pfkey_version {
  int protocol;
  int (*create)(struct socket *socket);
  int (*release)(struct socket *socket);
  int (*send)(struct socket *socket, void *message, int len);
};

static struct pfkey_version pfkeyv2_version;

#define PFKEYV2_SOCKETFLAGS_REGISTERED 1
#define PFKEYV2_SOCKETFLAGS_PROMISC    2

#define PFKEYV2_SENDMESSAGE_UNICAST    1
#define PFKEYV2_SENDMESSAGE_REGISTERED 2
#define PFKEYV2_SENDMESSAGE_BROADCAST  3

struct pfkeyv2_socket {
    struct pfkeyv2_socket *next;
    struct socket *socket;
    int flags;
    uint32_t pid;
    uint32_t registration;    /* Increase size if SATYPE_MAX > 31 */
};

static struct pfkeyv2_socket *pfkeyv2_sockets = NULL;

static uint32_t pfkeyv2_seq = 1;

static int nregistered = 0;
static int npromisc = 0;

static struct sadb_alg ealgs[] = {
    { SADB_EALG_DESCBC, 64, 64, 64 },
    { SADB_EALG_3DESCBC, 64, 192, 192 },
    { SADB_EALG_X_BLF, 64, 5, BLF_MAXKEYLEN},
    { SADB_EALG_X_CAST, 64, 5, 16},
    { SADB_EALG_X_SKIPJACK, 64, 10, 10},
};

static struct sadb_alg aalgs[] = {
{ SADB_AALG_SHA1HMAC96, 0, 160, 160 },
{ SADB_AALG_MD5HMAC96, 0, 128, 128 },
{ SADB_AALG_X_RIPEMD160HMAC96, 0, 160, 160 }
};

extern int pfkey_register(struct pfkey_version *version);
int pfkey_unregister(struct pfkey_version *version);
int pfkey_sendup(struct socket *socket, struct mbuf *packet, int more);

void export_address(void **, struct sockaddr *);
void export_identity(void **, struct tdb *, int);
void export_lifetime(void **, struct tdb *, int);
void export_sa(void **, struct tdb *);
void import_address(struct sockaddr *, struct sadb_address *);
void import_identity(struct tdb *, struct sadb_ident *, int);
void import_key(struct ipsecinit *, struct sadb_key *, int);
void import_lifetime(struct tdb *, struct sadb_lifetime *, int);
void import_sa(struct tdb *, struct sadb_sa *, struct ipsecinit *);
int pfdatatopacket(void *, int, struct mbuf **);
int pfkeyv2_acquire(void *);
int pfkeyv2_create(struct socket *);
int pfkeyv2_get(struct tdb *, void **, void **);
int pfkeyv2_release(struct socket *);
int pfkeyv2_send(struct socket *, void *, int);
int pfkeyv2_sendmessage(void **, int, struct socket *, u_int8_t, int);

#define EXTLEN(x) (((struct sadb_ext *)(x))->sadb_ext_len * sizeof(uint64_t))
#define PADUP(x) (((x) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1))

int
pfdatatopacket(void *data, int len, struct mbuf **packet)
{
  if (!(*packet = m_devget(data, len, 0, NULL, NULL)))
    return ENOMEM;

  return 0;
}

int
pfkeyv2_create(struct socket *socket)
{
  struct pfkeyv2_socket *pfkeyv2_socket;

  if (!(pfkeyv2_socket = malloc(sizeof(struct pfkeyv2_socket), M_TEMP,
				M_DONTWAIT)))
    return ENOMEM;

  bzero(pfkeyv2_socket, sizeof(struct pfkeyv2_socket));
  pfkeyv2_socket->next = pfkeyv2_sockets;
  pfkeyv2_socket->socket = socket;
  pfkeyv2_socket->pid = curproc->p_pid;

  pfkeyv2_sockets = pfkeyv2_socket;

  return 0;
}

int
pfkeyv2_release(struct socket *socket)
{
  struct pfkeyv2_socket **pp;

  for (pp = &pfkeyv2_sockets;
       *pp && ((*pp)->socket != socket);
       pp = &((*pp)->next))
    ;

  if (*pp) {
    struct pfkeyv2_socket *pfkeyv2_socket;

    pfkeyv2_socket = *pp;
    *pp = (*pp)->next;

    if (pfkeyv2_socket->flags & PFKEYV2_SOCKETFLAGS_REGISTERED)
      nregistered--;

    if (pfkeyv2_socket->flags & PFKEYV2_SOCKETFLAGS_PROMISC)
      npromisc--;

    free(pfkeyv2_socket, M_TEMP);
  }

  return 0;
}

void
import_sa(struct tdb *tdb, struct sadb_sa *sadb_sa, struct ipsecinit *ii)
{
  if (!sadb_sa)
    return;

  if (ii)
  {
      ii->ii_encalg = sadb_sa->sadb_sa_encrypt;
      ii->ii_authalg = sadb_sa->sadb_sa_auth;

      tdb->tdb_spi = sadb_sa->sadb_sa_spi;
      tdb->tdb_wnd = sadb_sa->sadb_sa_replay;

      if (sadb_sa->sadb_sa_flags & SADB_SAFLAGS_PFS)
	tdb->tdb_flags |= TDBF_PFS;

      if (sadb_sa->sadb_sa_flags & SADB_SAFLAGS_X_HALFIV)
	tdb->tdb_flags |= TDBF_HALFIV;

      if (sadb_sa->sadb_sa_flags & SADB_SAFLAGS_X_TUNNEL)
	tdb->tdb_flags |= TDBF_TUNNELING;
  }

  if (sadb_sa->sadb_sa_state != SADB_SASTATE_MATURE)
    tdb->tdb_flags |= TDBF_INVALID;
}

void
export_sa(void **p, struct tdb *tdb)
{
  struct sadb_sa *sadb_sa = (struct sadb_sa *)*p;

  sadb_sa->sadb_sa_len = sizeof(struct sadb_sa) / sizeof(uint64_t);

  sadb_sa->sadb_sa_spi = tdb->tdb_spi;
  sadb_sa->sadb_sa_replay = tdb->tdb_wnd;
  
  if (tdb->tdb_flags & TDBF_INVALID)
    sadb_sa->sadb_sa_state = SADB_SASTATE_LARVAL;

  if (tdb->tdb_authalgxform)
    sadb_sa->sadb_sa_auth = tdb->tdb_authalgxform->type;

  if (tdb->tdb_encalgxform)
    sadb_sa->sadb_sa_encrypt = tdb->tdb_encalgxform->type;

  if (tdb->tdb_flags & TDBF_PFS)
    sadb_sa->sadb_sa_flags |= SADB_SAFLAGS_PFS;

  if (tdb->tdb_flags & TDBF_HALFIV)
    sadb_sa->sadb_sa_flags |= SADB_SAFLAGS_X_HALFIV;
  
  if (tdb->tdb_flags & TDBF_TUNNELING)
    sadb_sa->sadb_sa_flags |= SADB_SAFLAGS_X_TUNNEL;

  *p += sizeof(struct sadb_sa);
}

void
import_lifetime(struct tdb *tdb, struct sadb_lifetime *sadb_lifetime, int type)
{
  struct expiration *exp;
    
  if (!sadb_lifetime)
    return;

  switch (type) {
    case 0:
      if ((tdb->tdb_exp_allocations =
	   sadb_lifetime->sadb_lifetime_allocations) != 0)
	tdb->tdb_flags |= TDBF_ALLOCATIONS;
      else
	tdb->tdb_flags &= ~TDBF_ALLOCATIONS;
      
      if ((tdb->tdb_exp_bytes = sadb_lifetime->sadb_lifetime_bytes) != 0)
	tdb->tdb_flags |= TDBF_BYTES;
      else
	tdb->tdb_flags &= ~TDBF_BYTES;
      
      if ((tdb->tdb_exp_timeout = sadb_lifetime->sadb_lifetime_addtime) != 0) {
	  tdb->tdb_flags |= TDBF_TIMER;
	  tdb->tdb_exp_timeout += time.tv_sec;
	  exp = get_expiration();
	  bcopy(&tdb->tdb_dst, &exp->exp_dst, SA_LEN(&tdb->tdb_dst.sa));
	  exp->exp_spi = tdb->tdb_spi;
	  exp->exp_sproto = tdb->tdb_sproto;
	  exp->exp_timeout = tdb->tdb_exp_timeout;
	  put_expiration(exp);
      }
      else
	tdb->tdb_flags &= ~TDBF_TIMER;
      
      if ((tdb->tdb_exp_first_use = sadb_lifetime->sadb_lifetime_usetime) != 0)
	tdb->tdb_flags |= TDBF_FIRSTUSE;
      else
	tdb->tdb_flags &= ~TDBF_FIRSTUSE;
      break;
      
    case 1:
      if ((tdb->tdb_soft_allocations =
	   sadb_lifetime->sadb_lifetime_allocations) != 0)
	tdb->tdb_flags |= TDBF_SOFT_ALLOCATIONS;
      else
	tdb->tdb_flags &= ~TDBF_SOFT_ALLOCATIONS;
      
      if ((tdb->tdb_soft_bytes = sadb_lifetime->sadb_lifetime_bytes) != 0)
	tdb->tdb_flags |= TDBF_SOFT_BYTES;
      else
	tdb->tdb_flags &= ~TDBF_SOFT_BYTES;
      
      if ((tdb->tdb_soft_timeout =
	   sadb_lifetime->sadb_lifetime_addtime) != 0) {
	  tdb->tdb_flags |= TDBF_SOFT_TIMER;
	  tdb->tdb_soft_timeout += time.tv_sec;
	  exp = get_expiration();
	  bcopy(&tdb->tdb_dst, &exp->exp_dst, SA_LEN(&tdb->tdb_dst.sa));
	  exp->exp_spi = tdb->tdb_spi;
	  exp->exp_sproto = tdb->tdb_sproto;
	  exp->exp_timeout = tdb->tdb_soft_timeout;
	  put_expiration(exp);
      }
      else
	tdb->tdb_flags &= ~TDBF_SOFT_TIMER;
      
      if ((tdb->tdb_soft_first_use =
	   sadb_lifetime->sadb_lifetime_usetime) != 0)
	tdb->tdb_flags |= TDBF_SOFT_FIRSTUSE;
      else
	tdb->tdb_flags &= ~TDBF_SOFT_FIRSTUSE;
      break;
      
    case 2:  /* Nothing fancy here */
	tdb->tdb_cur_allocations = sadb_lifetime->sadb_lifetime_allocations;
	tdb->tdb_cur_bytes = sadb_lifetime->sadb_lifetime_bytes;
	tdb->tdb_established = sadb_lifetime->sadb_lifetime_addtime;
	tdb->tdb_first_use = sadb_lifetime->sadb_lifetime_usetime;
  }
}

void
export_lifetime(void **p, struct tdb *tdb, int type)
{
  struct sadb_lifetime *sadb_lifetime = (struct sadb_lifetime *)*p;

  sadb_lifetime->sadb_lifetime_len = sizeof(struct sadb_lifetime) /
				     sizeof(uint64_t);

  switch (type) {
    case 0:
      if (tdb->tdb_flags & TDBF_ALLOCATIONS)
	sadb_lifetime->sadb_lifetime_allocations = tdb->tdb_exp_allocations;

      if (tdb->tdb_flags & TDBF_BYTES)
	sadb_lifetime->sadb_lifetime_bytes = tdb->tdb_exp_bytes;

      if (tdb->tdb_flags & TDBF_TIMER)
	sadb_lifetime->sadb_lifetime_addtime = tdb->tdb_exp_timeout -
					       tdb->tdb_established;

      if (tdb->tdb_flags & TDBF_FIRSTUSE)
	sadb_lifetime->sadb_lifetime_usetime = tdb->tdb_exp_first_use -
					       tdb->tdb_first_use;
      break;

    case 1:
      if (tdb->tdb_flags & TDBF_SOFT_ALLOCATIONS)
	sadb_lifetime->sadb_lifetime_allocations = tdb->tdb_soft_allocations;

      if (tdb->tdb_flags & TDBF_SOFT_BYTES)
	sadb_lifetime->sadb_lifetime_bytes = tdb->tdb_soft_bytes;

      if (tdb->tdb_flags & TDBF_SOFT_TIMER)
	sadb_lifetime->sadb_lifetime_addtime = tdb->tdb_soft_timeout -
					       tdb->tdb_established;

      if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
	sadb_lifetime->sadb_lifetime_usetime = tdb->tdb_soft_first_use -
					       tdb->tdb_first_use;
      break;
      
    case 2:
      sadb_lifetime->sadb_lifetime_allocations = tdb->tdb_cur_allocations;
      sadb_lifetime->sadb_lifetime_bytes = tdb->tdb_cur_bytes;
      sadb_lifetime->sadb_lifetime_addtime = tdb->tdb_established;
      sadb_lifetime->sadb_lifetime_usetime = tdb->tdb_first_use;
      break;
  }

  *p += sizeof(struct sadb_lifetime);
}

void
import_address(struct sockaddr *sa, struct sadb_address *sadb_address)
{
  int salen;
  struct sockaddr *ssa = (struct sockaddr *)((void *) sadb_address +
					     sizeof(struct sadb_address));

  if (!sadb_address)
    return;

  if (ssa->sa_len)
    salen = ssa->sa_len;
  else
    switch(ssa->sa_family) {
      case AF_INET:
        salen = sizeof(struct sockaddr_in);
	break;
#if INET6
      case AF_INET6:
        salen = sizeof(struct sockaddr_in6);
	break;
#endif /* INET6 */
      default:
	return;
    }

  bcopy(ssa, sa, salen);
  sa->sa_len = salen;
}

void
export_address(void **p, struct sockaddr *sa)
{
  struct sadb_address *sadb_address = (struct sadb_address *)*p;

  sadb_address->sadb_address_len = (sizeof(struct sadb_address) +
				    PADUP(SA_LEN(sa))) / sizeof(uint64_t);

  *p += sizeof(struct sadb_address);
  bcopy(sa, *p, SA_LEN(sa));
  ((struct sockaddr *)*p)->sa_family = sa->sa_family;
  *p += PADUP(SA_LEN(sa));
}

void
import_identity(struct tdb *tdb, struct sadb_ident *sadb_ident, int type)
{
  if (!sadb_ident)
    return;

  if (type == 0) {
      tdb->tdb_srcid_len = EXTLEN(sadb_ident) -
				   sizeof(struct sadb_ident);
      tdb->tdb_srcid_type = sadb_ident->sadb_ident_type;
      MALLOC(tdb->tdb_srcid, u_int8_t *, tdb->tdb_srcid_len, M_XDATA,
	     M_WAITOK);
      bcopy((void *)sadb_ident + sizeof(struct sadb_ident),
	    tdb->tdb_srcid, tdb->tdb_srcid_len);
  } else {
      tdb->tdb_dstid_len = EXTLEN(sadb_ident) -
				   sizeof(struct sadb_ident);
      tdb->tdb_dstid_type = sadb_ident->sadb_ident_type;
      MALLOC(tdb->tdb_dstid, u_int8_t *, tdb->tdb_dstid_len, M_XDATA,
	     M_WAITOK);
      bcopy((void *)sadb_ident + sizeof(struct sadb_ident),
	    tdb->tdb_dstid, tdb->tdb_dstid_len);
  }
}

void
export_identity(void **p, struct tdb *tdb, int type)
{
  struct sadb_ident *sadb_ident = (struct sadb_ident *)*p;

  if (type == 0) {
      sadb_ident->sadb_ident_len = (sizeof(struct sadb_ident) + PADUP(tdb->tdb_srcid_len)) / sizeof(uint64_t);
      sadb_ident->sadb_ident_type = tdb->tdb_srcid_type;
      *p += sizeof(struct sadb_ident);
      bcopy(tdb->tdb_srcid, *p, tdb->tdb_srcid_len);
      *p += PADUP(tdb->tdb_srcid_len);
  } else {
      sadb_ident->sadb_ident_len = (sizeof(struct sadb_ident) + PADUP(tdb->tdb_dstid_len)) / sizeof(uint64_t);
      sadb_ident->sadb_ident_type = tdb->tdb_dstid_type;
      *p += sizeof(struct sadb_ident);
      bcopy(tdb->tdb_dstid, *p, tdb->tdb_dstid_len);
      *p += PADUP(tdb->tdb_dstid_len);
  }
}

void
import_key(struct ipsecinit *ii, struct sadb_key *sadb_key, int type)
{
  if (!sadb_key)
    return;
        
  if (type == 0) { /* Encryption key */
      ii->ii_enckeylen = sadb_key->sadb_key_bits / 8;
      ii->ii_enckey = (void *)sadb_key + sizeof(struct sadb_key);
  } else {
      ii->ii_authkeylen = sadb_key->sadb_key_bits / 8;
      ii->ii_authkey = (void *)sadb_key + sizeof(struct sadb_key);
  }
}

int
pfkeyv2_sendmessage(void **headers, int mode, struct socket *socket,
		    u_int8_t satype, int count)
{
  int i, j, rval;
  void *p, *buffer = NULL;
  struct mbuf *packet;
  struct pfkeyv2_socket *s;

  j = sizeof(struct sadb_msg);

  for (i = 1; i <= SADB_EXT_MAX; i++)
    if (headers[i])
      j += ((struct sadb_ext *)headers[i])->sadb_ext_len * sizeof(uint64_t);

  if (!(buffer = malloc(j + sizeof(struct sadb_msg), M_TEMP, M_DONTWAIT))) {
    rval = ENOMEM;
    goto ret;
  }

  p = buffer + sizeof(struct sadb_msg);
  bcopy(headers[0], p, sizeof(struct sadb_msg));
  ((struct sadb_msg *)p)->sadb_msg_len = j / sizeof(uint64_t);
  p += sizeof(struct sadb_msg);

  for (i = 1; i <= SADB_EXT_MAX; i++)
    if (headers[i]) {
      ((struct sadb_ext *)headers[i])->sadb_ext_type = i;
      bcopy(headers[i], p, EXTLEN(headers[i]));
      p += EXTLEN(headers[i]);
    }

  if ((rval = pfdatatopacket(buffer + sizeof(struct sadb_msg),
			     j, &packet)) != 0)
    goto ret;

  switch(mode) {
    case PFKEYV2_SENDMESSAGE_UNICAST:
      pfkey_sendup(socket, packet, 0);
    
      bzero(buffer, sizeof(struct sadb_msg));
      ((struct sadb_msg *)buffer)->sadb_msg_version = PF_KEY_V2;
      ((struct sadb_msg *)buffer)->sadb_msg_type = SADB_X_PROMISC;
      ((struct sadb_msg *)buffer)->sadb_msg_len =
			     (sizeof(struct sadb_msg) + j) / sizeof(uint64_t);
      ((struct sadb_msg *)buffer)->sadb_msg_seq = 0;

      if ((rval = pfdatatopacket(buffer, sizeof(struct sadb_msg) + j,
				 &packet)) != 0)
	goto ret;

      for (s = pfkeyv2_sockets; s; s = s->next)
	if ((s->flags & PFKEYV2_SOCKETFLAGS_PROMISC) && (s->socket != socket))
	  pfkey_sendup(s->socket, packet, 1);

      m_zero(packet);
      m_freem(packet);
      break;

    case PFKEYV2_SENDMESSAGE_REGISTERED:
      for (s = pfkeyv2_sockets; s; s = s->next)
	if (s->flags & PFKEYV2_SOCKETFLAGS_REGISTERED) {
	    if (!satype)    /* Just send to everyone registered */
	      pfkey_sendup(s->socket, packet, 1);
	    else {
		if ((1 << satype) & s->registration) /* specified SATYPE */
		  if (count-- == 0) {     /* Done */
		      pfkey_sendup(s->socket, packet, 1);
		      break;
		  }
	    }
	}
      
      m_freem(packet);

      bzero(buffer, sizeof(struct sadb_msg));
      ((struct sadb_msg *)buffer)->sadb_msg_version = PF_KEY_V2;
      ((struct sadb_msg *)buffer)->sadb_msg_type = SADB_X_PROMISC;
      ((struct sadb_msg *)buffer)->sadb_msg_len =
			     (sizeof(struct sadb_msg) + j) / sizeof(uint64_t);
      ((struct sadb_msg *)buffer)->sadb_msg_seq = 0;

      if ((rval = pfdatatopacket(buffer, sizeof(struct sadb_msg) + j,
				 &packet)) != 0)
	goto ret;

      for (s = pfkeyv2_sockets; s; s = s->next)
	if ((s->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
	    (s->flags & PFKEYV2_SOCKETFLAGS_REGISTERED))
	  pfkey_sendup(s->socket, packet, 1);

      m_freem(packet);
      break;

    case PFKEYV2_SENDMESSAGE_BROADCAST:
      for (s = pfkeyv2_sockets; s; s = s->next)
	pfkey_sendup(s->socket, packet, 1);

      m_freem(packet);
      break;
  }

ret:
  bzero(buffer, j + sizeof(struct sadb_msg));
  free(buffer, M_TEMP);
  return rval;
}

extern uint32_t sadb_exts_allowed_out[SADB_MAX+1];
extern uint32_t sadb_exts_required_out[SADB_MAX+1];

int
pfkeyv2_get(struct tdb *sa, void **headers, void **buffer)
{
  int rval, i;
  void *p;

  i = sizeof(struct sadb_sa) + sizeof(struct sadb_lifetime);

  if (sa->tdb_soft_allocations || sa->tdb_soft_bytes ||
      sa->tdb_soft_timeout || sa->tdb_soft_first_use)
    i += sizeof(struct sadb_lifetime);

  if (sa->tdb_exp_allocations || sa->tdb_exp_bytes ||
      sa->tdb_exp_timeout || sa->tdb_exp_first_use)
    i += sizeof(struct sadb_lifetime);

  if (sa->tdb_src.sa.sa_family)
    i += sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_src.sa));

  if (sa->tdb_dst.sa.sa_family)
    i += sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_dst.sa));

  if (sa->tdb_proxy.sa.sa_family)
    i += sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_proxy.sa));

  if (sa->tdb_srcid_len)
    i += PADUP(sa->tdb_srcid_len) + sizeof(struct sadb_ident);

  if (sa->tdb_dstid_len)
    i += PADUP(sa->tdb_dstid_len) + sizeof(struct sadb_ident);

  if (!(p = malloc(i, M_TEMP, M_DONTWAIT))) {
    rval = ENOMEM;
    goto ret;
  }

  *buffer = p;

  bzero(p, i);
  
  headers[SADB_EXT_SA] = p;
  export_sa(&p, sa);

  headers[SADB_EXT_LIFETIME_CURRENT] = p;
  export_lifetime(&p, sa, 2);

  if (sa->tdb_soft_allocations || sa->tdb_soft_bytes ||
      sa->tdb_soft_first_use || sa->tdb_soft_timeout) {
    headers[SADB_EXT_LIFETIME_SOFT] = p;
    export_lifetime(&p, sa, 1);
  }

  if (sa->tdb_exp_allocations || sa->tdb_exp_bytes ||
      sa->tdb_exp_first_use || sa->tdb_exp_timeout) {
    headers[SADB_EXT_LIFETIME_HARD] = p;
    export_lifetime(&p, sa, 0);
  }

  headers[SADB_EXT_ADDRESS_SRC] = p;
  export_address(&p, (struct sockaddr *)&sa->tdb_src);

  headers[SADB_EXT_ADDRESS_DST] = p;
  export_address(&p, (struct sockaddr *)&sa->tdb_dst);

  if (SA_LEN(&sa->tdb_proxy.sa)) {
    headers[SADB_EXT_ADDRESS_PROXY] = p;
    export_address(&p, (struct sockaddr *)&sa->tdb_proxy);
  }

  if (sa->tdb_srcid_len) {
    headers[SADB_EXT_IDENTITY_SRC] = p;
    export_identity(&p, sa, 0);
  }

  if (sa->tdb_dstid_len) {
    headers[SADB_EXT_IDENTITY_DST] = p;
    export_identity(&p, sa, 1);
  }

  rval = 0;

ret:
  return rval;
}

struct dump_state {
  struct sadb_msg *sadb_msg;
  struct socket *socket;
};

#if 0 /* XXX Need to add a tdb_walk routine for this to work */
int
pfkeyv2_dump_walker(struct tdb *sa, void *state)
{
  struct dump_state *dump_state = (struct dump_state *)state;
  void *headers[SADB_EXT_MAX+1], *buffer;
  int rval;

  if (!dump_state->sadb_msg->sadb_msg_satype ||
      (sa->tdb_satype == dump_state->sadb_msg->sadb_msg_satype)) {
    bzero(headers, sizeof(headers));
    headers[0] = (void *)dump_state->sadb_msg;
    if ((rval = pfkeyv2_get(sa, headers, &buffer)) != 0)
      return rval;
    rval = pfkeyv2_sendmessage(headers, PFKEYV2_SENDMESSAGE_UNICAST,
			       dump_state->socket, 0, 0);
    free(buffer, M_TEMP);
    if (rval)
      return rval;
  }

  return 0;
}
#endif /* 0 */

int
pfkeyv2_send(struct socket *socket, void *message, int len)
{
  void *headers[SADB_EXT_MAX + 1];
  int i, j, rval = 0, mode = PFKEYV2_SENDMESSAGE_BROADCAST, delflag = 0;
  struct pfkeyv2_socket *pfkeyv2_socket, *s = NULL;
  void *freeme = NULL, *bckptr = NULL;
  struct tdb sa, *sa2 = NULL;
  struct flow *flow = NULL;

  bzero(headers, sizeof(headers));
  
  for (pfkeyv2_socket = pfkeyv2_sockets;
       pfkeyv2_socket;
       pfkeyv2_socket = pfkeyv2_socket->next)
    if (pfkeyv2_socket->socket == socket)
      break;

  if (!pfkeyv2_socket) {
    rval = EINVAL;
    goto ret;
  }

  if (npromisc) {
    struct mbuf *packet;

    if (!(freeme = malloc(sizeof(struct sadb_msg) + len, M_TEMP,
			  M_DONTWAIT))) {
      rval = ENOMEM;
      goto ret;
    }

    bzero(freeme, sizeof(struct sadb_msg));
    ((struct sadb_msg *)freeme)->sadb_msg_version = PF_KEY_V2;
    ((struct sadb_msg *)freeme)->sadb_msg_type = SADB_X_PROMISC;
    ((struct sadb_msg *)freeme)->sadb_msg_len =
			   (sizeof(struct sadb_msg) + len) / sizeof(uint64_t);
    ((struct sadb_msg *)freeme)->sadb_msg_seq = curproc->p_pid;

    bcopy(message, freeme + sizeof(struct sadb_msg), len);

    if ((rval = pfdatatopacket(freeme, sizeof(struct sadb_msg) + len,
			       &packet)) != 0)
      goto ret;

    for (s = pfkeyv2_sockets; s; s = s->next)
      if (s->flags & PFKEYV2_SOCKETFLAGS_PROMISC)
	pfkey_sendup(s->socket, packet, 1);

    m_zero(packet);
    m_freem(packet);

    bzero(freeme, sizeof(struct sadb_msg) + len);
    free(freeme, M_TEMP);
    freeme = NULL;
  }

  if ((rval = pfkeyv2_parsemessage(message, len, headers)) != 0)
    goto ret;

  switch(((struct sadb_msg *)headers[0])->sadb_msg_type) {
    case SADB_GETSPI:
      bzero(&sa, sizeof(struct tdb));

      switch (((struct sadb_msg *)headers[0])->sadb_msg_satype) {
	case SADB_SATYPE_AH:
	    sa.tdb_sproto = IPPROTO_AH;
	    break;
	    
	case SADB_SATYPE_ESP:
	    sa.tdb_sproto = IPPROTO_ESP;
	    break;

	case SADB_SATYPE_X_AH_OLD:
	    sa.tdb_sproto = IPPROTO_AH;
	    break;
	    
	case SADB_SATYPE_X_ESP_OLD:
	    sa.tdb_sproto = IPPROTO_ESP;
	    break;

	case SADB_SATYPE_X_IPIP:
	    sa.tdb_sproto = IPPROTO_IPIP;
	    break;

	default: /* Nothing else supported */
	    rval = EOPNOTSUPP;
	    goto ret;
      }
      
      import_address((struct sockaddr *)&sa.tdb_src,
		     headers[SADB_EXT_ADDRESS_SRC]);
      import_address((struct sockaddr *)&sa.tdb_dst,
		     headers[SADB_EXT_ADDRESS_DST]);

      sa.tdb_spi = reserve_spi(((struct sadb_spirange *)headers[SADB_EXT_SPIRANGE])->sadb_spirange_min, ((struct sadb_spirange *)headers[SADB_EXT_SPIRANGE])->sadb_spirange_max, &sa.tdb_src, &sa.tdb_dst, sa.tdb_sproto, &rval);
      if (sa.tdb_spi == 0)
	goto ret;

      if (!(freeme = malloc(sizeof(struct sadb_sa), M_TEMP, M_DONTWAIT))) {
	rval = ENOMEM;
	goto ret;
      }

      bzero(freeme, sizeof(struct sadb_sa));
      headers[SADB_EXT_SPIRANGE] = NULL;
      headers[SADB_EXT_SA] = freeme;
      bckptr = freeme;
      export_sa((void **) &bckptr, &sa);
      break;

    case SADB_UPDATE:
      sa2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi,
		   (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
					    sizeof(struct sadb_address)),
		   SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));
      if (sa2 == NULL) {
	rval = ESRCH;
	goto ret;
      }
      
      if (sa2->tdb_flags & TDBF_INVALID) {
	MALLOC(freeme, struct tdb *, sizeof(struct tdb), M_TDB, M_WAITOK);
	bzero(freeme, sizeof(struct tdb));

	{
	  struct tdb *newsa = (struct tdb *)freeme;
	  struct ipsecinit ii;
	  int alg;

	  bzero(&ii, sizeof(struct ipsecinit));
	  switch (((struct sadb_msg *)headers[0])->sadb_msg_satype) {
	      case SADB_SATYPE_AH:
		  newsa->tdb_sproto = IPPROTO_AH;
		  alg = XF_NEW_AH;
		  break;
	    
	      case SADB_SATYPE_ESP:
		  newsa->tdb_sproto = IPPROTO_ESP;
		  alg = XF_NEW_ESP;
		  break;

	      case SADB_SATYPE_X_AH_OLD:
		  newsa->tdb_sproto = IPPROTO_AH;
		  alg = XF_OLD_AH;
		  break;
	    
	      case SADB_SATYPE_X_ESP_OLD:
		  newsa->tdb_sproto = IPPROTO_ESP;
		  alg = XF_OLD_ESP;
		  break;

	      case SADB_SATYPE_X_IPIP:
		  newsa->tdb_sproto = IPPROTO_IPIP;
		  alg = XF_IP4;
		  break;
		  
	      default: /* Nothing else supported */
		  rval = EOPNOTSUPP;
		  goto ret;
	  }
	  
	  import_sa(newsa, headers[SADB_EXT_SA], &ii);
	  import_address((struct sockaddr *)&newsa->tdb_src,
			 headers[SADB_EXT_ADDRESS_SRC]);
	  import_address((struct sockaddr *)&newsa->tdb_dst,
			 headers[SADB_EXT_ADDRESS_DST]);
	  import_address((struct sockaddr *)&newsa->tdb_proxy,
			 headers[SADB_EXT_ADDRESS_PROXY]);

	  import_lifetime(newsa, headers[SADB_EXT_LIFETIME_CURRENT], 2);
	  import_lifetime(newsa, headers[SADB_EXT_LIFETIME_SOFT], 1);
	  import_lifetime(newsa, headers[SADB_EXT_LIFETIME_HARD], 0);
	  import_key(&ii, headers[SADB_EXT_KEY_AUTH], 1);
	  import_key(&ii, headers[SADB_EXT_KEY_ENCRYPT], 0);
	  import_identity(newsa, headers[SADB_EXT_IDENTITY_SRC], 0);
	  import_identity(newsa, headers[SADB_EXT_IDENTITY_DST], 1);

	  headers[SADB_EXT_KEY_AUTH] = NULL;
	  headers[SADB_EXT_KEY_ENCRYPT] = NULL;

	  rval = tdb_init(newsa, alg, &ii);
	  if (rval) {
	    rval = EINVAL;
	    tdb_delete(freeme, 0);
	    freeme = NULL;
	    goto ret;
	  }
	  newsa->tdb_flow = sa2->tdb_flow;
	  newsa->tdb_cur_allocations = sa2->tdb_cur_allocations;
	  for (flow = newsa->tdb_flow; flow != NULL; flow = flow->flow_next)
	    flow->flow_sa = newsa;
	  sa2->tdb_flow = NULL;
	}

	 tdb_delete(sa2, 0);
	 puttdb((struct tdb *) freeme);
	 sa2 = freeme = NULL;
      } else {
	  if (headers[SADB_EXT_ADDRESS_PROXY] ||
	      headers[SADB_EXT_KEY_AUTH] ||
	      headers[SADB_EXT_KEY_ENCRYPT] ||
	      headers[SADB_EXT_IDENTITY_SRC] ||
	      headers[SADB_EXT_IDENTITY_DST] ||
	      headers[SADB_EXT_SENSITIVITY]) {
	    rval = EINVAL;
	    goto ret;
	  }

	  import_sa(sa2, headers[SADB_EXT_SA], NULL);
	  import_lifetime(sa2, headers[SADB_EXT_LIFETIME_CURRENT], 2);
	  import_lifetime(sa2, headers[SADB_EXT_LIFETIME_SOFT], 1);
	  import_lifetime(sa2, headers[SADB_EXT_LIFETIME_HARD], 0);
      }
      break;

    case SADB_ADD:
      sa2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi,
		   (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
					    sizeof(struct sadb_address)),
		   SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));
      if (sa2 != NULL) {
	rval = EEXIST;
	goto ret;
      }
      if (((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_state !=
	  SADB_SASTATE_MATURE) {
	rval = EINVAL;
	goto ret;
      }

      MALLOC(freeme, struct tdb *, sizeof(struct tdb), M_TDB, M_WAITOK);
      bzero(freeme, sizeof(struct tdb));

      {
	struct tdb *newsa = (struct tdb *) freeme;
	struct ipsecinit ii;
	int alg;

	bzero(&ii, sizeof(struct ipsecinit));
	switch (((struct sadb_msg *)headers[0])->sadb_msg_satype) {
	    case SADB_SATYPE_AH:
		newsa->tdb_sproto = IPPROTO_AH;
		alg = XF_NEW_AH;
		break;
	    
	    case SADB_SATYPE_ESP:
		newsa->tdb_sproto = IPPROTO_ESP;
		alg = XF_NEW_ESP;
		break;
		
	    case SADB_SATYPE_X_AH_OLD:
		newsa->tdb_sproto = IPPROTO_AH;
		alg = XF_OLD_AH;
		break;
	    
	    case SADB_SATYPE_X_ESP_OLD:
		newsa->tdb_sproto = IPPROTO_ESP;
		alg = XF_OLD_ESP;
		break;

	    case SADB_SATYPE_X_IPIP:
		newsa->tdb_sproto = IPPROTO_IPIP;
		alg = XF_IP4;
		break;

	    default: /* Nothing else supported */
		rval = EOPNOTSUPP;
		goto ret;
	}

	import_sa(newsa, headers[SADB_EXT_SA], &ii);
	import_address((struct sockaddr *)&newsa->tdb_src,
		       headers[SADB_EXT_ADDRESS_SRC]);
	import_address((struct sockaddr *)&newsa->tdb_dst,
		       headers[SADB_EXT_ADDRESS_DST]);
	import_address((struct sockaddr *)&newsa->tdb_proxy,
		       headers[SADB_EXT_ADDRESS_PROXY]);

	import_lifetime(newsa, headers[SADB_EXT_LIFETIME_CURRENT], 2);
	import_lifetime(newsa, headers[SADB_EXT_LIFETIME_SOFT], 1);
	import_lifetime(newsa, headers[SADB_EXT_LIFETIME_HARD], 0);
	import_key(&ii, headers[SADB_EXT_KEY_AUTH], 1);
	import_key(&ii, headers[SADB_EXT_KEY_ENCRYPT], 0);
	import_identity(newsa, headers[SADB_EXT_IDENTITY_SRC], 0);
	import_identity(newsa, headers[SADB_EXT_IDENTITY_DST], 1);

	headers[SADB_EXT_KEY_AUTH] = NULL;
	headers[SADB_EXT_KEY_ENCRYPT] = NULL;

	rval = tdb_init(newsa, alg, &ii);
	if (rval) {
	  rval = EINVAL;
	  tdb_delete(freeme, 0);
	  freeme = NULL;
	  goto ret;
	}
      }

      puttdb((struct tdb *)freeme);
      freeme = NULL;
      break;

    case SADB_DELETE:
	sa2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi,
		     (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
					      sizeof(struct sadb_address)),
		     SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));
	if (sa2 == NULL) {
	    rval = ESRCH;
	    goto ret;
	}
      
	tdb_delete(sa2, ((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_flags & SADB_SAFLAGS_X_CHAINDEL);
	sa2 = NULL;
	break;

    case SADB_GET:
      sa2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi,
		   (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
					    sizeof(struct sadb_address)),
		   SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));
      if (sa2 == NULL) {
	rval = ESRCH;
	goto ret;
      }
      
      rval = pfkeyv2_get(sa2, headers, &freeme);
      if (rval)
	mode = PFKEYV2_SENDMESSAGE_UNICAST;
      break;

    case SADB_REGISTER:
      pfkeyv2_socket->flags |= PFKEYV2_SOCKETFLAGS_REGISTERED;
      nregistered++;

      i = sizeof(struct sadb_supported) + sizeof(ealgs) + sizeof(aalgs);

      if (!(freeme = malloc(i, M_TEMP, M_DONTWAIT))) {
	rval = ENOMEM;
	goto ret;
      }
      
      /* Keep track what this socket has registered for */
      pfkeyv2_socket->registration |= (1 << ((struct sadb_msg *)message)->sadb_msg_satype);
      
      bzero(freeme, i);

      ((struct sadb_supported *)freeme)->sadb_supported_len =
							 i / sizeof(uint64_t);
      ((struct sadb_supported *)freeme)->sadb_supported_nauth =
				      sizeof(aalgs) / sizeof(struct sadb_alg);
      ((struct sadb_supported *)freeme)->sadb_supported_nencrypt =
				      sizeof(ealgs) / sizeof(struct sadb_alg);

      {
	void *p = freeme + sizeof(struct sadb_supported);

	bcopy(&aalgs[0], p, sizeof(aalgs));
	p += sizeof(aalgs);
	bcopy(&ealgs[0], p, sizeof(ealgs));
      }

      headers[SADB_EXT_SUPPORTED] = freeme;
      break;

    case SADB_ACQUIRE:
      rval = 0;
      break;

    case SADB_EXPIRE:
      rval = 0;
      break;

    case SADB_FLUSH:
/* XXX netsec_sadb_flush(((struct sadb_msg *)headers[0])->sadb_msg_satype); */
      rval = 0;
      break;

    case SADB_DUMP:
      {
        struct dump_state dump_state;
        dump_state.sadb_msg = (struct sadb_msg *)headers[0];
        dump_state.socket = socket;
	
/** XXX
        if (!(rval = netsec_sadb_walk(pfkeyv2_dump_walker, &dump_state, 1)))
	  goto realret;
*/
	if ((rval == ENOMEM) || (rval == ENOBUFS))
	  rval = 0;
      }
      break;

	
    case SADB_X_DELFLOW:
	delflag = 1;   /* fall through */
	
    case SADB_X_ADDFLOW: /* XXX This is highly INET dependent */
    {
	struct sockaddr_encap encapdst, encapgw, encapnetmask;
	struct flow *flow2 = NULL, *old_flow = NULL, *old_flow2 = NULL;
	union sockaddr_union *src, *dst, *srcmask, *dstmask;
	union sockaddr_union alts, altm;
	u_int8_t sproto = 0, local = 0, replace;
	struct rtentry *rt;

	/*
	 * SADB_SAFLAGS_X_REPLACEFLOW set means we should remove any
	 * potentially conflicting flow while we are adding this new one.
	 */
	replace = ((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_flags & 
	          SADB_SAFLAGS_X_REPLACEFLOW;
	if (replace && delflag) {
	    rval = EINVAL;
	    goto ret;
	}

	if (!delflag)
	{
	    sa2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi, (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] + sizeof(struct sadb_address)), SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));

	    if (sa2 == NULL) {
		rval = ESRCH;
		goto ret;
	    }
	}

	local = ((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_flags & 
		SADB_SAFLAGS_X_LOCALFLOW;
	bzero(&encapdst, sizeof(struct sockaddr_encap));
	bzero(&encapnetmask, sizeof(struct sockaddr_encap));
	bzero(&encapgw, sizeof(struct sockaddr_encap));
	bzero(&alts, sizeof(alts));
	bzero(&altm, sizeof(altm));
	
	src = (union sockaddr_union *) (headers[SADB_EXT_X_SRC_FLOW] + sizeof(struct sadb_address));
	dst = (union sockaddr_union *) (headers[SADB_EXT_X_DST_FLOW] + sizeof(struct sadb_address));
	srcmask = (union sockaddr_union *) (headers[SADB_EXT_X_SRC_MASK] + sizeof(struct sadb_address));
	dstmask = (union sockaddr_union *) (headers[SADB_EXT_X_DST_MASK] + sizeof(struct sadb_address));

	if (headers[SADB_EXT_X_PROTOCOL])
	  sproto = ((struct sadb_protocol *) headers[SADB_EXT_X_PROTOCOL])->sadb_protocol_proto;
	else
	  sproto = 0;
	
	src->sin.sin_addr.s_addr &= srcmask->sin.sin_addr.s_addr;
	dst->sin.sin_addr.s_addr &= dstmask->sin.sin_addr.s_addr;

	flow = find_global_flow(src, srcmask, dst, dstmask, sproto);
	if (!replace &&
	    ((delflag && (flow == NULL)) || (!delflag && (flow != NULL))))
	{
	    rval = delflag ? ESRCH : EEXIST;
	    goto ret;
	}

	/* Check for 0.0.0.0/255.255.255.255 if the flow is local */
	if (local)
	{
	    alts.sin.sin_family = altm.sin.sin_family = AF_INET;
	    alts.sin.sin_len = altm.sin.sin_len = sizeof(struct sockaddr_in);
	    alts.sin.sin_addr.s_addr = INADDR_ANY;
	    altm.sin.sin_addr.s_addr = INADDR_BROADCAST;

	    flow2 = find_global_flow(&alts, &altm, dst, dstmask, sproto);
	    if (!replace &&
		((delflag && (flow2 == NULL)) ||
		 (!delflag && (flow2 != NULL))))
	    {
		rval = delflag ? ESRCH : EEXIST;
		goto ret;
	    }
	}

	if (!delflag)
	{
	    if (replace)
	      old_flow = flow;
	    flow = get_flow();
	    bcopy(src, &flow->flow_src, src->sa.sa_len);
	    bcopy(dst, &flow->flow_dst, dst->sa.sa_len);
	    bcopy(srcmask, &flow->flow_srcmask, srcmask->sa.sa_len);
	    bcopy(dstmask, &flow->flow_dstmask, dstmask->sa.sa_len);
	    flow->flow_proto = sproto;
	    put_flow(flow, sa2);

	    if (local)
	    {
		if (replace)
		  old_flow2 = flow2;
		flow2 = get_flow();
		bcopy(&alts, &flow2->flow_src, alts.sa.sa_len);
		bcopy(dst, &flow2->flow_dst, dst->sa.sa_len);
		bcopy(&altm, &flow2->flow_srcmask, altm.sa.sa_len);
		bcopy(dstmask, &flow2->flow_dstmask, dstmask->sa.sa_len);
		flow2->flow_proto = sproto;
		put_flow(flow2, sa2);
	    }
	}
	
	/* Setup the encap fields */
	encapdst.sen_len = SENT_IP4_LEN;
	encapdst.sen_family = PF_KEY;
	encapdst.sen_type = SENT_IP4;
	encapdst.sen_ip_src = flow->flow_src.sin.sin_addr;
	encapdst.sen_ip_dst = flow->flow_dst.sin.sin_addr;
	encapdst.sen_proto = flow->flow_proto;
	encapdst.sen_sport = flow->flow_src.sin.sin_port;
	encapdst.sen_dport = flow->flow_dst.sin.sin_port;

	if (!delflag)
	{
	    encapgw.sen_len = SENT_IPSP_LEN;
	    encapgw.sen_family = PF_KEY;
	    encapgw.sen_type = SENT_IPSP;
	    encapgw.sen_ipsp_dst = sa2->tdb_dst.sin.sin_addr;
	    encapgw.sen_ipsp_spi = sa2->tdb_spi;
	    encapgw.sen_ipsp_sproto = sa2->tdb_sproto;
	}
	
	encapnetmask.sen_len = SENT_IP4_LEN;
	encapnetmask.sen_family = PF_KEY;
	encapnetmask.sen_type = SENT_IP4;
	encapnetmask.sen_ip_src = flow->flow_srcmask.sin.sin_addr;
	encapnetmask.sen_ip_dst = flow->flow_dstmask.sin.sin_addr;

	if (flow->flow_proto)
	{
	    encapnetmask.sen_proto = 0xff;
	    
	    if (flow->flow_src.sin.sin_port)
	      encapnetmask.sen_sport = 0xffff;
	    
	    if (flow->flow_dst.sin.sin_port)
	      encapnetmask.sen_dport = 0xffff;
	}
	/* Add the entry in the routing table */
	if (delflag)
	{
	    rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
		      (struct sockaddr *) 0,
		      (struct sockaddr *) &encapnetmask,
		      0, (struct rtentry **) 0);

	    delete_flow(flow, flow->flow_sa);
	    ipsec_in_use--;
	}
	else if (!replace)
	{
	    rval = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
			     (struct sockaddr *) &encapgw,
			     (struct sockaddr *) &encapnetmask,
			     RTF_UP | RTF_GATEWAY | RTF_STATIC,
			     (struct rtentry **) 0);
	    
	    if (rval)
	    {
		delete_flow(flow, sa2);
		if (flow2)
		  delete_flow(flow2, sa2);
		goto ret;
	    }

	    ipsec_in_use++;
	    sa2->tdb_cur_allocations++;
	}
	else
	{
	    rt = (struct rtentry *) rn_lookup(&encapdst, &encapnetmask, 
					      rt_tables[PF_KEY]);
	    if (rt == NULL)
	    {
		rval = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				 (struct sockaddr *) &encapgw,
				 (struct sockaddr *) &encapnetmask,
				 RTF_UP | RTF_GATEWAY | RTF_STATIC,
				 (struct rtentry **) 0);

		if (rval)
		{
		    delete_flow(flow, sa2);
		    if (flow2)
		      delete_flow(flow2, sa2);
		    goto ret;
		}
	    }
	    else if (rt_setgate(rt, rt_key(rt), (struct sockaddr *) &encapgw))
	    {
	        rval = ENOMEM;
		delete_flow(flow, sa2);
		if (flow2)
		  delete_flow(flow2, sa2);
		goto ret;
	    }

	    sa2->tdb_cur_allocations++;
	}

	/* If this is a "local" packet flow */
	if (local)
	{
	    encapdst.sen_ip_src.s_addr = INADDR_ANY;
	    encapnetmask.sen_ip_src.s_addr = INADDR_BROADCAST;

	    if (delflag)
	    {
		rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
			  (struct sockaddr *) 0,
			  (struct sockaddr *) &encapnetmask, 0,
			  (struct rtentry **) 0);

		delete_flow(flow2, flow2->flow_sa);
		ipsec_in_use--;
	    }
	    else if (!replace)
	    {
		rval = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				 (struct sockaddr *) &encapgw,
				 (struct sockaddr *) &encapnetmask,
				 RTF_UP | RTF_GATEWAY | RTF_STATIC,
				 (struct rtentry **) 0);
	    
		if (rval)
		{
		    /* Delete the first entry inserted */
		    encapdst.sen_ip_src = flow->flow_src.sin.sin_addr;
		    encapnetmask.sen_ip_src = flow->flow_srcmask.sin.sin_addr;
		
		    rtrequest(RTM_DELETE, (struct sockaddr *) &encapdst,
			      (struct sockaddr *) 0,
			      (struct sockaddr *) &encapnetmask, 0,
			      (struct rtentry **) 0);
		
		    delete_flow(flow, sa2);
		    delete_flow(flow2, sa2);
		    ipsec_in_use--;
		    goto ret;
		}

		ipsec_in_use++;
		sa2->tdb_cur_allocations++;
	    }
	    else
	    {
	        rt = (struct rtentry *) rn_lookup(&encapdst, &encapnetmask, 
						  rt_tables[PF_KEY]);
		if (rt == NULL)
		{
		    rval = rtrequest(RTM_ADD, (struct sockaddr *) &encapdst,
				     (struct sockaddr *) &encapgw,
				     (struct sockaddr *) &encapnetmask,
				     RTF_UP | RTF_GATEWAY | RTF_STATIC,
				     (struct rtentry **) 0);

		    if (rval)
		    {
			/*
			 * XXX We really should try to restore the non-local
			 * route if we need to abort here but that is getting
			 * very hairy.  Currently we do half the change and
			 * return an error, which is not optimal.
			 */

			if (old_flow)
			  delete_flow(old_flow, old_flow->flow_sa);
			delete_flow(flow2, sa2);
			goto ret;
		    }
		}
		else if (rt_setgate(rt, rt_key(rt),
				    (struct sockaddr *) &encapgw))
		{
		    /*
		     * XXX See above regarding the cleaning of the
		     * non-local route.
		     */
		    rval = ENOMEM;
		    if (old_flow)
		      delete_flow(old_flow, old_flow->flow_sa);
		    delete_flow(flow2, sa2);
		    goto ret;
		}

		sa2->tdb_cur_allocations++;
	    }
	}

	if (replace)
	{
	    if (old_flow != NULL)
	      delete_flow(old_flow, old_flow->flow_sa);
	    if (old_flow2 != NULL)
	      delete_flow(old_flow2, old_flow2->flow_sa);
	}

	/* If we are adding flows, check for allocation expirations */
	if (!delflag && !(replace && old_flow != NULL)) {
	    if ((sa2->tdb_flags & TDBF_ALLOCATIONS) &&
		(sa2->tdb_cur_allocations > sa2->tdb_exp_allocations)) {
		/* XXX expiration notification */

		tdb_delete(sa2, 0);
		break;
	    } else 
	      if ((sa2->tdb_flags & TDBF_SOFT_ALLOCATIONS) &&
		  (sa2->tdb_cur_allocations > sa2->tdb_soft_allocations)) {
		  /* XXX expiration notification */
		  sa2->tdb_flags &= ~TDBF_SOFT_ALLOCATIONS;
	      }
	}
    }

     break;
	
    case SADB_X_GRPSPIS:
    {
	struct tdb *tdb1, *tdb2, *tdb3;
	
	tdb1 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi,
		     (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
					      sizeof(struct sadb_address)),
		     SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));
	if (tdb1 == NULL) {
	    rval = ESRCH;
	    goto ret;
	}

	tdb2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_X_SA2])->sadb_sa_spi,
		     (union sockaddr_union *)(headers[SADB_EXT_X_DST2] +
					      sizeof(struct sadb_address)),
		     SADB_GETSPROTO(((struct sadb_protocol *)headers[SADB_EXT_X_PROTOCOL])->sadb_protocol_proto));

	if (tdb2 == NULL) {
	    rval = ESRCH;
	    goto ret;
	}

	/* Detect cycles */
	for (tdb3 = tdb2; tdb3; tdb3 = tdb3->tdb_onext)
	  if (tdb3 == tdb1)
	  {
	      rval = ESRCH;
	      goto ret;
	  }

	/* Maintenance */
	if ((tdb1->tdb_onext) &&
	    (tdb1->tdb_onext->tdb_inext == tdb1))
	  tdb1->tdb_onext->tdb_inext = NULL;

	if ((tdb2->tdb_inext) &&
	    (tdb2->tdb_inext->tdb_onext == tdb2))
	  tdb2->tdb_inext->tdb_onext = NULL;

	/* Link them */
	tdb1->tdb_onext = tdb2;
	tdb2->tdb_inext = tdb1;
    }
       
	break;
	
    case SADB_X_BINDSA:
    {
	struct tdb *tdb1, *tdb2;
	
	tdb1 = gettdb(((struct sadb_sa *)headers[SADB_EXT_SA])->sadb_sa_spi,
		     (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
					      sizeof(struct sadb_address)),
		     SADB_GETSPROTO(((struct sadb_msg *)headers[0])->sadb_msg_satype));
	if (tdb1 == NULL) {
	    rval = ESRCH;
	    goto ret;
	}

	if (TAILQ_FIRST(&tdb1->tdb_bind_in)) {
	    /* Incoming SA has not list of referencing incoming SAs */
	    rval = EINVAL;
	    goto ret;
	}

	tdb2 = gettdb(((struct sadb_sa *)headers[SADB_EXT_X_SA2])->sadb_sa_spi,
		     (union sockaddr_union *)(headers[SADB_EXT_X_DST2] +
					      sizeof(struct sadb_address)),
		     SADB_GETSPROTO(((struct sadb_protocol *)headers[SADB_EXT_X_PROTOCOL])->sadb_protocol_proto));

	if (tdb2 == NULL) {
	    rval = ESRCH;
	    goto ret;
	}

	if (tdb2->tdb_bind_out) {
	    /* Outgoing SA has no pointer to an outgoing SA */
	    rval = EINVAL;
	    goto ret;
	}

	/* Maintenance */
	if (tdb1->tdb_bind_out)
	    TAILQ_REMOVE(&tdb1->tdb_bind_out->tdb_bind_in, tdb1,
			 tdb_bind_in_next);

	/* Link them */
	tdb1->tdb_bind_out = tdb2;
	TAILQ_INSERT_TAIL(&tdb2->tdb_bind_in, tdb1, tdb_bind_in_next);
    }
       
	break;
	
    case SADB_X_PROMISC:
      if (len >= 2 * sizeof(struct sadb_msg)) {
	struct mbuf *packet;

	if ((rval = pfdatatopacket(message, len, &packet)) != 0)
	  goto ret;

	for (s = pfkeyv2_sockets; s; s = s->next)
	  if ((s != pfkeyv2_socket) &&
	      (!((struct sadb_msg *)headers[0])->sadb_msg_seq ||
	       (((struct sadb_msg *)headers[0])->sadb_msg_seq ==
		pfkeyv2_socket->pid)))
	    pfkey_sendup(s->socket, packet, 1);

	m_freem(packet);
      } else {
	if (len != sizeof(struct sadb_msg)) {
	  rval = EINVAL;
	  goto ret;
	}

	i = (pfkeyv2_socket->flags & PFKEYV2_SOCKETFLAGS_PROMISC) ? 1 : 0;
	j = ((struct sadb_msg *)headers[0])->sadb_msg_satype ? 1 : 0;
	
	if (i ^ j) {
	  if (j) {
	    pfkeyv2_socket->flags |= PFKEYV2_SOCKETFLAGS_PROMISC;
	    npromisc++;
	  } else {
	    pfkeyv2_socket->flags &= ~PFKEYV2_SOCKETFLAGS_PROMISC;
	    npromisc--;
	  }
	}
      }
      break;
    default:
      rval = EINVAL;
      goto ret;
  }

ret:
  if (rval) {
    if ((rval == EINVAL) || (rval == ENOMEM) ||
	(rval == ENOBUFS))
      goto realret;
    for (i = 1; i <= SADB_EXT_MAX; i++)
      headers[i] = NULL;
    ((struct sadb_msg *)headers[0])->sadb_msg_errno = abs(rval);
  } else {
    uint32_t seen = 0;

    for (i = 1; i <= SADB_EXT_MAX; i++)
      if (headers[i])
	seen |= (1 << i);

      if ((seen & sadb_exts_allowed_out[((struct sadb_msg *)headers[0])->sadb_msg_type]) != seen) {
        goto realret;
      }

      if ((seen & sadb_exts_required_out[((struct sadb_msg *)headers[0])->sadb_msg_type]) != sadb_exts_required_out[((struct sadb_msg *)headers[0])->sadb_msg_type]) {
        goto realret;
      }
  }

  rval = pfkeyv2_sendmessage(headers, mode, socket, 0, 0);

realret:
  if (freeme)
    free(freeme, M_TEMP);
  free(message, M_TEMP);

  return rval;
}

int
pfkeyv2_acquire(void *os)
{
#if 0
  int rval = 0;
  int i, j;
  void *p, *headers[SADB_EXT_MAX+1], *buffer;

  if (!nregistered) {
    rval = ESRCH;
    goto ret;
  }

  i = sizeof(struct sadb_msg) + sizeof(struct sadb_address) +
      PADUP(SA_LEN(&os->src.sa)) + sizeof(struct sadb_address) +
      PADUP(SA_LEN(&os->dst.sa)) + sizeof(struct sadb_prop) +
      os->nproposals * sizeof(struct sadb_comb) +
      2 * sizeof(struct sadb_ident);

  if (os->rekeysa)
    i += PADUP(os->rekeysa->srcident.bytes) +
	 PADUP(os->rekeysa->dstident.bytes);

  if (!(p = malloc(i, M_TEMP, M_DONTWAIT))) {
    rval = ENOMEM;
    goto ret;
  }

  bzero(headers, sizeof(headers));

  buffer = p;
  bzero(p, i);

  headers[0] = p;
  p += sizeof(struct sadb_msg);
  ((struct sadb_msg *)headers[0])->sadb_msg_version = PF_KEY_V2;
  ((struct sadb_msg *)headers[0])->sadb_msg_type    = SADB_ACQUIRE;
  ((struct sadb_msg *)headers[0])->sadb_msg_satype  = os->satype;
  ((struct sadb_msg *)headers[0])->sadb_msg_len     = i / sizeof(uint64_t);
  ((struct sadb_msg *)headers[0])->sadb_msg_seq     = pfkeyv2_seq++;

  headers[SADB_EXT_ADDRESS_SRC] = p;
  p += sizeof(struct sadb_address) + PADUP(SA_LEN(&os->src.sa));
  ((struct sadb_address *)headers[SADB_EXT_ADDRESS_SRC])->sadb_address_len = (sizeof(struct sadb_address) + SA_LEN(&os->src.sa) + sizeof(uint64_t) - 1) / sizeof(uint64_t);
  bcopy(&os->src, headers[SADB_EXT_ADDRESS_SRC] + sizeof(struct sadb_address),
	SA_LEN(&os->src.sa));

  headers[SADB_EXT_ADDRESS_DST] = p;
  p += sizeof(struct sadb_address) + PADUP(SA_LEN(&os->dst.sa));
  ((struct sadb_address *)headers[SADB_EXT_ADDRESS_DST])->sadb_address_len = (sizeof(struct sadb_address) + SA_LEN(&os->dst.sa) + sizeof(uint64_t) - 1) / sizeof(uint64_t);
  bcopy(&os->dst, headers[SADB_EXT_ADDRESS_DST] + sizeof(struct sadb_address),
	SA_LEN(&os->dst.sa));

  headers[SADB_EXT_IDENTITY_SRC] = p;
  p += sizeof(struct sadb_ident);
  ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC])->sadb_ident_type = os->srcidenttype;
  ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC])->sadb_ident_id = os->srcidentid;
  if (os->rekeysa) {
    ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC])->sadb_ident_len = (sizeof(struct sadb_ident) + PADUP(os->rekeysa->srcident.bytes)) / sizeof(uint64_t);
    bcopy(os->rekeysa->srcident.data, p, os->rekeysa->srcident.bytes);
    p += PADUP(os->rekeysa->srcident.bytes);
  } else
    ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC])->sadb_ident_len = (sizeof(struct sadb_ident)) / sizeof(uint64_t);

  headers[SADB_EXT_IDENTITY_DST] = p;
  p += sizeof(struct sadb_ident);
  ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC])->sadb_ident_type = os->dstidenttype;
  ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC])->sadb_ident_id = os->dstidentid;
  if (os->rekeysa) {
    ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_DST])->sadb_ident_len = (sizeof(struct sadb_ident) + PADUP(os->rekeysa->dstident.bytes)) / sizeof(uint64_t);
    bcopy(os->rekeysa->dstident.data, p, os->rekeysa->dstident.bytes);
    p += PADUP(os->rekeysa->srcident.bytes);
  } else
    ((struct sadb_ident *)headers[SADB_EXT_IDENTITY_DST])->sadb_ident_len = (sizeof(struct sadb_ident)) / sizeof(uint64_t);

  headers[SADB_EXT_PROPOSAL] = p;
  p += sizeof(struct sadb_prop);
  ((struct sadb_prop *)headers[SADB_EXT_PROPOSAL])->sadb_prop_len = (sizeof(struct sadb_prop) + sizeof(struct sadb_comb) * os->nproposals) / sizeof(uint64_t);
  ((struct sadb_prop *)headers[SADB_EXT_PROPOSAL])->sadb_prop_num = os->nproposals;

  {
    struct sadb_comb *sadb_comb = p;
    struct netsec_sadb_proposal *proposal = os->proposals;

    for (j = 0; j < os->nproposals; j++) {
      sadb_comb->sadb_comb_auth = proposal->auth;
      sadb_comb->sadb_comb_encrypt = proposal->encrypt;
      sadb_comb->sadb_comb_flags = proposal->flags;
      sadb_comb->sadb_comb_auth_minbits = proposal->auth_minbits;
      sadb_comb->sadb_comb_auth_maxbits = proposal->auth_maxbits;
      sadb_comb->sadb_comb_encrypt_minbits = proposal->encrypt_minbits;
      sadb_comb->sadb_comb_encrypt_maxbits = proposal->encrypt_maxbits;
      sadb_comb->sadb_comb_soft_allocations = proposal->soft.allocations;
      sadb_comb->sadb_comb_hard_allocations = proposal->hard.allocations;
      sadb_comb->sadb_comb_soft_bytes = proposal->soft.bytes;
      sadb_comb->sadb_comb_hard_bytes = proposal->hard.bytes;
      sadb_comb->sadb_comb_soft_addtime = proposal->soft.addtime;
      sadb_comb->sadb_comb_hard_addtime = proposal->hard.addtime;
      sadb_comb->sadb_comb_soft_usetime = proposal->soft.usetime;
      sadb_comb->sadb_comb_hard_usetime = proposal->hard.usetime;
      sadb_comb++;
      proposal++;
    }
  }

  if ((rval = pfkeyv2_sendmessage(headers, PFKEYV2_SENDMESSAGE_REGISTERED,
				  NULL, os->satype, count))!= 0)
    goto ret;

  rval = 0;

ret:
  return rval;
#endif
  return 0;
}

int
pfkeyv2_expire(struct tdb *sa, u_int16_t type)
{
  int rval = 0;
  int i;
  u_int8_t satype;
  void *p, *headers[SADB_EXT_MAX+1], *buffer;

  switch (sa->tdb_sproto) {
    case IPPROTO_AH:
      satype = sa->tdb_xform->xf_type == XF_OLD_AH ? SADB_SATYPE_X_AH_OLD : SADB_SATYPE_AH;
      break;
    case IPPROTO_ESP:
      satype = sa->tdb_xform->xf_type == XF_OLD_ESP ? SADB_SATYPE_X_ESP_OLD : SADB_SATYPE_ESP;
      break;
    case IPPROTO_IPIP:
      satype = SADB_SATYPE_X_IPIP;
      break;
    default:
      rval = EOPNOTSUPP;
      goto ret;
  }

  i = sizeof(struct sadb_msg) + sizeof(struct sadb_sa) +
      2 * sizeof(struct sadb_lifetime) +
      sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_src.sa)) +
      sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_dst.sa));

  if (!(p = malloc(i, M_TEMP, M_DONTWAIT))) {
    rval = ENOMEM;
    goto ret;
  }

  bzero(headers, sizeof(headers));

  buffer = p;
  bzero(p, i);

  headers[0] = p;
  p += sizeof(struct sadb_msg);
  ((struct sadb_msg *)headers[0])->sadb_msg_version = PF_KEY_V2;
  ((struct sadb_msg *)headers[0])->sadb_msg_type    = SADB_EXPIRE;
  ((struct sadb_msg *)headers[0])->sadb_msg_satype  = satype;
  ((struct sadb_msg *)headers[0])->sadb_msg_len     = i / sizeof(uint64_t);
  ((struct sadb_msg *)headers[0])->sadb_msg_seq     = pfkeyv2_seq++;

  headers[SADB_EXT_SA] = p;
  export_sa(&p, sa);

  headers[SADB_EXT_LIFETIME_CURRENT] = p;
  export_lifetime(&p, sa, 2);

  headers[type] = p;
  export_lifetime(&p, sa, type == SADB_EXT_LIFETIME_SOFT ? 1 : 0);

  headers[SADB_EXT_ADDRESS_SRC] = p;
  export_address(&p, (struct sockaddr *)&sa->tdb_src);

  headers[SADB_EXT_ADDRESS_DST] = p;
  export_address(&p, (struct sockaddr *)&sa->tdb_dst);

  if ((rval = pfkeyv2_sendmessage(headers, PFKEYV2_SENDMESSAGE_BROADCAST,
				  NULL, 0, 0))!= 0)
    goto ret;

  rval = 0;

ret:
  return rval;
}

int
pfkeyv2_init(void)
{
  int rval;

  bzero(&pfkeyv2_version, sizeof(struct pfkey_version));
  pfkeyv2_version.protocol = PFKEYV2_PROTOCOL;
  pfkeyv2_version.create   = &pfkeyv2_create;
  pfkeyv2_version.release  = &pfkeyv2_release;
  pfkeyv2_version.send     = &pfkeyv2_send;

  rval = pfkey_register(&pfkeyv2_version);
  return rval;
}

int
pfkeyv2_cleanup(void)
{
  pfkey_unregister(&pfkeyv2_version);
  return 0;
}
