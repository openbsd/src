. ${srcdir}/emulparams/elf_i386.sh
. ${srcdir}/emulparams/elf_obsd.sh

if test "${LD_FLAG#"${LD_FLAG%pie}"}" = "pie"; then
  TEXT_START_ADDR=0x0
  if test "${LD_FLAG%%(cpie|pie)}" = "Z"; then
    RODATA_PADSIZE=${MAXPAGESIZE}
  else
    RODATA_PADSIZE=0x20000000
  fi
else
  if test "${LD_FLAG%%(cpie|pie)}" = "Z"; then
    TEXT_START_ADDR=0x08048000
    RODATA_PADSIZE=${MAXPAGESIZE}
  else
    TEXT_START_ADDR=0x1C000000
    RODATA_PADSIZE=0x20000000
  fi
fi

RODATA_ALIGN=". = ALIGN(${RODATA_PADSIZE})"
RODATA_ALIGN_ADD="${TEXT_START_ADDR}"

unset PAD_PLT
