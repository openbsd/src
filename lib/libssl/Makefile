.include <bsd.own.mk>
ECHO= /bin/echo

.if exists(${.OBJDIR}/src-patent)
SUBDIR= crypto-patent ssl-patent
.else
SUBDIR= crypto ssl
.endif

.include <bsd.subdir.mk>
