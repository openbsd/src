# . ${srcdir}/emulparams/elf32ppc.sh
. ${srcdir}/emulparams/elf32ppccommon.sh
## # Yes, we want duplicate .got and .plt sections.  The linker chooses the
## # appropriate one magically in ppc_after_open
## DATA_GOT=
## SDATA_GOT=
## SEPARATE_GOTPLT=0
BSS_PLT=
## GOT=".got          ${RELOCATING-0} : SPECIAL { *(.got) }"
## PLT=".plt          ${RELOCATING-0} : SPECIAL { *(.plt) }"
## GOTPLT="${PLT}"
## OTHER_TEXT_SECTIONS="*(.glink)"
## EXTRA_EM_FILE=ppc32elf
. ${srcdir}/emulparams/elf_obsd.sh
