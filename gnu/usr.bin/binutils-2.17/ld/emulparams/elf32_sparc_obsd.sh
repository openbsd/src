. ${srcdir}/emulparams/elf32_sparc.sh
#override MAXPAGESIZE to avoid cache aliasing.
MAXPAGESIZE=0x100000
. ${srcdir}/emulparams/elf_obsd.sh
