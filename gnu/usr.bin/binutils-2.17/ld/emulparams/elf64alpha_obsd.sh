. ${srcdir}/emulparams/elf64alpha.sh
# XXX for now, until we can support late PLT placement
unset PLT
unset TEXT_PLT
DATA_PLT=
. ${srcdir}/emulparams/elf_obsd.sh
ENTRY=__start
