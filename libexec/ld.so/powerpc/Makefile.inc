#	$OpenBSD: Makefile.inc,v 1.5 2019/10/20 03:44:49 guenther Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
CFLAGS += -fPIC -msoft-float
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_PPC_RELATIVE
