
CTOR=".ctors ${CONSTRUCTING-0} : 
  {
    ${CONSTRUCTING+ __CTOR_LIST__ = .; }
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */

    KEEP (*crtbegin.o(.ctors))

    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */

    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    ${CONSTRUCTING+ __CTOR_END__ = .; }
  } ${RELOCATING+ > ${DATA_MEMORY}}"

DTOR="  .dtors	${CONSTRUCTING-0} :
  {
    ${CONSTRUCTING+ __DTOR_LIST__ = .; }
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    ${CONSTRUCTING+ __DTOR_END__ = .; }
  } ${RELOCATING+ > ${DATA_MEMORY}}"

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

MEMORY
{
  text ${TEXT_DEF_SECTION} : ORIGIN = ${TEXT_START_ADDR}, LENGTH = ${TEXT_SIZE}
  data ${DATA_DEF_SECTION} : ORIGIN = ${DATA_START_ADDR}, LENGTH = ${DATA_SIZE}
  emem ${EMEM_DEF_SECTION} : ORIGIN = ${EMEM_START_ADDR}, LENGTH = ${EMEM_SIZE}
  eit			   : ORIGIN = ${EIT_START_ADDR},  LENGTH = ${EIT_SIZE}
}

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  ${TEXT_DYNAMIC+${DYNAMIC}}
  .hash			${RELOCATING-0} : { *(.hash) }
  .dynsym		${RELOCATING-0} : { *(.dynsym) }
  .dynstr		${RELOCATING-0} : { *(.dynstr) }
  .gnu.version		${RELOCATING-0} : { *(.gnu.version) }
  .gnu.version_d	${RELOCATING-0} : { *(.gnu.version_d) }
  .gnu.version_r	${RELOCATING-0} : { *(.gnu.version_r) }

  .rela.text		${RELOCATING-0} : { *(.rela.text) *(.rela.gnu.linkonce.t*) }
  .rela.data		${RELOCATING-0} : { *(.rela.data) *(.rela.gnu.linkonce.d*) }
  .rela.rodata		${RELOCATING-0} : { *(.rela.rodata) *(.rela.gnu.linkonce.r*) }
  .rela.stext		${RELOCATING-0} : { *(.rela.stest) }
  .rela.etext		${RELOCATING-0} : { *(.rela.etest) }
  .rela.sdata		${RELOCATING-0} : { *(.rela.sdata) }
  .rela.edata		${RELOCATING-0} : { *(.rela.edata) }
  .rela.eit_v		${RELOCATING-0} : { *(.rela.eit_v) }
  .rela.sbss		${RELOCATING-0} : { *(.rela.sbss) }
  .rela.ebss		${RELOCATING-0} : { *(.rela.ebss) }
  .rela.srodata		${RELOCATING-0} : { *(.rela.srodata) }
  .rela.erodata		${RELOCATING-0} : { *(.rela.erodata) }
  .rela.got		${RELOCATING-0} : { *(.rela.got) }
  .rela.ctors		${RELOCATING-0} : { *(.rela.ctors) }
  .rela.dtors		${RELOCATING-0} : { *(.rela.dtors) }
  .rela.init		${RELOCATING-0} : { *(.rela.init) }
  .rela.fini		${RELOCATING-0} : { *(.rela.fini) }
  .rela.bss		${RELOCATING-0} : { *(.rela.bss) }
  .rela.plt		${RELOCATING-0} : { *(.rela.plt) }

  .rel.data		${RELOCATING-0} : { *(.rel.data) *(.rel.gnu.linkonce.d*) }
  .rel.rodata		${RELOCATING-0} : { *(.rel.rodata) *(.rel.gnu.linkonce.r*) }
  .rel.stext		${RELOCATING-0} : { *(.rel.stest) }
  .rel.etext		${RELOCATING-0} : { *(.rel.etest) }
  .rel.sdata		${RELOCATING-0} : { *(.rel.sdata) }
  .rel.edata		${RELOCATING-0} : { *(.rel.edata) }
  .rel.sbss		${RELOCATING-0} : { *(.rel.sbss) }
  .rel.ebss		${RELOCATING-0} : { *(.rel.ebss) }
  .rel.eit_v		${RELOCATING-0} : { *(.rel.eit_v) }
  .rel.srodata		${RELOCATING-0} : { *(.rel.srodata) }
  .rel.erodata		${RELOCATING-0} : { *(.rel.erodata) }
  .rel.got		${RELOCATING-0} : { *(.rel.got) }
  .rel.ctors		${RELOCATING-0} : { *(.rel.ctors) }
  .rel.dtors		${RELOCATING-0} : { *(.rel.dtors) }
  .rel.init		${RELOCATING-0} : { *(.rel.init) }
  .rel.fini		${RELOCATING-0} : { *(.rel.fini) }
  .rel.bss		${RELOCATING-0} : { *(.rel.bss) }
  .rel.plt		${RELOCATING-0} : { *(.rel.plt) }

  .init			${RELOCATING-0} : { *(.init) } =${NOP-0}
  ${DATA_PLT-${PLT}}

  /* Internal text space */
  .stext	${RELOCATING-0} : { *(.stext) }		${RELOCATING+ > text}

  /* Internal text space or external memory */
  .text :
  {
    *(.text)
    *(.gnu.linkonce.t*)
    *(.init)
    *(.fini)
    ${RELOCATING+ _etext = . ; }
  } ${RELOCATING+ > ${TEXT_MEMORY}}

  /* Internal data space */
  .srodata	${RELOCATING-0} : { *(.srodata) }	${RELOCATING+ > data}
  .sdata	${RELOCATING-0} : { *(.sdata) }		${RELOCATING+ > data}

  /* Internal data space or external memory */
  .rodata	${RELOCATING-0} : { *(.rodata) }	${RELOCATING+ > ${DATA_MEMORY}}

  /* C++ exception support.  */
  .eh_frame	${RELOCATING-0} : { KEEP (*(.eh_frame)) }	${RELOCATING+ > ${DATA_MEMORY}}
  .gcc_except_table ${RELOCATING-0} : { *(.gcc_except_table) }	${RELOCATING+ > ${DATA_MEMORY}}

  ${RELOCATING+${CTOR}}
  ${RELOCATING+${DTOR}}

  .data		${RELOCATING-0} :
  {
    *(.data)
    *(.gnu.linkonce.d*)
    ${CONSTRUCTING+CONSTRUCTORS}
    ${RELOCATING+ _edata = . ; }
  } ${RELOCATING+ > ${DATA_MEMORY}}

  /* External memory */
  .etext	${RELOCATING-0} :
  {
    ${RELOCATING+ PROVIDE (__etext_start = .) ; }
    *(.etext)
    ${RELOCATING+ PROVIDE (__etext_end = .) ; }
  } ${RELOCATING+ > emem}

  .erodata	${RELOCATING-0} : { *(.erodata) }	${RELOCATING+ > emem}
  .edata	${RELOCATING-0} : { *(.edata) }		${RELOCATING+ > emem}

  .sbss		${RELOCATING-0} :
  {
    ${RELOCATING+ PROVIDE (__sbss_start = .) ; }
    *(.sbss)
    ${RELOCATING+ PROVIDE (__sbss_end = .) ;  }
  } ${RELOCATING+ > data}

  .ebss		${RELOCATING-0} :
  {
    ${RELOCATING+ PROVIDE (__ebss_start = .) ; }
    *(.ebss)
    ${RELOCATING+ PROVIDE (__ebss_end = .) ;  }
  } ${RELOCATING+ > data}

  .bss		${RELOCATING-0} :
  {
    ${RELOCATING+ PROVIDE (__bss_start = .) ; }
    *(.bss)
    *(COMMON)
    ${RELOCATING+ PROVIDE (__bss_end = .) ; }
    ${RELOCATING+ _end = . ;  }
  } ${RELOCATING+ > ${DATA_MEMORY}}

  .eit_v	${RELOCATING-0} :
  {
    ${RELOCATING+ PROVIDE (__eit_start = .) ; }
    *(.eit_v)
    ${RELOCATING+ PROVIDE (__eit_end = .) ; }
  } ${RELOCATING+ > eit}

  /* Stabs debugging sections.  */
  .stab		 0 : { *(.stab) }
  .stabstr	 0 : { *(.stabstr) }
  .stab.excl	 0 : { *(.stab.excl) }
  .stab.exclstr	 0 : { *(.stab.exclstr) }
  .stab.index	 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment	 0 : { *(.comment) }

  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */

  /* DWARF 1 */
  .debug	 0 : { *(.debug) }
  .line		 0 : { *(.line) }

  /* GNU DWARF 1 extensions */
  .debug_srcinfo 0 : { *(.debug_srcinfo) }
  .debug_sfnames 0 : { *(.debug_sfnames) }

  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }

  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }

  PROVIDE (__stack = ${STACK_START_ADDR});
}
EOF
