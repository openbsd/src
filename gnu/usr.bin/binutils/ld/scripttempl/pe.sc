# Linker script for PE.
# These are substituted in as variables in order to get '}' in a shell
# conditional expansion.
INIT='.init : { *(.init) }'
FINI='.fini : { *(.fini) }'
cat <<EOF
OUTPUT_FORMAT(${OUTPUT_FORMAT})
${LIB_SEARCH_DIRS}

ENTRY(_mainCRTStartup)

SECTIONS
{

  .text ${RELOCATING+ __image_base__ + __section_alignment__ } : 
	{
	    ${RELOCATING+ *(.init);}
	    *(.text)
	    ${CONSTRUCTING+ ___CTOR_LIST__ = .; __CTOR_LIST__ = . ; 
		              LONG (-1); *(.ctors); *(.ctor); LONG (0); }
            ${CONSTRUCTING+ ___DTOR_LIST__ = .; __DTOR_LIST__ = . ; 
				LONG (-1); *(.dtors); *(.dtor);  LONG (0); }
	    ${RELOCATING+ *(.fini);}
	    ${RELOCATING+ etext  =  .};
	  }

  .bss BLOCK(__section_alignment__)  :
	{
	__bss_start__ = . ;
	*(.bss) ;
	*(COMMON);
	__bss_end__ = . ;
	${RELOCATING+ end =  .};
	}
  .data BLOCK(__section_alignment__) : 
	{
	__data_start__ = . ; 
	*(.data);
	*(.data2);
 	__data_end__ = . ; 
	}

  .rdata BLOCK(__section_alignment__) :
  { 					
    *(.rdata)
    ;
  }



  .edata BLOCK(__section_alignment__) :   { 					
    *(.edata)   ;
  }

  .junk BLOCK(__section_alignment__) : {
    *(.debug\$S)
    *(.debug\$T)
    *(.debug\$F)
    *(.drectve)
    ;
  }

  .idata BLOCK(__section_alignment__) :
  { 					
    *(.idata\$2)
    *(.idata\$3)
    *(.idata\$4)
    *(.idata\$5)
    *(.idata\$6)
    *(.idata\$7)
    ;
  }
  .CRT BLOCK(__section_alignment__) :
  { 					
    *(.CRT\$XCA)
    *(.CRT\$XCC)
    *(.CRT\$XCZ)
    *(.CRT\$XIA)
    *(.CRT\$XIC)
    *(.CRT\$XIZ)
    *(.CRT\$XLA)
    *(.CRT\$XLZ)
    *(.CRT\$XPA)
    *(.CRT\$XPX)
    *(.CRT\$XPZ)
    *(.CRT\$XTA)
    *(.CRT\$XTZ)
    ;
  }
  .rsrc BLOCK(__section_alignment__) :
  { 					
    *(.rsrc\$01)
    *(.rsrc\$02)
    ;
  }
  .junk BLOCK(__section_alignment__) :
  { 					
    *(.debug\$S)
    *(.debug\$T)
    *(.debug\$F)
    *(.drectve)
    ;
  }

  .stab BLOCK(__section_alignment__)  ${RELOCATING+(NOLOAD)} : 
  {
    [ .stab ]
  }

  .stabstr BLOCK(__section_alignment__) ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }


  .reloc BLOCK(__section_alignment__) :
  { 					
    *(.reloc)
    ;
  }


}
EOF
