# Linker script for i386 go32 COFF.
# stolen from ian
test -z "$ENTRY" && ENTRY=_start
# These are substituted in as variables in order to get '}' in a shell
# conditional expansion.
INIT='.init : { *(.init) }'
FINI='.fini : { *(.fini) }'
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}

ENTRY(${ENTRY})

SECTIONS
{
  .text ${RELOCATING+ 0x10a8} : {
    ${RELOCATING+ *(.init)}
    *(.text)
    ${RELOCATING+ *(.fini)}
    ${RELOCATING+ etext  =  .};
  }
  .data ALIGN (0x1000) : {
    *(.data .data2)
	*(.ctor)
	*(.dtor)
    ${RELOCATING+ _edata  =  .};
  }
  .bss ${RELOCATING+ SIZEOF(.data) + ADDR(.data)} :
  { 					
    *(.bss)
    *(COMMON)
    ${RELOCATING+ end = .};
  }
  ${RELOCATING- ${INIT}}
  ${RELOCATING- ${FINI}}
  .stab  0 ${RELOCATING+(NOLOAD)} : 
  {
    [ .stab ]
  }
  .stabstr  0 ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }
}
EOF
