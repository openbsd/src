#	$OpenBSD: Makefile,v 1.2 2013/10/14 02:57:59 dtucker Exp $

.include <bsd.own.mk>

all:	moduli

# The larger ones will take many days, so if you're going to regen them run
# it in a tmux session or something.  The checkpoints should make it safe
# to stop and restart.
DHSIZE=1024 1536 2048 3072 4096 6144 7680 8192

.for bits in ${DHSIZE}
MODULI_PARTS+=moduli.${bits}

moduli.${bits}:
	sh ${.CURDIR}/moduli-gen.sh ${.CURDIR} ${.CURDIR}/work ${bits}
.endfor

moduli:	${MODULI_PARTS}
	( \
		echo -n '#    $$Open'; echo 'BSD$$'; \
		echo '# Time Type Tests Tries Size Generator Modulus'; \
		cat ${MODULI_PARTS} \
	) > moduli

update-moduli:
	cp moduli ${BSDSRCDIR}/etc/moduli

clean:
.for bits in ${DHSIZE}
	rm -f ${.OBJDIR}/moduli.${bits}.sieved.gz
.endfor
