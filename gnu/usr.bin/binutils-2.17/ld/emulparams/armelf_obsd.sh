. ${srcdir}/emulparams/armelf.sh

MAXPAGESIZE=0x8000
COMMONPAGESIZE=0x1000
TEXT_START_ADDR=0x00008000
TARGET2_TYPE=got-rel

unset EMBEDDED

. ${srcdir}/emulparams/elf_obsd.sh
