#!/bin/sh
#	$OpenBSD: moduli-gen.sh,v 1.5 2020/02/27 02:32:37 dtucker Exp $
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
	for i in 0 1;
		do ssh-keygen -M generate -O bits=${bits} /dev/stdout;
	done | gzip -9c >${moduli_sieved}.tmp && \
	mv ${moduli_sieved}.tmp ${moduli_sieved}
fi

lines=`gzip -dc ${moduli_sieved} | wc -l`
lines=`echo $lines`  # remove leading space

gzip -dc ${moduli_sieved} | \
    ssh-keygen -M screen -O checkpoint=${moduli_tested}.ckpt \
        -O lines=${lines} ${moduli_tested} && \
    mv ${objdir}/moduli.${bits}.tested ${srcdir}/moduli.${bits}

exit 0
