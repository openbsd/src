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

#include <sys/protosw.h>
#include <sys/domain.h>
#include <net/raw_cb.h>
#include <netinet/ip_ipsp.h>

#define PFKEY_PROTOCOL_MAX 3
static struct pfkey_version *pfkey_versions[PFKEY_PROTOCOL_MAX+1] = { NULL, NULL, NULL, NULL };

#define PFKEY_MSG_MAXSZ 4096

struct sockaddr pfkey_addr = { 2, PF_KEY, };

/* static struct domain pfkey_domain; */
static int pfkey_usrreq(struct socket *socket, int req, struct mbuf *mbuf,
			struct mbuf *nam, struct mbuf *control);
static int pfkey_output(struct mbuf *mbuf, struct socket *socket);

int pfkey_register(struct pfkey_version *version);
int pfkey_unregister(struct pfkey_version *version);
int pfkey_sendup(struct socket *socket, struct mbuf *packet, int more);
void pfkey_init(void);
static int pfkey_buildprotosw(void);

int
pfkey_register(struct pfkey_version *version)
{
  int rval;

  if ((version->protocol > PFKEY_PROTOCOL_MAX) || (version->protocol < 0))
    return EPROTONOSUPPORT;

  if (pfkey_versions[version->protocol])
    return EADDRINUSE;

  pfkey_versions[version->protocol] = version;

  if ((rval = pfkey_buildprotosw()) != 0) {
    pfkey_versions[version->protocol] = NULL;
    return rval;
  }

  return 0;
}

int
pfkey_unregister(struct pfkey_version *version)
{
  int rval;

  if ((rval = pfkey_buildprotosw()) != 0)
    return rval;

  pfkey_versions[version->protocol] = NULL;
  return 0;
}

int
pfkey_sendup(struct socket *socket, struct mbuf *packet, int more)
{
  struct mbuf *packet2;
  int s;

  if (more) {
    if (!(packet2 = m_copym(packet, 0, M_COPYALL, M_DONTWAIT)))
      return ENOMEM;
  } else
    packet2 = packet;

  s = spltdb();
  if (!sbappendaddr(&socket->so_rcv, &pfkey_addr, packet2, NULL)) {
    m_freem(packet2);
    splx(s);
    return ENOBUFS;
  }
  splx(s);

  sorwakeup(socket);
  return 0;
}

static int
pfkey_output(struct mbuf *mbuf, struct socket *socket)
{
  void *message;
  int error = 0;

#if DIAGNOSTIC
  if (!mbuf || !(mbuf->m_flags & M_PKTHDR)) {
    error = EINVAL;
    goto ret;
  }
#endif /* DIAGNOSTIC */

  if (mbuf->m_pkthdr.len > PFKEY_MSG_MAXSZ) {
    error = EMSGSIZE;
    goto ret;
  }

  if (!(message = malloc((unsigned long) mbuf->m_pkthdr.len, M_PFKEY,
			 M_DONTWAIT))) {
    error = ENOMEM;
    goto ret;
  }

  m_copydata(mbuf, 0, mbuf->m_pkthdr.len, message);

  error =
    pfkey_versions[socket->so_proto->pr_protocol]->send(socket, message,
							mbuf->m_pkthdr.len);

 ret:
  if (mbuf)
    m_freem (mbuf);
  return error;
}

static int
pfkey_attach(struct socket *socket, struct mbuf *proto)
{
  int rval;
  int s;

  if (!(socket->so_pcb = malloc(sizeof(struct rawcb), M_PCB, M_DONTWAIT)))
    return ENOMEM;
  bzero(socket->so_pcb, sizeof(struct rawcb));

  s = splnet();
  rval = raw_usrreq(socket, PRU_ATTACH, NULL, proto, NULL);
  splx(s);
  if (rval)
    goto ret;

  ((struct rawcb *)socket->so_pcb)->rcb_faddr = &pfkey_addr;
  soisconnected(socket);

  socket->so_options |= SO_USELOOPBACK;
  if ((rval = pfkey_versions[socket->so_proto->pr_protocol]->create(socket))
      != 0)
    goto ret;

  return 0;

ret:
  free(socket->so_pcb, M_PCB);
  return rval;
}

