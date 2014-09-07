LIB=            aa
SRCS=           aa.c
LDADD+=         -Wl,-E
LDADD+=         -Wl,-rpath,$(BB_OBJDIR)
LDADD+=         -L$(BB_OBJDIR) -lbb

regress: all

.include <bsd.lib.mk>

