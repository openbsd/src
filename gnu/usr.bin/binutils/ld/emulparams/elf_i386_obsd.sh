. ${srcdir}/emulparams/elf_i386.sh
. ${srcdir}/emulparams/elf_obsd.sh

if test "$LD_FLAG" = "Z"
then
TEXT_START_ADDR=0x08048000
else
TEXT_START_ADDR=0x1C000000
fi

RODATA_PADSIZE=0x20000000
RODATA_ALIGN=". = ALIGN(${RODATA_PADSIZE})"
RODATA_ALIGN_ADD="${TEXT_START_ADDR}"

unset PAD_PLT
