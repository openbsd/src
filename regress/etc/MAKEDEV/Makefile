
# $OpenBSD: Makefile,v 1.9 2002/02/15 21:21:55 todd Exp $

MAKEDEVARCHS+=alpha amiga hp300 hppa i386 mac68k macppc mvme68k mvme88k
MAKEDEVARCHS+=mvmeppc sparc sparc64 sun3 vax

ETCSRCDIR?=	/usr/src/etc

# test separate targets, all, ramdisk/raminst, std, etc

REGRESSTARGETS=${MAKEDEVARCHS}
REGRESSSLOWTARGETS=${REGRESSTARGETS}

${MAKEDEVARCHS}:
	@echo "====> ${.TARGET}"
	@exec ${SUDO} rm -rf test.${.TARGET}
	@exec mkdir -p -m 700 test.${.TARGET}
	@exec cp ${ETCSRCDIR}/etc.${.TARGET}/MAKEDEV test.${.TARGET}
	@exec ${SUDO} chown root.wheel test.${.TARGET}
	@cd test.${.TARGET}; time ${SUDO} sh ./MAKEDEV all || true; \
	time ${SUDO} sh ./MAKEDEV all || true; \
	( ls -ln; ls -ln fd/; ls -ln altq/ ) | \
		awk '/^[bcps]/ {printf "%s %x.%x %x,%x%s\n",$$1,$$3,$$4,$$5,$$6,$$10} \
		     /^l/      {printf "%s %s.%s%s>%s\n",$$1,$$3,$$4,$$9,$$11}' | \
		sort +5 -n | \
		sed -e 's/rwx/7/g;s/rw-/6/g;s/r-x/5/g;s/r--/4/g' \
		    -e 's/-wx/3/g;s/-w-/2/g;s/--x/1/g;s/---/0/g' \
		    -e 's/^\([bcpsl]\)\([0-9][0-9]*\) /\2\1/' \
		> ../t1.${.TARGET}.out

clean:
	for f in ${MAKEDEVARCHS}; do rm -rf test.$$f; done
	rm -f ${CLEANFILES}

.PHONY: t1 t2 t3 t4 t5 t6 t7 regress ${MAKEDEVARCHS}
CLEANFILES+=t1.*.out

.include <bsd.regress.mk>
