# This is totally made up, from the a29k stuff.  If you know better,
# tell us about it.
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}

MEMORY {
	text   	: ORIGIN = 0x1000000, LENGTH = 0x1000000
	talias 	: ORIGIN = 0x2000000, LENGTH = 0x1000000
	data	: ORIGIN = 0x3000000, LENGTH = 0x1000000
	mstack 	: ORIGIN = 0x4000000, LENGTH = 0x1000000
	rstack 	: ORIGIN = 0x5000000, LENGTH = 0x1000000
}
SECTIONS
{
  .text : {
    *(.text)
    ${RELOCATING+ etext  =  .;}
    ${CONSTRUCTING+ __CTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)}
    ${CONSTRUCTING+ *(.ctors)}
    ${CONSTRUCTING+ LONG(0)}
    ${CONSTRUCTING+ __CTOR_END__ = .;}
    ${CONSTRUCTING+ __DTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)}
    ${CONSTRUCTING+ *(.dtors)}
    ${CONSTRUCTING+ LONG(0)}
    ${CONSTRUCTING+ __DTOR_END__ = .;}
    *(.lit)
    *(.shdata)
  } ${RELOCATING+ > text}
  .shbss SIZEOF(.text) + ADDR(.text) :	{
    *(.shbss)
  } 
  .talias :	 { } ${RELOCATING+ > talias}
  .data  : {
    *(.data)
    ${RELOCATING+ edata  =  .};
  } ${RELOCATING+ > data}
  .bss   SIZEOF(.data) + ADDR(.data) :
  { 					
    ${RELOCATING+ __bss_start = .};
   *(.bss)
   *(COMMON)
     ${RELOCATING+ end = ALIGN(0x8)};
     ${RELOCATING+ _end = ALIGN(0x8)};
  } ${RELOCATING+ > data}
  .mstack  : { } ${RELOCATING+ > mstack}
  .rstack  : { } ${RELOCATING+ > rstack}
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
