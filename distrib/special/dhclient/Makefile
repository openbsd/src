.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../usr.sbin/dhcp/dhclient
.PATH:  ${.CURDIR}/../../../usr.sbin/dhcp/common

CPPFLAGS+= -I${.CURDIR}/../../../usr.sbin/dhcp -I${.CURDIR}/../../../usr.sbin/dhcp/includes
SRCS=	dhclient.c clparse.c \
        raw.c parse.c nit.c icmp.c dispatch.c conflex.c upf.c bpf.c socket.c \
        packet.c memory.c print.c options.c inet.c convert.c sysconf.c \
        tree.c tables.c hash.c alloc.c errwarn.c inet_addr.c dns.c resolv.c
PROG=	dhclient
MAN=

LDSTATIC?=      -static

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
