# If you change this file, please also look at files which source this one:
# elf64bmip.sh elf64btsmip.sh elf32btsmipn32.sh elf32bmipn32.sh

# This is an ELF platform.
SCRIPT_NAME=elf

# Handle both big- and little-ended 32-bit MIPS objects.
ARCH=mips
OUTPUT_FORMAT="elf32-bigmips"
BIG_OUTPUT_FORMAT="elf32-bigmips"
LITTLE_OUTPUT_FORMAT="elf32-littlemips"

TEMPLATE_NAME=elf32

case "$EMULATION_NAME" in
elf32*n32*) ELFSIZE=32 ;;
elf64*) ELFSIZE=64 ;;
*) echo $0: unhandled emulation $EMULATION_NAME >&2; exit 1 ;;
esac

if test `echo "$host" | sed -e s/64//` = `echo "$target" | sed -e s/64//`; then
  case " $EMULATION_LIBPATH " in
    *" ${EMULATION_NAME} "*)
      NATIVE=yes
      ;;
  esac
fi

# Look for 64 bit target libraries in /lib64, /usr/lib64 etc., first.
LIBPATH_SUFFIX=$ELFSIZE

GENERATE_SHLIB_SCRIPT=yes

TEXT_START_ADDR=0x10000000
MAXPAGESIZE=0x100000
ENTRY=__start

# GOT-related settings.  
OTHER_GOT_SYMBOLS='
  _gp = ALIGN(16) + 0x7ff0;
'
OTHER_SDATA_SECTIONS="
  .lit8         ${RELOCATING-0} : { *(.lit8) }
  .lit4         ${RELOCATING-0} : { *(.lit4) }
  .srdata       ${RELOCATING-0} : { *(.srdata) }
"

# Magic symbols.
TEXT_START_SYMBOLS='_ftext = . ;'
DATA_START_SYMBOLS='_fdata = . ;'
OTHER_BSS_SYMBOLS='_fbss = .;'

OTHER_SECTIONS="
  .MIPS.events.text ${RELOCATING-0} :
    {
       *(.MIPS.events.text${RELOCATING+ .MIPS.events.gnu.linkonce.t*})
    }
  .MIPS.content.text ${RELOCATING-0} : 
    {
       *(.MIPS.content.text${RELOCATING+ .MIPS.content.gnu.linkonce.t*})
    }
  .MIPS.events.data ${RELOCATING-0} :
    {
       *(.MIPS.events.data${RELOCATING+ .MIPS.events.gnu.linkonce.d*})
    }
  .MIPS.content.data ${RELOCATING-0} :
    {
       *(.MIPS.content.data${RELOCATING+ .MIPS.content.gnu.linkonce.d*})
    }
  .MIPS.events.rodata ${RELOCATING-0} :
    {
       *(.MIPS.events.rodata${RELOCATING+ .MIPS.events.gnu.linkonce.r*})
    }
  .MIPS.content.rodata ${RELOCATING-0} :
    {
       *(.MIPS.content.rodata${RELOCATING+ .MIPS.content.gnu.linkonce.r*})
    }"
