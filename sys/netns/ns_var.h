/*	$OpenBSD: ns_var.h,v 1.3 1996/04/24 08:46:20 mickey Exp $	*/

#ifdef _KERNEL
struct socket;
struct nspcb;
struct ifnet;
struct ns_ifaddr;
struct sockaddr_ns;
struct mbuf;
struct ns_addr;
struct route;
struct ifnet_en;
struct in_addr;

/* ns.c */
int ns_control __P((struct socket *, u_long, caddr_t, struct ifnet *));
void ns_ifscrub __P((struct ifnet *, struct ns_ifaddr *));
int ns_ifinit __P((struct ifnet *, struct ns_ifaddr *, struct sockaddr_ns *,
		   int));
struct ns_ifaddr *ns_iaonnetof __P((struct ns_addr *));

/* ns_cksum.c */
u_short ns_cksum __P((struct mbuf *, int));

/* ns_error.c */
int ns_err_x __P((int));
void ns_error __P((struct mbuf *, int, int ));
void ns_printhost __P((struct ns_addr *));
void ns_err_input __P((struct mbuf *));
u_long nstime __P((void));
int ns_echo __P((struct mbuf *));

/* ns_input.c */
void ns_init __P((void));
void nsintr __P((void));
void *idp_ctlinput __P((int, struct sockaddr *, void *));
void idp_forward __P((struct mbuf *));
int idp_do_route __P((struct ns_addr *, struct route *));
void idp_undo_route __P((struct route *));
void ns_watch_output __P((struct mbuf *, struct ifnet *));

/* ns_ip.c */
struct ifnet_en *nsipattach __P((void));
int nsipioctl __P((struct ifnet *, u_long, caddr_t));
void idpip_input __P((struct mbuf *, ...));
int nsipoutput __P((struct ifnet_en *, struct mbuf *, struct sockaddr *));
void nsipstart __P((struct ifnet *));
int nsip_route __P((struct mbuf *));
int nsip_free __P((struct ifnet *));
void *nsip_ctlinput __P((int, struct sockaddr *, void *));
int nsip_rtchange __P((struct in_addr *));

/* ns_output.c */
int ns_output __P((struct mbuf *, ...));

/* ns_pcb.c */
int ns_pcballoc __P((struct socket *, struct nspcb *));
int ns_pcbbind __P((struct nspcb *, struct mbuf *));
int ns_pcbconnect __P((struct nspcb *, struct mbuf *));
void ns_pcbdisconnect __P((struct nspcb *));
void ns_pcbdetach __P((struct nspcb *));
void ns_setsockaddr __P((struct nspcb *, struct mbuf *));
void ns_setpeeraddr __P((struct nspcb *, struct mbuf *));
void ns_pcbnotify __P((struct ns_addr *, int, void (*)(struct nspcb *), long));
void ns_rtchange __P((struct nspcb *));
struct nspcb *ns_pcblookup __P((struct ns_addr *, u_short, int));

#endif
