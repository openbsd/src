CFLAGS+=	-I${.OBJDIR} -I${.CURDIR}/..

CLEANFILES=	gen_minfd minfd.h

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS} == "yes")
CFLAGS+= -DKRB4 -I/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.if (${AFS} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKRBAFS}
.endif # AFS
.endif # KERBEROS

USER_SHELLS = sh jsh ksh csh tcsh bash zsh ash
