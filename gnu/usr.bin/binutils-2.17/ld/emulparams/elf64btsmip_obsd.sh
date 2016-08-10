. ${srcdir}/emulparams/elf64btsmip.sh
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x4000
TEXT_START_ADDR="0x10000000"
. ${srcdir}/emulparams/elf_obsd.sh
# XXX causes GOT oflows
NO_PAD_CDTOR=y
