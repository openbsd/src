# AIX linker script.
# AIX always uses shared libraries.  The section VMA appears to be
# unimportant.  The native linker aligns the sections on boundaries
# specified by the -H option.
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})
${RELOCATING+${LIB_SEARCH_DIRS}}
ENTRY(__start)
SECTIONS
{
  .pad 0 : { *(.pad) }
  .text ${RELOCATING-0} : {
    ${RELOCATING+PROVIDE (_text = .);}
    *(.text)
    *(.pr)
    *(.ro)
    *(.db)
    *(.gl)
    *(.xo)
    *(.ti)
    *(.tb)
    ${RELOCATING+PROVIDE (_etext = .);}
  }
  .data 0 : {
    ${RELOCATING+PROVIDE (_data = .);}
    *(.data)
    *(.rw)
    *(.sv)
    *(.ua)
    . = ALIGN(4);
    ${CONSTRUCTING+CONSTRUCTORS}
    *(.ds)
    *(.tc0)
    *(.tc)
    *(.td)
    ${RELOCATING+PROVIDE (_edata = .);}
  }
  .bss : {
    *(.bss)
    *(.bs)
    *(.uc)
    *(COMMON)
    ${RELOCATING+PROVIDE (_end = .);}
    ${RELOCATING+PROVIDE (end = .);}
  }
  .loader 0 : {
    *(.loader)
  }
  .debug 0 : {
    *(.debug)
  }
}
EOF
