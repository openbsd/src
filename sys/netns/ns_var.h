/*	$OpenBSD: ns_var.h,v 1.8 2002/03/14 03:16:12 millert Exp $	*/

#ifdef _KERNEL
struct socket;
struct nspcb;
struct ifnet;
struct ns_ifaddr;
struct sockaddr_ns;
struct mbuf;
struct ns_addr;
struct route;
struct rtentry;
struct ifnet_en;
struct in_addr;
struct sockaddr;

/* ns.c */
int ns_control(struct socket *, u_long, caddr_t, struct ifnet *);
void ns_ifscrub(struct ifnet *, struct ns_ifaddr *);
int ns_ifinit(struct ifnet *, struct ns_ifaddr *, struct sockaddr_ns *,
		   int);
struct ns_ifaddr *ns_iaonnetof(struct ns_addr *);

/* ns_cksum.c */
u_short ns_cksum(struct mbuf *, int);

/* ns_error.c */
int ns_err_x(int);
void ns_error(struct mbuf *, int, int );
void ns_printhost(struct ns_addr *);
void ns_err_input(struct mbuf *);
u_long nstime(void);
int ns_echo(struct mbuf *);

/* ns_input.c */
void ns_init(void);
void nsintr(void);
void *idp_ctlinput(int, struct sockaddr *, void *);
void idp_forward(struct mbuf *);
int idp_do_route(struct ns_addr *, struct route *);
void idp_undo_route(struct route *);
void ns_watch_output(struct mbuf *, struct ifnet *);

/* ns_ip.c */
struct ifnet_en *nsipattach(void);
int nsipioctl(struct ifnet *, u_long, caddr_t);
void idpip_input(struct mbuf *, ...);
int nsipoutput(struct ifnet *, struct mbuf *, struct sockaddr *,     
		struct rtentry *);
void nsipstart(struct ifnet *);
int nsip_route(struct mbuf *);
int nsip_free(struct ifnet *);
void *nsip_ctlinput(int, struct sockaddr *, void *);
int nsip_rtchange(struct in_addr *);

/* ns_output.c */
int ns_output(struct mbuf *, ...);

/* ns_pcb.c */
int ns_pcballoc(struct socket *, struct nspcb *);
int ns_pcbbind(struct nspcb *, struct mbuf *);
int ns_pcbconnect(struct nspcb *, struct mbuf *);
void ns_pcbdisconnect(struct nspcb *);
void ns_pcbdetach(struct nspcb *);
void ns_setsockaddr(struct nspcb *, struct mbuf *);
void ns_setpeeraddr(struct nspcb *, struct mbuf *);
void ns_pcbnotify(struct ns_addr *, int, void (*)(struct nspcb *), long);
void ns_rtchange(struct nspcb *);
struct nspcb *ns_pcblookup(struct ns_addr *, u_short, int);

#endif
