SCRIPT_NAME=elf
ELFSIZE=64
TEMPLATE_NAME=elf32
OUTPUT_FORMAT="elf64-hppa"
MAXPAGESIZE=0x10000
ARCH=hppa
MACHINE=hppa2.0w
DATA_PLT=
DATA_NONEXEC_PLT=
ENTRY="__start"
GENERATE_SHLIB_SCRIPT=yes

TEXT_START_ADDR=0x10000

# We really want multiple .stub sections, one for each input .text section,
# but for now this is good enough.
OTHER_READONLY_SECTIONS="
  .PARISC.unwind ${RELOCATING-0} : { *(.PARISC.unwind) }"

# The PA64 ELF port treats .plt sections differently than most.  We also have
# to create a .opd section.  What most systems call the .got, we call the .dlt
OTHER_READWRITE_SECTIONS="
  .opd          ${RELOCATING-0} : { *(.opd) }
  ${RELOCATING+PROVIDE (__gp = .);}
  .plt          ${RELOCATING-0} : { *(.plt) }
  .dlt          ${RELOCATING-0} : { *(.dlt) }"

# The PA64 ELF port has two additional bss sections. huge bss and thread bss.
# Make sure they end up in the appropriate location.
OTHER_BSS_SECTIONS="
  .hbss         ${RELOCATING-0} : { *(.hbss) }
  .tbss         ${RELOCATING-0} : { *(.tbss) }
"

# HPs use .dlt where systems use .got.  Sigh.
OTHER_GOT_RELOC_SECTIONS="
  .rela.dlt     ${RELOCATING-0} : { *(.rela.dlt) }
  .rela.opd     ${RELOCATING-0} : { *(.rela.opd) }"

# .dynamic should be at the start of the .text segment.
TEXT_DYNAMIC=

. ${srcdir}/emulparams/elf_obsd.sh
