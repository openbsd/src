SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-littlemips"
BIG_OUTPUT_FORMAT="elf32-bigmips"
LITTLE_OUTPUT_FORMAT="elf32-littlemips"
TEXT_START_ADDR=0x0400000
DATA_ADDR=0x10000000
MAXPAGESIZE=0x40000
NONPAGED_TEXT_START_ADDR=0x0400000
OTHER_READONLY_SECTIONS='.reginfo : { *(.reginfo) }'
OTHER_READWRITE_SECTIONS='
  _gp = . + 0x8000;
  .lit8 : { *(.lit8) }
  .lit4 : { *(.lit4) }
'
TEXT_START_SYMBOLS='_ftext = . ;'
DATA_START_SYMBOLS='_fdata = . ;'
OTHER_BSS_SYMBOLS='_fbss = .;'
EXECUTABLE_SYMBOLS='_DYNAMIC_LINK = 0;'
OTHER_SECTIONS='
  .gptab.sdata : { *(.gptab.data) *(.gptab.sdata) }
  .gptab.sbss : { *(.gptab.bss) *(.gptab.sbss) }
'
ARCH=mips
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
DYNAMIC_LINK=false
