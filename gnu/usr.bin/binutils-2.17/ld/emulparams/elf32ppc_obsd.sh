. ${srcdir}/emulparams/elf32ppc.sh
. ${srcdir}/emulparams/elf_obsd.sh
TRAP=0x00000000 # gauranteed always illegal

# override these to put the padding *in* the output section
sdata_GOT=".got          ${RELOCATING-0} : SPECIAL {
    *(.got)
    ${RELOCATING+. = ALIGN(${MAXPAGESIZE}) + (. & (${MAXPAGESIZE} - 1));}
  }"
bss_PLT="
  .plt          ${RELOCATING-0} : SPECIAL {
    ${RELOCATING+. = ALIGN(${MAXPAGESIZE}) + (. & (${MAXPAGESIZE} - 1));}
    *(.plt)
    ${RELOCATING+. = ALIGN(${MAXPAGESIZE}) + (. & (${MAXPAGESIZE} - 1));}
  }"
