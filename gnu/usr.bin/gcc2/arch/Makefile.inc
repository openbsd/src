#	$Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:39:15 deraadt Exp $

CFLAGS+= -I${.CURDIR}/../../common -I${.CURDIR}/.. -I${.CURDIR} # -DIN_GCC
.PATH:	${.CURDIR}/../../common ${.CURDIR}/..


.MAIN:	stamp-flags stamp-config stamp-codes stamp-output stamp-recog \
	stamp-emit stamp-extract stamp-peep stamp-attr stamp-attrtab \
	stamp-opinit


stamp-flags: ${.CURDIR}/md genflags ${.CURDIR}/../move-if-change
	./genflags ${.CURDIR}/md > tmp-flags.h
	${.CURDIR}/../move-if-change tmp-flags.h ${.CURDIR}/insn-flags.h
	touch stamp-flags

stamp-config: ${.CURDIR}/md genconfig ${.CURDIR}/../move-if-change
	./genconfig ${.CURDIR}/md > tmp-config.h
	${.CURDIR}/../move-if-change tmp-config.h ${.CURDIR}/insn-config.h
	touch stamp-config

stamp-codes: ${.CURDIR}/md gencodes ${.CURDIR}/../move-if-change
	./gencodes ${.CURDIR}/md > tmp-codes.h
	${.CURDIR}/../move-if-change tmp-codes.h ${.CURDIR}/insn-codes.h
	touch stamp-codes

stamp-output: ${.CURDIR}/md genoutput ${.CURDIR}/../move-if-change
	./genoutput ${.CURDIR}/md > tmp-output.c
	${.CURDIR}/../move-if-change tmp-output.c ${.CURDIR}/insn-output.c
	touch stamp-output

stamp-recog: ${.CURDIR}/md genrecog ${.CURDIR}/../move-if-change
	./genrecog ${.CURDIR}/md > tmp-recog.c
	${.CURDIR}/../move-if-change tmp-recog.c ${.CURDIR}/insn-recog.c
	touch stamp-recog

stamp-emit: ${.CURDIR}/md genemit ${.CURDIR}/../move-if-change
	./genemit ${.CURDIR}/md > tmp-emit.c
	${.CURDIR}/../move-if-change tmp-emit.c ${.CURDIR}/insn-emit.c
	touch stamp-emit

stamp-extract: ${.CURDIR}/md genextract ${.CURDIR}/../move-if-change
	./genextract ${.CURDIR}/md > tmp-extract.c
	${.CURDIR}/../move-if-change tmp-extract.c ${.CURDIR}/insn-extract.c
	touch stamp-extract

stamp-peep: ${.CURDIR}/md genpeep ${.CURDIR}/../move-if-change
	./genpeep ${.CURDIR}/md > tmp-peep.c
	${.CURDIR}/../move-if-change tmp-peep.c ${.CURDIR}/insn-peep.c
	touch stamp-peep

stamp-attr: ${.CURDIR}/md genattr ${.CURDIR}/../move-if-change
	./genattr ${.CURDIR}/md > tmp-attr.h
	${.CURDIR}/../move-if-change tmp-attr.h ${.CURDIR}/insn-attr.h
	touch stamp-attr

stamp-attrtab: ${.CURDIR}/md genattrtab ${.CURDIR}/../move-if-change
	./genattrtab ${.CURDIR}/md > tmp-attrtab.c
	${.CURDIR}/../move-if-change tmp-attrtab.c ${.CURDIR}/insn-attrtab.c
	touch stamp-attrtab

stamp-opinit: ${.CURDIR}/md genopinit ${.CURDIR}/../move-if-change
	./genopinit ${.CURDIR}/md > tmp-opinit.c
	${.CURDIR}/../move-if-change tmp-opinit.c ${.CURDIR}/insn-opinit.c
	touch stamp-opinit


genflags: genflags.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genconfig: genconfig.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

gencodes: gencodes.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genoutput: genoutput.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genrecog: genrecog.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genemit: genemit.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genextract: genextract.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genpeep: genpeep.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genattr: genattr.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genattrtab: genattrtab.o rtl.o print-rtl.o rtlanal.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}

genopinit: genopinit.o rtl.o obstack.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC}
