MACHINE=
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-m32r"
TEXT_START_ADDR=0x100
ARCH=m32r
MACHINE=
MAXPAGESIZE=32
EMBEDDED=yes

# Hmmm, there's got to be a better way.  This sets the stack to the
# top of the simulator memory (currently 1M).
OTHER_RELOCATING_SECTIONS='PROVIDE (_stack = 0x100000);'
