cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

MEMORY {
	rom   : o = 0x0000, l = 0x7fe0 
	duart : o = 0x7fe0, l = 16
	ram   : o = 0x8000, l = 28k
	topram : o = 0x8000+28k, l = 1k
	hmsram : o = 0xfb80, l = 512
	}

SECTIONS 				
{ 					
.text :
	{ 					
	  *(.text) 				
	  *(.strings)
   	 ${RELOCATING+ _etext = . ; }
	} ${RELOCATING+ > ram}
.tors   : {
	___ctors = . ;
	*(.ctors)
	___ctors_end = . ;
	___dtors = . ;
	*(.dtors)
	___dtors_end = . ;
}  ${RELOCATING+ > ram}
.data  :
	{
	*(.data)
	${RELOCATING+ _edata = . ; }
	} ${RELOCATING+ > ram}
.bss  :
	{
	${RELOCATING+ _bss_start = . ;}
	*(.bss)
	*(COMMON)
	${RELOCATING+ _end = . ;  }
	} ${RELOCATING+ >ram}
.stack : 
	{
	${RELOCATING+ _stack = . ; }
	*(.stack)
	} ${RELOCATING+ > topram}
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




