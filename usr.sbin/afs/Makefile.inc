
AFSSRC	= ${.CURDIR}/${AFSTOPDIR}/src

YDR	= ${AFSTOPDIR}/usr.sbin/ydr/ydr

OS_VER	!= uname -r
BINDIR	?=/usr/sbin

ROKENDIR	= ${AFSTOPDIR}/../../kerberosV/lib/roken
LIBROKEN	= ${ROKENDIR}/libroken.a

LIBARLA		= ${AFSTOPDIR}/lib/libarla/libarla.a

CFLAGS	+= -DHAVE_CONFIG_H -DSYSCONFDIR=\"/etc/afs\" \
	   -DARLACACHEDIR=\"/var/spool/afs\"\
	   -DARLACONFFILE=\"/etc/afs/afsd.conf\" -DKERBEROS \
	   -DARLAVENDOR=\"unknown\" -DARLACPU=\"${MACHINE_ARCH}\" \
	   -DARLAOS=\"openbsd${OS_VER}\" \

CFLAGS	+= -I${.CURDIR}/${AFSTOPDIR}/include -I${AFSSRC}/include \
	   -I${.CURDIR}/${AFSTOPDIR}/../../kerberosV/include \
	   -I${.CURDIR}/${AFSTOPDIR}/../../kerberosV/src/lib/roken \
	   -I${AFSTOPDIR}/lib/libarla -I${AFSSRC}/lwp \
	   -I${AFSSRC}/rxkad -I${AFSSRC}/lib/bufdir -I${AFSSRC}/lib/ko \
	   -I${AFSSRC} -I${DESTDIR}/usr/include/kerberosIV \
	   -I${AFSSRC}/util -I${AFSSRC}/arlad \
	   -I${AFSSRC}/lib/sl

#CFLAGS+=-g -Wall -Werror -Wno-uninitialized

