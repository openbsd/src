# If you change this file, please alsolook at files which source this one:
# shlelf_obsd.sh

. ${srcdir}/emulparams/shelf.sh
. ${srcdir}/emulparams/elf_obsd.sh

OUTPUT_FORMAT="elf32-sh-obsd"
TEXT_START_ADDR=0x400000
MAXPAGESIZE=0x10000

DATA_START_SYMBOLS='__data_start = . ;';

ENTRY=_start

unset EMBEDDED
unset OTHER_SECTIONS
