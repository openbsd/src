#	$OpenBSD: Makefile.boot,v 1.10 2010/07/19 19:46:43 espie Exp $
#
# a very simple makefile...
#
# You only want to use this if you aren't running OpenBSD.
#
# modify MACHINE, MACHINE_ARCH and MACHINE_CPU as appropriate
# for your target architecture
#

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

MACHINE=sun
MACHINE_ARCH=sparc
MACHINE_CPU=sparc
CFLAGS= -Iohash -I. -DTARGET_MACHINE=\"${MACHINE}\" -DTARGET_MACHINE_ARCH=\"${MACHINE_ARCH}\" -DTARGET_MACHINE_CPU=\"${MACHINE_CPU}\" -DMACHINE=\"${MACHINE}\" \
	-DMAKE_BOOTSTRAP -DNEED_FGETLN
LIBS= ohash/libohash.a

OBJ=arch.o buf.o compat.o cond.o dir.o for.o job.o main.o make.o \
    parse.o str.o suff.o targ.o var.o util.o error.o lowparse.o \
    varmodifiers.o memory.o cmd_exec.o timestamp.o parsevar.o \
    varname.o init.o

LIBOBJ=	lst.lib/lstAddNew.o lst.lib/lstAppend.o \
	lst.lib/lstConcat.o lst.lib/lstConcatDestroy.o lst.lib/lstDeQueue.o \
	lst.lib/lstDestroy.o lst.lib/lstDupl.o lst.lib/lstFindFrom.o \
	lst.lib/lstForEachFrom.o lst.lib/lstInit.o lst.lib/lstInsert.o \
	lst.lib/lstMember.o lst.lib/lstRemove.o lst.lib/lstReplace.o \
	lst.lib/lstSucc.o

bmake: varhashconsts.h ${OBJ} ${LIBOBJ}
#	@echo 'make of make and make.0 started.'
	${CC} ${CFLAGS} ${OBJ} ${LIBOBJ} -o bmake ${LIBS}
	@ls -l $@
#	nroff -h -man make.1 > make.0
#	@echo 'make of make and make.0 completed.'

GENOBJ= generate.o stats.o memory.o ohash/libohash.a

OHASHOBJ= ohash/ohash_create_entry.o ohash/ohash_delete.o ohash/ohash_do.o \
	ohash/ohash_entries.o ohash/ohash_enum.o ohash/ohash_init.o \
	ohash/ohash_interval.o ohash/ohash_lookup_interval.o \
	ohash/ohash_lookup_memory.o ohash/ohash_qlookup.o \
	ohash/ohash_qlookupi.o

ohash/libohash.a: ${OHASHOBJ}
	ar cq ohash/libohash.a ${OHASHOBJ}
	ranlib ohash/libohash.a

generate: ${GENOBJ}
	${CC} ${CFLAGS} ${GENOBJ} -o generate ${LIBS}

varhashconsts.h: generate
	./generate 1 77 > varhashconsts.h

clean:
	rm -f ${OBJ} ${LIBOBJ} ${PORTOBJ} ${GENOBJ} ${OHASHOBJ} bmake
	rm -f varhashconsts.h generate

