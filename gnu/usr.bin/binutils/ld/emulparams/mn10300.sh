MACHINE=
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-mn10300"
TEXT_START_ADDR=0x0
ARCH=mn10300
MACHINE=
MAXPAGESIZE=1
ENTRY=_start
EMBEDDED=yes

# Hmmm, there's got to be a better way.  This sets the stack to the
# top of the simulator memory (2^19 bytes).
OTHER_RELOCATING_SECTIONS='.stack 0x80000 : { _stack = .; *(.stack) }'

# These are for compatibility with the COFF toolchain.
# XXX These should definitely disappear.
CTOR_START='___ctors = .;'
CTOR_END='___ctors_end = .;'
DTOR_START='___dtors = .;'
DTOR_END='___dtors_end = .;'
