# Linker script for i386 go32 (DJGPP)

test -z "$ENTRY" && ENTRY=start
EXE=${CONSTRUCTING+${RELOCATING+-exe}}

# These are substituted in as variables in order to get '}' in a shell
# conditional expansion.
CTOR='.ctor : { *(.ctor) }'
DTOR='.dtor : { *(.dtor) }'

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}${EXE}")

ENTRY(${ENTRY})

SECTIONS
{
  .text ${RELOCATING+ ${TARGET_PAGE_SIZE}+SIZEOF_HEADERS} : {
    *(.text)
    ${RELOCATING+*(.gnu.linkonce.t*)}
    *(.const*)
    *(.ro*)
    ${RELOCATING+*(.gnu.linkonce.r*)}
    ${RELOCATING+etext  =  . ; _etext = . ;}
    ${RELOCATING+. = ALIGN(${SEGMENT_SIZE});}
  }
  .data ${RELOCATING+ ${DATA_ALIGNMENT}} : {
    ${RELOCATING+djgpp_first_ctor = . ;
    *(.ctor)
    djgpp_last_ctor = . ;}
    ${RELOCATING+djgpp_first_dtor = . ;
    *(.dtor)
    djgpp_last_dtor = . ;}
    *(.data)

    ${RELOCATING+*(.gcc_exc*)}
    ${RELOCATING+___EH_FRAME_BEGIN__ = . ;}
    ${RELOCATING+*(.eh_fram*)}
    ${RELOCATING+___EH_FRAME_END__ = . ;}
    ${RELOCATING+LONG(0);}

    ${RELOCATING+*(.gnu.linkonce.d*)}
    ${RELOCATING+edata  =  . ; _edata = . ;}
    ${RELOCATING+. = ALIGN(${SEGMENT_SIZE});}
  }
  ${CONSTRUCTING+${RELOCATING-$CTOR}}
  ${CONSTRUCTING+${RELOCATING-$DTOR}}
  .bss ${RELOCATING+ SIZEOF(.data) + ADDR(.data)} :
  { 					
    *(.bss)
    *(COMMON)
    ${RELOCATING+ end = . ; _end = . ;}
    ${RELOCATING+ . = ALIGN(${SEGMENT_SIZE});}
  }
}
EOF
