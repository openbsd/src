#!/bin/sh
#	$OpenBSD: moduli-gen.sh,v 1.3 2017/06/23 03:25:53 dtucker Exp $
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
	for i in 0 1; do ssh-keygen -b ${bits} -G /dev/stdout; done | \
	    gzip -9c >${moduli_sieved}.tmp && \
	mv ${moduli_sieved}.tmp ${moduli_sieved}
fi

lines=`gzip -dc ${moduli_sieved} | wc -l`

gzip -dc ${moduli_sieved} | \
    ssh-keygen -K ${moduli_tested}.ckpt -T ${moduli_tested} -J $lines && \
mv ${objdir}/moduli.${bits}.tested ${srcdir}/moduli.${bits}

exit 0
