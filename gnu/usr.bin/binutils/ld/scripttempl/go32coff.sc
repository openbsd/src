# Linker script for 386 go32
# DJ Delorie (dj@ctron.com)

test -z "$ENTRY" && ENTRY=start
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}

ENTRY(${ENTRY})

SECTIONS
{
  .text ${RELOCATING+ 0x1000+SIZEOF_HEADERS} : {
    *(.text)
    ${RELOCATING+ etext  =  . ; _etext = .};
    ${RELOCATING+ . = ALIGN(0x200);}
  }
  .data ${RELOCATING+ ${DATA_ALIGNMENT}} : {
    ${RELOCATING+ *(.ctor)}
    ${RELOCATING+ *(.dtor)}
    *(.data)
    ${RELOCATING+ edata  =  . ; _edata = .};
    ${RELOCATING+ . = ALIGN(0x200);}
  }
  .bss ${RELOCATING+ SIZEOF(.data) + ADDR(.data)} :
  { 					
    *(.bss)
    *(COMMON)
    ${RELOCATING+ end = . ; _end = .};
    ${RELOCATING+ . = ALIGN(0x200);}
  }
}
EOF
