#	$OpenBSD: Makefile,v 1.10 1999/03/06 20:19:25 millert Exp $

.PATH:	${.CURDIR}/doc

CFLAGS+=-I${.CURDIR} -I${.OBJDIR} -DHAVE_CONFIG_H

LDADD+=	-lcurses
DPADD+=	${LIBCURSES}
PROG=	vim

SRCS=	alloc.c buffer.c charset.c cmdcmds.c cmdline.c csearch.c \
	digraph.c edit.c fileio.c getchar.c help.c linefunc.c main.c mark.c \
	memfile.c memline.c message.c misccmds.c normal.c ops.c option.c \
	pathdef.c quickfix.c regexp.c regsub.c screen.c search.c tables.c \
	tag.c term.c undo.c unix.c version.c window.c

DOCS=	vim_40.txt vim_ami.txt vim_arch.txt vim_diff.txt vim_digr.txt \
	vim_dos.txt vim_gui.txt vim_help.txt vim_idx.txt vim_kcc.txt \
	vim_mac.txt vim_menu.txt vim_mint.txt vim_os2.txt vim_ref.txt \
	vim_rlh.txt vim_tips.txt vim_unix.txt vim_w32.txt vim_win.txt \
	vim_tags

# I haven't tried this, but it's a start...
# Use the following to build Motif vim:
#SRCS+=	gui.c gui_motif.c gui_x11.c pty_openbsd.c
#LDADD+=	-L/usr/local/LessTiff/lib -L/usr/local/X11R6.1/lib -lXm -lXt -lX11 -lutil
#CFLAGS+=	-I/usr/local/LessTiff/include -I/usr/local/X11R6.1/include -DHAVE_X11 -DUSE_GUI_MOTIF
# Use the following to build Athena vim:
#SRCS+=	gui.c gui_athena.c gui_x11.c pty_openbsd.c gui_at_sb.c
#LDADD+=	-L/some/where/X11/lib -lXaw -lXt -lXmu -lX11 -lutil
#CFLAGS+=	-I/some/where/X11/include -DHAVE_X11 -DUSE_GUI_ATHENA

CLEANFILES+=	mkcmdtab mkcmdtab.o cmdtab.h

.depend cmdline.o: cmdtab.h

cmdtab.h: mkcmdtab cmdtab.tab
	${.OBJDIR}/mkcmdtab ${.CURDIR}/cmdtab.tab ${.OBJDIR}/cmdtab.h

mkcmdtab: mkcmdtab.o
	${CC} mkcmdtab.o -o mkcmdtab

afterinstall:
	${INSTALL} -d -m 555 -o ${BINOWN} -g ${BINGRP} ${DESTDIR}/usr/share/vim
	cd ${.CURDIR}/doc; for i in ${DOCS}; do \
		cmp -s $$i ${DESTDIR}/usr/share/vim/$$i || \
		${INSTALL} ${INSTALL_COPY} -m 444 -o ${BINOWN} -g ${BINGRP} $$i \
		${DESTDIR}/usr/share/vim; done

.include <bsd.prog.mk>
