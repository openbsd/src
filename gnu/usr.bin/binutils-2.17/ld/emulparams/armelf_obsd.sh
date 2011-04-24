. ${srcdir}/emulparams/armelf.sh
. ${srcdir}/emulparams/elf_obsd.sh

MAXPAGESIZE=0x8000
TEXT_START_ADDR=0x00008000
TARGET2_TYPE=got-rel

unset EMBEDDED
