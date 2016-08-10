# If you change this file, please alsolook at files which source this one:
# shlelf_obsd.sh

. ${srcdir}/emulparams/shelf.sh

OUTPUT_FORMAT="elf32-sh-obsd"
TEXT_START_ADDR=0x400000
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x1000

DATA_START_SYMBOLS='__data_start = . ;';

ENTRY=__start

unset EMBEDDED
unset OTHER_SECTIONS

. ${srcdir}/emulparams/elf_obsd.sh

# No nx bit, so don't bother to pad between .text and .rodata
unset PAD_RO
