# $OpenBSD: Makefile,v 1.3 2001/09/07 17:04:31 todd Exp $

NOMAN=
NOPROG=
        
regress: t1

MAKEDEVARCHS+=alpha amiga hp300 hppa i386 mac68k macppc mvme68k mvme88k
MAKEDEVARCHS+=mvmeppc sparc sparc64 sun3 vax

ETCSRCDIR?=	/usr/src/etc

t1: ${MAKEDEVARCHS}

${MAKEDEVARCHS}:
	@echo "====> ${.TARGET}"
	@exec ${SUDO} rm -rf test.${.TARGET}
	@exec mkdir -p test.${.TARGET}
	@exec cp ${ETCSRCDIR}/etc.${.TARGET}/MAKEDEV test.${.TARGET}
	@exec ${SUDO} chown root.wheel test.${.TARGET}
	@cd test.${.TARGET}; time ${SUDO} sh ./MAKEDEV all || true; \
	( ls -l; ls -l fd/; ls -l altq/ ) | \
		awk '/^[bcps]/ {print $$1,$$2,$$3,$$4,$$5,$$6,$$10} \
		     /^l/      {print $$1,$$2,$$3,$$4,$$5,$$9,$$10,$$11}' |\
		sort +4 -n > ../t1.${.TARGET}.out

clean:
	for f in ${MAKEDEVARCHS}; do ${SUDO} rm -rf test.$$f; done

.PHONY: t1 t2 t3 t4 t5 t6 t7 regress ${MAKEDEVARCHS}
CLEANFILES+=t1.*.out

.include <bsd.prog.mk>
