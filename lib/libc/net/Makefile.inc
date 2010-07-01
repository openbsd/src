#	$OpenBSD: Makefile.inc,v 1.48 2010/07/01 19:15:30 deraadt Exp $

# net sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/net ${LIBCSRCDIR}/net

CFLAGS+=-DRESOLVSORT

SRCS+=	base64.c freeaddrinfo.c gai_strerror.c getaddrinfo.c gethostnamadr.c \
	getifaddrs.c getnameinfo.c getnetbyaddr.c getnetbyname.c getnetent.c \
	getnetnamadr.c getpeereid.c getproto.c getprotoent.c getprotoname.c \
	getservbyname.c getservbyport.c getservent.c getrrsetbyname.c \
	herror.c if_indextoname.c if_nameindex.c if_nametoindex.c inet_addr.c \
	inet_lnaof.c inet_makeaddr.c inet_neta.c inet_netof.c inet_network.c \
	inet_net_ntop.c inet_net_pton.c inet_ntoa.c inet_ntop.c inet_pton.c \
	linkaddr.c net_addrcmp.c nsap_addr.c \
	rcmd.c ruserok.c rresvport.c recv.c res_comp.c res_data.c res_debug.c \
	res_debug_syms.c \
	res_init.c res_mkquery.c res_query.c res_random.c res_send.c send.c \
	sethostent.c ethers.c rcmdsh.c

# IPv6
SRCS+=	ip6opt.c rthdr.c vars6.c

# machine-dependent net sources
# m-d Makefile.inc must include sources for:
#	htonl() htons() ntohl() ntohs()

.include "${LIBCSRCDIR}/arch/${MACHINE_CPU}/net/Makefile.inc"

MAN+=	byteorder.3 ethers.3 gai_strerror.3 getaddrinfo.3 gethostbyname.3 \
	getifaddrs.3 getnameinfo.3 getnetent.3 getpeereid.3 getprotoent.3 \
	getrrsetbyname.3 getservent.3 if_indextoname.3 inet.3 \
	inet_net.3 inet6_option_space.3 inet6_rthdr_space.3 \
	inet6_opt_init.3 inet6_rth_space.3 \
	link_addr.3 net_addrcmp.3 \
	rcmd.3 rcmdsh.3 resolver.3

MLINKS+=byteorder.3 htonl.3 byteorder.3 htons.3 byteorder.3 ntohl.3 \
	byteorder.3 ntohs.3 byteorder.3 htobe16.3 byteorder.3 htobe32.3 \
	byteorder.3 htobe64.3 byteorder.3 betoh16.3 byteorder.3 betoh32.3 \
	byteorder.3 betoh64.3 byteorder.3 htole16.3 byteorder.3 htole32.3 \
	byteorder.3 htole64.3 byteorder.3 letoh16.3 byteorder.3 letoh32.3 \
	byteorder.3 letoh64.3 byteorder.3 swap16.3 byteorder.3 swap32.3 \
	byteorder.3 swap64.3
MLINKS+=ethers.3 ether_aton.3 ethers.3 ether_hostton.3 ethers.3 ether_line.3 \
	ethers.3 ether_ntoa.3 ethers.3 ether_ntohost.3
MLINKS+=getaddrinfo.3 freeaddrinfo.3
MLINKS+=gethostbyname.3 endhostent.3 gethostbyname.3 gethostbyaddr.3 \
	gethostbyname.3 sethostent.3 gethostbyname.3 gethostent.3 \
	gethostbyname.3 herror.3 gethostbyname.3 gethostbyname2.3 \
	gethostbyname.3 hstrerror.3
MLINKS+=getifaddrs.3 freeifaddrs.3
MLINKS+=getnetent.3 endnetent.3 getnetent.3 getnetbyaddr.3 \
	getnetent.3 getnetbyname.3 getnetent.3 setnetent.3
MLINKS+=getprotoent.3 endprotoent.3 getprotoent.3 getprotobyname.3 \
	getprotoent.3 getprotobynumber.3 getprotoent.3 setprotoent.3 \
	getprotoent.3 getprotoent_r.3 getprotoent.3 getprotobyname_r.3 \
	getprotoent.3 getprotobynumber_r.3 getprotoent.3 setprotoent_r.3 \
	getprotoent.3 endprotoent_r.3
MLINKS+=getservent.3 endservent.3 getservent.3 getservbyname.3 \
	getservent.3 getservbyport.3 getservent.3 setservent.3 \
	getservent.3 getservent_r.3 getservent.3 getservbyname_r.3 \
	getservent.3 getservbyport_r.3 getservent.3 setservent_r.3 \
	getservent.3 endservent_r.3
MLINKS+= if_indextoname.3 if_nametoindex.3 if_indextoname.3 if_nameindex.3 \
	if_indextoname.3 if_freenameindex.3
MLINKS+=inet.3 addr.3 inet.3 inet_addr.3 inet.3 inet_aton.3 \
	inet.3 inet_lnaof.3 inet.3 inet_makeaddr.3 inet.3 inet_netof.3 \
	inet.3 inet_network.3 inet.3 inet_ntoa.3 inet.3 network.3 \
	inet.3 ntoa.3 inet.3 inet_ntop.3 inet.3 inet_pton.3
MLINKS+=inet_net.3 inet_net_ntop.3 inet_net.3 inet_net_pton.3
MLINKS+=link_addr.3 link_ntoa.3
MLINKS+=rcmd.3 iruserok.3 rcmd.3 rresvport.3 rcmd.3 ruserok.3 \
	rcmd.3 rresvport_af.3 rcmd.3 rcmd_af.3 rcmd.3 iruserok_sa.3
MLINKS+=resolver.3 dn_comp.3 resolver.3 dn_expand.3 resolver.3 res_init.3 \
	resolver.3 res_mkquery.3 resolver.3 res_send.3 resolver.3 res_query.3 \
	resolver.3 res_search.3
MLINKS+=getrrsetbyname.3 freerrset.3
MLINKS+=inet6_option_space.3 inet6_option_init.3 \
	inet6_option_space.3 inet6_option_append.3 \
	inet6_option_space.3 inet6_option_alloc.3 \
	inet6_option_space.3 inet6_option_next.3 \
	inet6_option_space.3 inet6_option_find.3
MLINKS+=inet6_rthdr_space.3 inet6_rthdr_init.3 \
	inet6_rthdr_space.3 inet6_rthdr_add.3 \
	inet6_rthdr_space.3 inet6_rthdr_lasthop.3 \
	inet6_rthdr_space.3 inet6_rthdr_reverse.3 \
	inet6_rthdr_space.3 inet6_rthdr_segments.3 \
	inet6_rthdr_space.3 inet6_rthdr_getaddr.3 \
	inet6_rthdr_space.3 inet6_rthdr_getflags.3
MLINKS+=inet6_opt_init.3 inet6_opt_append.3 \
	inet6_opt_init.3 inet6_opt_finish.3 \
	inet6_opt_init.3 inet6_opt_set_val.3 \
	inet6_opt_init.3 inet6_opt_next.3 \
	inet6_opt_init.3 inet6_opt_find.3 \
	inet6_opt_init.3 inet6_opt_get_val.3
MLINKS+=inet6_rth_space.3 inet6_rth_init.3 \
	inet6_rth_space.3 inet6_rth_add.3 \
	inet6_rth_space.3 inet6_rth_reverse.3 \
	inet6_rth_space.3 inet6_rth_segments.3 \
	inet6_rth_space.3 inet6_rth_getaddr.3 \
