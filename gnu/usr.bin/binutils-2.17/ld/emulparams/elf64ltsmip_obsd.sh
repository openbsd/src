. ${srcdir}/emulparams/elf64ltsmip.sh
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x4000
TEXT_START_ADDR="0x10000000"
. ${srcdir}/emulparams/elf_obsd.sh
SCRIPT_NAME=elf_obsd
# XXX causes GOT oflows
NO_PAD_CDTOR=y
NOP=0x00000000
TRAP=0xefefefef
