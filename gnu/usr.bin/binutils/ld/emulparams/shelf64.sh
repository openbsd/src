# Note: this is sourced in turn by shlelf64.sh
OUTPUT_FORMAT=${OUTPUT_FORMAT-"elf64-sh64"}
ELFSIZE=64

EXTRA_EM_FILE=
. ${srcdir}/emulparams/shelf32.sh

# We do not need .cranges
OTHER_SECTIONS=''
