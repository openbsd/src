#
# Unusual variables checked by this code:
#	NOP - two byte opcode for no-op (defaults to 0)
#	DATA_ADDR - if end-of-text-plus-one-page isn't right for data start
#	OTHER_READONLY_SECTIONS - other than .text .init .ctors .rodata ...
#		(e.g., .PARISC.milli)
#	OTHER_READWRITE_SECTIONS - other than .data .bss .sdata ...
#		(e.g., .PARISC.global)
#	OTHER_SECTIONS - at the end
#	EXECUTABLE_SYMBOLS - symbols that must be defined for an
#		executable (e.g., _DYNAMIC_LINK)
#	TEXT_START_SYMBOLS - symbols that appear at the start of the
#		.text section.
#	DATA_START_SYMBOLS - symbols that appear at the start of the
#		.data section.
#	OTHER_BSS_SYMBOLS - symbols that appear at the start of the
#		.bss section besides __bss_start.
#	DATA_PLT - .plt should be in data segment, not text segment.
#
# When adding sections, do note that the names of some sections are used
# when specifying the start address of the next.
#
test -z "$ENTRY" && ENTRY=_start
test -z "${BIG_OUTPUT_FORMAT}" && BIG_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test -z "${LITTLE_OUTPUT_FORMAT}" && LITTLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test "$LD_FLAG" = "N" && DATA_ADDR=.
INTERP=".interp   ${RELOCATING-0} : { *(.interp) 	}"
PLT=".plt    ${RELOCATING-0} : { *(.plt)	}"
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}", "${BIG_OUTPUT_FORMAT}",
	      "${LITTLE_OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})
ENTRY(${ENTRY})

${RELOCATING+${LIB_SEARCH_DIRS}}
${RELOCATING+/* Do we need any of these for elf?
   __DYNAMIC = 0; ${STACKZERO+${STACKZERO}} ${SHLIB_PATH+${SHLIB_PATH}}  */}
${RELOCATING+${EXECUTABLE_SYMBOLS}}
${RELOCATING- /* For some reason, the Solaris linker makes bad executables
  if gld -r is used and the intermediate file has sections starting
  at non-zero addresses.  Could be a Solaris ld bug, could be a GNU ld
  bug.  But for now assigning the zero vmas works.  */}
