# This is an ELF platform.
SCRIPT_NAME=elf

# Handle both big- and little-ended 64-bit MIPS objects.
ARCH=mips
OUTPUT_FORMAT="elf64-tradbigmips"
BIG_OUTPUT_FORMAT="elf64-tradbigmips"
LITTLE_OUTPUT_FORMAT="elf64-tradlittlemips"

# Note that the elf32 template is used for 64-bit emulations as well 
# as 32-bit emulations.
ELFSIZE=64
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes

TEXT_START_ADDR=0x10000000
DATA_ADDR=0x0400000000
MAXPAGESIZE=0x100000
NONPAGED_TEXT_START_ADDR=0x10000000
SHLIB_TEXT_START_ADDR=0x0
TEXT_DYNAMIC=
ENTRY=__start

# GOT-related settings.  
OTHER_GOT_SYMBOLS='
  _gp = ALIGN(16) + 0x7ff0;
'
OTHER_GOT_SECTIONS='
  .lit8 : { *(.lit8) }
  .lit4 : { *(.lit4) }
'

# Magic symbols.
TEXT_START_SYMBOLS='_ftext = . ;'
DATA_START_SYMBOLS='_fdata = . ;'
OTHER_BSS_SYMBOLS='_fbss = .;'

# Magic sections.
INITIAL_READONLY_SECTIONS='.reginfo : { *(.reginfo) }'
OTHER_TEXT_SECTIONS='*(.mips16.fn.*) *(.mips16.call.*)'
OTHER_SECTIONS='
  .gptab.sdata : { *(.gptab.data) *(.gptab.sdata) }
  .gptab.sbss : { *(.gptab.bss) *(.gptab.sbss) }
'
