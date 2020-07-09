#	$OpenBSD: Makefile.inc,v 1.1 2020/06/25 04:00:58 drahn Exp $

# no jump tables in _dl_boot_bind()
CFLAGS += -fno-jump-tables
CFLAGS += -fPIC -msoft-float
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script

RELATIVE_RELOC=R_PPC64_RELATIVE

# Suppress DWARF2 warnings
DEBUG?= -gdwarf-4
