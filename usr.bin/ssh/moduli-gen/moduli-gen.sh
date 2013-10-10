#!/bin/sh
#	$OpenBSD: moduli-gen.sh,v 1.1 2013/10/10 00:59:18 dtucker Exp $
#

srcdir="$1"
objdir="$2"
bits="$3"

moduli_sieved=${objdir}/moduli.${bits}.sieved.gz
moduli_tested=${objdir}/moduli.${bits}.tested
moduli_part=${srcdir}/moduli.${bits}

if [ ! -d ${objdir} ]; then
	mkdir ${objdir}
fi

if [ -f ${moduli_part} ]; then
	exit 0
fi

if [ ! -f ${moduli_sieved} ]; then
	ssh-keygen -b ${bits} -G /dev/stdout | \
	    gzip -9c >${moduli_sieved}.tmp && \
	mv ${moduli_sieved}.tmp ${moduli_sieved}
fi

gzip -dc ${moduli_sieved} | \
    ssh-keygen -K ${moduli_tested}.ckpt -T ${moduli_tested} && \
mv ${objdir}/moduli.${bits}.tested ${srcdir}/moduli.${bits}

exit 0
