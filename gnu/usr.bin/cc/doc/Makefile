# $FreeBSD: src/gnu/usr.bin/cc/doc/Makefile,v 1.14.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.1.1.1 2009/10/15 20:50:20 robert Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR}/doc ${GCCDIR}/doc/include

INFOS=	gcc cpp gccint cppinternals
INFOF=	${INFOS:S/$/.info/}

INFOSECTION=    "Gcc Documentation"
INFOENTRY_gcc=  "* gcc: (gcc).			The GNU C compiler."
INFOENTRY_cpp=  "* cpp: (cpp).			The GNU C pre-processor."
INFOENTRY_gccint= "* gccint: (gccint).		The GNU compiler family internal documentation."
INFOENTRY_cppinternals= "* cppinternals: (cppinternals).		The GNU compiler preprocessor internal documentation."

gcc.info: gcc.texi gcc-common.texi gcc-vers.texi frontends.texi		\
	 standards.texi invoke.texi extend.texi md.texi objc.texi	\
	 gcov.texi trouble.texi bugreport.texi service.texi		\
	 contribute.texi compat.texi funding.texi gnu.texi gpl.texi	\
	 fdl.texi contrib.texi cppenv.texi cppopts.texi			\
	 implement-c.texi

gccint.info: gccint.texi gcc-common.texi gcc-vers.texi			\
	 contribute.texi makefile.texi configterms.texi options.texi	\
	 portability.texi interface.texi passes.texi c-tree.texi	\
	 rtl.texi md.texi tm.texi hostconfig.texi fragments.texi	\
	 configfiles.texi collect2.texi headerdirs.texi funding.texi	\
	 gnu.texi gpl.texi fdl.texi contrib.texi languages.texi		\
	 sourcebuild.texi gty.texi libgcc.texi cfg.texi tree-ssa.texi	\
	 loop.texi

cpp.info: cpp.texi fdl.texi cppenv.texi cppopts.texi gcc-common.texi \
	gcc-vers.texi

cppinternals.info:  cppinternals.texi gcc-common.texi gcc-vers.texi

MAKEINFOFLAGS+= -I ${GCCDIR}/doc -I ${GCCDIR}/doc/include

gcc-vers.texi:
	(echo "@set version-GCC $(BASEVER)"; \
	if [ "$(DEVPHASE)" = "experimental" ]; then \
		echo "@set DEVELOPMENT"; \
	else \
		echo "@clear DEVELOPMENT"; \
	fi; \
	echo "@set srcdir $(BSDSRCDIR)/gnu/lib/libiberty/src" ) > ${.TARGET}

CLEANFILES= ${INFOF} gcc-vers.texi

.for x in ${INFOS}
${x}.info: ${x}.texi
	makeinfo --no-split -I ${.CURDIR} -I ${GCCDIR}/doc -I ${GCCDIR}/doc/include \
		${x}.texi -o ${.TARGET}
.endfor
	
all:	${INFOF}

clean: _SUBDIRUSE
	rm -f ${CLEANFILES}

depend:
	# Nothing here so far...

install: ${INFOF}
.for x in ${INFOS}
	install-info --quiet --section=${INFOSECTION} --entry=${INFOENTRY_${x}} \
		${x}.info ${DESTDIR}/usr/share/info/dir
.endfor
	${INSTALL} -o ${BINOWN} -g ${LOCALEGRP} -m ${NONBINMODE} ${.ALLSRC} \
		${DESTDIR}/usr/share/info

.include <bsd.prog.mk>