static int
pfkey_detach(struct socket *socket)
{
  int rval, i, s;

  rval = pfkey_versions[socket->so_proto->pr_protocol]->release(socket);
  s = splnet();
  i = raw_usrreq(socket, PRU_DETACH, NULL, NULL, NULL);
  splx(s);

  if (!rval)
    rval = i;

  return rval;
}

static int
pfkey_usrreq(struct socket *socket, int req, struct mbuf *mbuf,
	     struct mbuf *nam, struct mbuf *control)
{
  int rval;
  int s;

  if ((socket->so_proto->pr_protocol > PFKEY_PROTOCOL_MAX) ||
      (socket->so_proto->pr_protocol < 0) ||
      !pfkey_versions[socket->so_proto->pr_protocol])
    return EPROTONOSUPPORT;

  switch(req) {
    case PRU_ATTACH:
      return pfkey_attach(socket, nam);

    case PRU_DETACH:
      return pfkey_detach(socket);

    default:
      s = splnet();
      rval = raw_usrreq(socket, req, mbuf, nam, control);
      splx(s);
  }

  return rval;
}

static struct domain pfkey_domain = {
  PF_KEY,
  "PF_KEY",
  NULL, /* init */
  NULL, /* externalize */
  NULL, /* dispose */
  NULL, /* protosw */
  NULL, /* protoswNPROTOSW */
  NULL, /* dom_next */
  rn_inithead, /* dom_rtattach */
  16, /* rtoffset */
  sizeof(struct sockaddr_encap)  /* maxrtkey */
};

static struct protosw pfkey_protosw_template = {
  SOCK_RAW,
  &pfkey_domain,
  -1, /* protocol */
  PR_ATOMIC | PR_ADDR,
  (void *) raw_input,
  (void *) pfkey_output,
  (void *) raw_ctlinput,
  NULL, /* ctloutput */
  pfkey_usrreq,
  NULL, /* init */
  NULL, /* fasttimo */
  NULL, /* slowtimo */
  NULL, /* drain */
  NULL	/* sysctl */
};

static int
pfkey_buildprotosw(void)
{  
  struct protosw *protosw, *p;
  int i, j;

  for (i = j = 0; i <= PFKEY_PROTOCOL_MAX; i++)
    if (pfkey_versions[i])
      j++;

  if (j) {
    if (!(protosw = malloc(j * sizeof(struct protosw), M_PFKEY, M_DONTWAIT)))
      return ENOMEM;

    for (i = 0, p = protosw; i <= PFKEY_PROTOCOL_MAX; i++)
      if (pfkey_versions[i]) {
	bcopy(&pfkey_protosw_template, p, sizeof(struct protosw));
	p->pr_protocol = pfkey_versions[i]->protocol;
	p++;
      }

    if (pfkey_domain.dom_protosw)
      free(pfkey_domain.dom_protosw, M_PFKEY);

    pfkey_domain.dom_protosw = protosw;
    pfkey_domain.dom_protoswNPROTOSW = p;
  } else  {
    if (!(protosw = malloc(sizeof(struct protosw), M_PFKEY, M_DONTWAIT)))
      return ENOMEM;

    bcopy(&pfkey_protosw_template, protosw, sizeof(struct protosw));

    if (pfkey_domain.dom_protosw)
      free(pfkey_domain.dom_protosw, M_PFKEY);

    pfkey_domain.dom_protosw = protosw;
    pfkey_domain.dom_protoswNPROTOSW = protosw;
  }

  return 0;
}

void pfkey_init(void)
{
  if (pfkey_buildprotosw() != 0)
    return;

  pfkey_domain.dom_next = domains;
  domains = &pfkey_domain;
  pfkeyv2_init();
}
