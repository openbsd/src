# First set some configuration-specific variables
. ${srcdir}/emulparams/xtensa-config.sh

# See genscripts.sh and ../scripttempl/elfxtensa.sc for the meaning of these.
SCRIPT_NAME=elfxtensa
TEMPLATE_NAME=elf32
EXTRA_EM_FILE=xtensaelf
OUTPUT_FORMAT=undefined
BIG_OUTPUT_FORMAT="elf32-xtensa-be"
LITTLE_OUTPUT_FORMAT="elf32-xtensa-le"
TEXT_START_ADDR=0x400000
NONPAGED_TEXT_START_ADDR=0x400000
ARCH=xtensa
MACHINE=
GENERATE_SHLIB_SCRIPT=yes
GENERATE_COMBRELOC_SCRIPT=yes
NO_SMALL_DATA=yes
OTHER_READONLY_SECTIONS='
  .xt_except_table : { KEEP (*(.xt_except_table)) }
  .xt.lit : { *(.xt.lit*) *(.gnu.linkonce.p*) }
'
OTHER_READWRITE_SECTIONS='
  .xt_except_desc :
  {
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    *(.xt_except_desc_end)
  }
'
OTHER_SECTIONS='
  .xt.insn : { *(.xt.insn) *(.gnu.linkonce.x*) }
'
