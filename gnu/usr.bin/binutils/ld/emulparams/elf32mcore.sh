SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-mcore-little"
BIG_OUTPUT_FORMAT="elf32-mcore-big"
LITTLE_OUTPUT_FORMAT="elf32-mcore-little"
PAGE_SIZE=0x1000
TARGET_PAGE_SIZE=0x400
MAXPAGESIZE=0x1000
TEXT_START_ADDR=0
NONPAGED_TEXT_START_ADDR=0
ARCH=mcore
EMBEDDED=yes

# There is a problem with the NOP value - it must work for both
# big endian and little endian systems.  Unfortunately there is
# no symmetrical mcore opcode that functions as a noop.  The
# chosen solution is to use "tst r0, r14".  This is a symetrical
# value, and apart from the corruption of the C bit, it has no other
# side effects.  Since the carry bit is never tested without being
# explicitly set first, and since the NOP code is only used as a
# fill value between independantly viable peices of code, it should
# not matter.
NOP=0x0e0e

OTHER_BSS_SYMBOLS="__bss_start__ = . ;"
OTHER_BSS_END_SYMBOLS="__bss_end__ = . ;"

# Hmmm, there's got to be a better way.  This sets the stack to the
# top of the simulator memory (2^19 bytes).
OTHER_RELOCATING_SECTIONS='.stack 0x80000 : { _stack = .; *(.stack) }'

TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes

# This code gets inserted into the generic elf32.sc linker script
# and allows us to define our own command line switches.
PARSE_AND_LIST_ARGS='

#define OPTION_BASE_FILE		300

#include "getopt.h"

static struct option longopts[] =
{
  {"base-file", required_argument, NULL, OPTION_BASE_FILE},
  {NULL, no_argument, NULL, 0}
};

static void
gld_elf32mcore_list_options (file)
     FILE * file;
{
  fprintf (file, _("  --base_file <basefile>      Generate a base file for relocatable DLLs\n"));
}

static int
gld_elf32mcore_parse_args (argc, argv)
     int argc;
     char ** argv;
{
  int        longind;
  int        optc;
  int        prevoptind = optind;
  int        prevopterr = opterr;
  int        wanterror;
  static int lastoptind = -1;

  if (lastoptind != optind)
    opterr = 0;
  
  wanterror  = opterr;
  lastoptind = optind;

  optc   = getopt_long_only (argc, argv, "-", longopts, & longind);
  opterr = prevopterr;

  switch (optc)
    {
    default:
      if (wanterror)
	xexit (1);
      optind =  prevoptind;
      return 0;

    case OPTION_BASE_FILE:
      link_info.base_file = (PTR) fopen (optarg, FOPEN_WB);
      if (link_info.base_file == NULL)
	{
	  /* xgettext:c-format */
	  fprintf (stderr, _("%s: Cannot open base file %s\n"),
		   program_name, optarg);
	  xexit (1);
	}
      break;
    }
  
  return 1;
}

'
