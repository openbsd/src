CFLAGS+=	-I${.OBJDIR} -I${.CURDIR}/..

.include <bsd.own.mk> # for KERBEROS and AFS

.if (${KERBEROS} == "yes")
CFLAGS+= -DKRB4 -I/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.endif

.if (${AFS} == "yes")
CFLAGS+= -DAFS
LDADD+=	 -lkafs
DPADD+=	 ${LIBKRBAFS}
.endif

USER_SHELLS = sh jsh ksh csh tcsh bash zsh ash
