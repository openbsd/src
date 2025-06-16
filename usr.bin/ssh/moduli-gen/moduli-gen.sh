#!/bin/sh
#	$OpenBSD: moduli-gen.sh,v 1.6 2025/06/16 09:07:08 dtucker Exp $
#

srcdir="$1"
objdir="$2"
bits="$3"

moduli_sieved=${objdir}/moduli.${bits}.sieved.gz
moduli_tested=${objdir}/moduli.${bits}.tested
moduli_part=${srcdir}/moduli.${bits}

if [ -f ${moduli_part} ]; then
	exit 0
fi

mkdir -p ${objdir}

if [ ! -f ${moduli_sieved} ]; then
	ssh-keygen -M generate -O bits=${bits} /dev/stdout | \
	    gzip -9c >${moduli_sieved}.tmp && \
	mv ${moduli_sieved}.tmp ${moduli_sieved}
fi

lines=`gzip -dc ${moduli_sieved} | wc -l`
lines=`echo $lines`  # remove leading space

gzip -dc ${moduli_sieved} | \
    ssh-keygen -M screen -O checkpoint=${moduli_tested}.ckpt \
        -O lines=${lines} ${moduli_tested} && \
    mv ${objdir}/moduli.${bits}.tested ${srcdir}/moduli.${bits}

exit 0