SECTIONS
{
  /* Read-only sections, merged into text segment: */
  ${CREATE_SHLIB-${RELOCATING+. = ${TEXT_START_ADDR} + SIZEOF_HEADERS;}}
  ${CREATE_SHLIB+${RELOCATING+. = SIZEOF_HEADERS;}}
  ${CREATE_SHLIB-${INTERP}}
  .hash        ${RELOCATING-0} : { *(.hash)		}
  .dynsym      ${RELOCATING-0} : { *(.dynsym)		}
  .dynstr      ${RELOCATING-0} : { *(.dynstr)		}
  .rel.text    ${RELOCATING-0} : { *(.rel.text)		}
  .rela.text   ${RELOCATING-0} : { *(.rela.text) 	}
  .rel.data    ${RELOCATING-0} : { *(.rel.data)		}
  .rela.data   ${RELOCATING-0} : { *(.rela.data) 	}
  .rel.rodata  ${RELOCATING-0} : { *(.rel.rodata) 	}
  .rela.rodata ${RELOCATING-0} : { *(.rela.rodata) 	}
  .rel.got     ${RELOCATING-0} : { *(.rel.got)		}
  .rela.got    ${RELOCATING-0} : { *(.rela.got)		}
  .rel.got1    ${RELOCATING-0} : { *(.rel.got1)		}
  .rela.got1   ${RELOCATING-0} : { *(.rela.got1)	}
  .rel.got2    ${RELOCATING-0} : { *(.rel.got2)		}
  .rela.got2   ${RELOCATING-0} : { *(.rela.got2)	}
  .rel.ctors   ${RELOCATING-0} : { *(.rel.ctors)	}
  .rela.ctors  ${RELOCATING-0} : { *(.rela.ctors)	}
  .rel.dtors   ${RELOCATING-0} : { *(.rel.dtors)	}
  .rela.dtors  ${RELOCATING-0} : { *(.rela.dtors)	}
  .rel.init    ${RELOCATING-0} : { *(.rel.init)	}
  .rela.init   ${RELOCATING-0} : { *(.rela.init)	}
  .rel.fini    ${RELOCATING-0} : { *(.rel.fini)	}
  .rela.fini   ${RELOCATING-0} : { *(.rela.fini)	}
  .rel.bss     ${RELOCATING-0} : { *(.rel.bss)		}
  .rela.bss    ${RELOCATING-0} : { *(.rela.bss)		}
  .rel.plt     ${RELOCATING-0} : { *(.rel.plt)		}
  .rela.plt    ${RELOCATING-0} : { *(.rela.plt)		}
  .init        ${RELOCATING-0} : { *(.init)	} =${NOP-0}
  ${DATA_PLT-${PLT}}
  .text    ${RELOCATING-0} :
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    *(.text)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  } =${NOP-0}
  ${RELOCATING+_etext = .;}
  ${RELOCATING+PROVIDE (etext = .);}
  .fini    ${RELOCATING-0} : { *(.fini)    } =${NOP-0}
  .rodata  ${RELOCATING-0} : { *(.rodata)  }
  .rodata1 ${RELOCATING-0} : { *(.rodata1) }
  ${RELOCATING+${OTHER_READONLY_SECTIONS}}

  /* Read-write section, merged into data segment: */
  ${RELOCATING+. = ${DATA_ADDR- ALIGN(8) + ${MAXPAGESIZE}};}
  .data  ${RELOCATING-0} :
  {
    ${RELOCATING+${DATA_START_SYMBOLS}}
    *(.data)
    ${CONSTRUCTING+CONSTRUCTORS}
  }
  .data1 ${RELOCATING-0} : { *(.data1) }
  ${RELOCATING+${OTHER_READWRITE_SECTIONS}}

  ${RELOCATING+_GOT1_START_ = .;}
  .got1 ${RELOCATING-0} :  { *(.got1) }
  ${RELOCATING+_GOT1_END_ = .;}

  .dynamic     ${RELOCATING-0} : { *(.dynamic) }

  /* Put .ctors and .dtors next to the .got2 section, so that the pointers
     get relocated with -mrelocatable. Also put in the .fixup pointers.  */

  ${RELOCATING+_GOT2_START_ = .;}
  .got2  ${RELOCATING-0} :  { *(.got2) }

  ${RELOCATING+__CTOR_LIST__ = .;}
  .ctors ${RELOCATING-0} : { *(.ctors) }
  ${RELOCATING+__CTOR_END__ = .;}

  ${RELOCATING+__DTOR_LIST__ = .;}
  .dtors ${RELOCATING-0} : { *(.dtors) }
  ${RELOCATING+__DTOR_END__ = .;}

  ${RELOCATING+_FIXUP_START_ = .;}
  .fixup ${RELOCATING-0} : { *(.fixup) }
  ${RELOCATING+_FIXUP_END_ = .;}
  ${RELOCATING+_GOT2_END_ = .;}

  ${RELOCATING+_GOT_START_ = .;}
  ${RELOCATING+_GLOBAL_OFFSET_TABLE_ = . + 32768;}
  ${RELOCATING+_SDA_BASE_ = . + 32768;}
  .got         ${RELOCATING-0} : { *(.got.plt) *(.got) }
  ${DATA_PLT+${PLT}}
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata   ${RELOCATING-0} : { *(.sdata) }
  ${RELOCATING+_edata  =  .;}
  ${RELOCATING+PROVIDE (edata = .);}
  .sbss    ${RELOCATING-0} :
  {
    ${RELOCATING+__sbss_start = .;}
    *(.sbss)
    *(.scommon)
    ${RELOCATING+__sbss_end = .;}
  }
  ${RELOCATING+_GOT_END_ = .;}
  .bss     ${RELOCATING-0} :
  {
   ${RELOCATING+${OTHER_BSS_SYMBOLS}}
   ${RELOCATING+__bss_start = .;}
   *(.dynbss)
   *(.bss)
   *(COMMON)
  }
  ${RELOCATING+_end = . ;}
  ${RELOCATING+PROVIDE (end = .);}

  /* These are needed for ELF backends which have not yet been
     converted to the new style linker.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }

  /* These must appear regardless of ${RELOCATING}.  */
  ${OTHER_SECTIONS}
}
EOF
