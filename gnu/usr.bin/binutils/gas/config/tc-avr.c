/* tc-avr.c -- Assembler code for the ATMEL AVR

   Copyright (C) 1999, 2000 Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include <ctype.h>
#include "as.h"
#include "subsegs.h"

const char comment_chars[] = ";";
const char line_comment_chars[] = "#";
const char line_separator_chars[] = "$";

#define AVR_ISA_1200      1
#define AVR_ISA_2xxx      3
#define AVR_ISA_MEGA_x03  0x17
#define AVR_ISA_MEGA      0x10
#define AVR_ISA_MEGA_161  0x1b

const char *md_shortopts = "m:";
struct mcu_type_s
{
  char *name;
  int isa;
  int mach;
};

static struct mcu_type_s mcu_types[] =
{
  {"avr1",      AVR_ISA_1200,     bfd_mach_avr1},
  {"avr2",      AVR_ISA_2xxx,     bfd_mach_avr2},
  {"avr3",      AVR_ISA_MEGA_x03, bfd_mach_avr3},
  {"avr4",      AVR_ISA_MEGA_161, bfd_mach_avr4},
  {"at90s1200", AVR_ISA_1200,     bfd_mach_avr1},
  {"at90s2313", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s2323", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s2333", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"attiny22" , AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s2343", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s4433", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s4414", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s4434", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s8515", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"at90s8535", AVR_ISA_2xxx,     bfd_mach_avr2},
  {"atmega603", AVR_ISA_MEGA_x03, bfd_mach_avr3},
  {"atmega103", AVR_ISA_MEGA_x03, bfd_mach_avr3},
  {"atmega161", AVR_ISA_MEGA_161, bfd_mach_avr4},
  {NULL, 0, 0}
};


/* Current MCU type.  */
static struct mcu_type_s default_mcu = {"avr2", AVR_ISA_2xxx,bfd_mach_avr2};
static struct mcu_type_s *avr_mcu = &default_mcu;

const char EXP_CHARS[] = "eE";
const char FLT_CHARS[] = "dD";
static void avr_set_arch (int dummy);

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  {"arch", avr_set_arch,	0},
  { NULL,	NULL,		0}
};

#define LDI_IMMEDIATE(x) (((x) & 0xf) | (((x) << 4) & 0xf00))
#define REGISTER_P(x) ((x) == 'r' || (x) == 'd' || (x) == 'w')

struct avr_opcodes_s
{
  char *name;
  char *constraints;
  char *opcode;
  int insn_size;		/* in words */
  int isa;
  unsigned int bin_opcode;
};

static char * skip_space (char * s);
static char * extract_word (char *from, char *to, int limit);
static unsigned int avr_operand (struct avr_opcodes_s *opcode,
				 int where, char *op, char **line);
static unsigned int avr_operands (struct avr_opcodes_s *opcode, char **line);
static unsigned int avr_get_constant (char * str, unsigned int max);
static char *parse_exp (char *s, expressionS * op);
static bfd_reloc_code_real_type avr_ldi_expression (expressionS *exp);
long md_pcrel_from_section PARAMS ((fixS *, segT));

/* constraint letters
   r - any register
   d - `ldi' register (r16-r31)
   M - immediate value from 0 to 255
   n - immediate value from 0 to 255 ( n = ~M ). Relocation impossible
   w - `adiw' register (r24,r26,r28,r30)
   s - immediate value from 0 to 7
   P - Port address value from 0 to 64. (in, out)
   p - Port address value from 0 to 32. (cbi, sbi, sbic, sbis)
   K - immediate value from 0 to 64 (used in `adiw', `sbiw')
   e - pointer regegisters (X,Y,Z)
   b - base pointer register and displacement ([YZ]+disp)
   i - immediate value
   l - signed pc relative offset from -64 to 63
   L - signed pc relative offset from -2048 to 2047
   h - absolut code address (call, jmp)
   S - immediate value from 0 to 7 (S = s << 4)
*/
struct avr_opcodes_s avr_opcodes[] =
{
  {"adc",  "r,r", "000111rdddddrrrr", 1, AVR_ISA_1200, 0x1c00},
  {"add",  "r,r", "000011rdddddrrrr", 1, AVR_ISA_1200, 0x0c00},
  {"and",  "r,r", "001000rdddddrrrr", 1, AVR_ISA_1200, 0x2000},
  {"cp",   "r,r", "000101rdddddrrrr", 1, AVR_ISA_1200, 0x1400},
  {"cpc",  "r,r", "000001rdddddrrrr", 1, AVR_ISA_1200, 0x0400},
  {"cpse", "r,r", "000100rdddddrrrr", 1, AVR_ISA_1200, 0x1000},
  {"eor",  "r,r", "001001rdddddrrrr", 1, AVR_ISA_1200, 0x2400},
  {"mov",  "r,r", "001011rdddddrrrr", 1, AVR_ISA_1200, 0x2c00},
  {"mul",  "r,r", "100111rdddddrrrr", 1, AVR_ISA_MEGA_161, 0x9c00},
  {"or",   "r,r", "001010rdddddrrrr", 1, AVR_ISA_1200, 0x2800},
  {"sbc",  "r,r", "000010rdddddrrrr", 1, AVR_ISA_1200, 0x0800},
  {"sub",  "r,r", "000110rdddddrrrr", 1, AVR_ISA_1200, 0x1800},

  {"clr",  "r=r", "001001rdddddrrrr", 1, AVR_ISA_1200, 0x2400},
  {"lsl",  "r=r", "000011rdddddrrrr", 1, AVR_ISA_1200, 0x0c00},
  {"rol",  "r=r", "000111rdddddrrrr", 1, AVR_ISA_1200, 0x1c00},
  {"tst",  "r=r", "001000rdddddrrrr", 1, AVR_ISA_1200, 0x2000},

  {"andi", "d,M", "0111KKKKddddKKKK", 1, AVR_ISA_1200, 0x7000},
  /*XXX special case*/
  {"cbr",  "d,n", "0111KKKKddddKKKK", 1, AVR_ISA_1200, 0x7000},
  {"cpi",  "d,M", "0011KKKKddddKKKK", 1, AVR_ISA_1200, 0x3000},
  {"ldi",  "d,M", "1110KKKKddddKKKK", 1, AVR_ISA_1200, 0xe000},
  {"ori",  "d,M", "0110KKKKddddKKKK", 1, AVR_ISA_1200, 0x6000},
  {"sbci", "d,M", "0100KKKKddddKKKK", 1, AVR_ISA_1200, 0x4000},
  {"sbr",  "d,M", "0110KKKKddddKKKK", 1, AVR_ISA_1200, 0x6000},
  {"subi", "d,M", "0101KKKKddddKKKK", 1, AVR_ISA_1200, 0x5000},

  {"sbrc", "r,s", "1111110rrrrr0sss", 1, AVR_ISA_1200, 0xfc00},
  {"sbrs", "r,s", "1111111rrrrr0sss", 1, AVR_ISA_1200, 0xfe00},
  {"bld",  "r,s", "1111100ddddd0sss", 1, AVR_ISA_1200, 0xf800},
  {"bst",  "r,s", "1111101ddddd0sss", 1, AVR_ISA_1200, 0xfa00},

  {"in",   "r,P", "10110PPdddddPPPP", 1, AVR_ISA_1200, 0xb000},
  {"out",  "P,r", "10111PPrrrrrPPPP", 1, AVR_ISA_1200, 0xb800},

  {"adiw", "w,K", "10010110KKddKKKK", 1, AVR_ISA_2xxx, 0x9600},
  {"sbiw", "w,K", "10010111KKddKKKK", 1, AVR_ISA_2xxx, 0x9700},

  {"cbi",  "p,s", "10011000pppppsss", 1, AVR_ISA_1200, 0x9800},
  {"sbi",  "p,s", "10011010pppppsss", 1, AVR_ISA_1200, 0x9a00},
  {"sbic", "p,s", "10011001pppppsss", 1, AVR_ISA_1200, 0x9900},
  {"sbis", "p,s", "10011011pppppsss", 1, AVR_ISA_1200, 0x9b00},

  /* ee = {X=11,Y=10,Z=00, 0} */
  {"ld",   "r,e", "100!000dddddee-+", 1, AVR_ISA_2xxx, 0x8000},
  {"st",   "e,r", "100!001rrrrree-+", 1, AVR_ISA_2xxx, 0x8200},
  {"ldd",  "r,b", "10o0oo0dddddbooo", 1, AVR_ISA_2xxx, 0x8000},
  {"std",  "b,r", "10o0oo1rrrrrbooo", 1, AVR_ISA_2xxx, 0x8200},
  {"sts",  "i,r", "1001001ddddd0000", 2, AVR_ISA_2xxx, 0x9200},
  {"lds",  "r,i", "1001000ddddd0000", 2, AVR_ISA_2xxx, 0x9000},

  {"brbc", "s,l", "111101lllllllsss", 1, AVR_ISA_1200, 0xf400},
  {"brbs", "s,l", "111100lllllllsss", 1, AVR_ISA_1200, 0xf000},

  {"brcc", "l",   "111101lllllll000", 1, AVR_ISA_1200, 0xf400},
  {"brcs", "l",   "111100lllllll000", 1, AVR_ISA_1200, 0xf000},
  {"breq", "l",   "111100lllllll001", 1, AVR_ISA_1200, 0xf001},
  {"brge", "l",   "111101lllllll100", 1, AVR_ISA_1200, 0xf404},
  {"brhc", "l",   "111101lllllll101", 1, AVR_ISA_1200, 0xf405},
  {"brhs", "l",   "111100lllllll101", 1, AVR_ISA_1200, 0xf005},
  {"brid", "l",   "111101lllllll111", 1, AVR_ISA_1200, 0xf407},
  {"brie", "l",   "111100lllllll111", 1, AVR_ISA_1200, 0xf007},
  {"brlo", "l",   "111100lllllll000", 1, AVR_ISA_1200, 0xf000},
  {"brlt", "l",   "111100lllllll100", 1, AVR_ISA_1200, 0xf004},
  {"brmi", "l",   "111100lllllll010", 1, AVR_ISA_1200, 0xf002},
  {"brne", "l",   "111101lllllll001", 1, AVR_ISA_1200, 0xf401},
  {"brpl", "l",   "111101lllllll010", 1, AVR_ISA_1200, 0xf402},
  {"brsh", "l",   "111101lllllll000", 1, AVR_ISA_1200, 0xf400},
  {"brtc", "l",   "111101lllllll110", 1, AVR_ISA_1200, 0xf406},
  {"brts", "l",   "111100lllllll110", 1, AVR_ISA_1200, 0xf006},
  {"brvc", "l",   "111101lllllll011", 1, AVR_ISA_1200, 0xf403},
  {"brvs", "l",   "111100lllllll011", 1, AVR_ISA_1200, 0xf003},

  {"rcall", "L",  "1101LLLLLLLLLLLL", 1, AVR_ISA_1200, 0xd000},
  {"rjmp",  "L",  "1100LLLLLLLLLLLL", 1, AVR_ISA_1200, 0xc000},

  {"call", "h",   "1001010hhhhh111h", 2, AVR_ISA_MEGA, 0x940e},
  {"jmp",  "h",   "1001010hhhhh110h", 2, AVR_ISA_MEGA, 0x940c},

  {"asr",  "r",   "1001010rrrrr0101", 1, AVR_ISA_1200, 0x9405},
  {"com",  "r",   "1001010rrrrr0000", 1, AVR_ISA_1200, 0x9400},
  {"dec",  "r",   "1001010rrrrr1010", 1, AVR_ISA_1200, 0x940a},
  {"inc",  "r",   "1001010rrrrr0011", 1, AVR_ISA_1200, 0x9403},
  {"lsr",  "r",   "1001010rrrrr0110", 1, AVR_ISA_1200, 0x9406},
  {"neg",  "r",   "1001010rrrrr0001", 1, AVR_ISA_1200, 0x9401},
  {"pop",  "r",   "1001000rrrrr1111", 1, AVR_ISA_2xxx, 0x900f},
  {"push", "r",   "1001001rrrrr1111", 1, AVR_ISA_2xxx, 0x920f},
  {"ror",  "r",   "1001010rrrrr0111", 1, AVR_ISA_1200, 0x9407},
  {"ser",  "d",   "11101111dddd1111", 1, AVR_ISA_1200, 0xef0f},
  {"swap", "r",   "1001010rrrrr0010", 1, AVR_ISA_1200, 0x9402},

  {"bclr", "S",   "100101001SSS1000", 1, AVR_ISA_1200, 0x9488},
  {"bset", "S",   "100101000SSS1000", 1, AVR_ISA_1200, 0x9408},

  {"clc",  "", 	  "1001010010001000", 1, AVR_ISA_1200, 0x9488},
  {"clh",  "", 	  "1001010011011000", 1, AVR_ISA_1200, 0x94d8},
  {"cli",  "", 	  "1001010011111000", 1, AVR_ISA_1200, 0x94f8},
  {"cln",  "", 	  "1001010010101000", 1, AVR_ISA_1200, 0x94a8},
  {"cls",  "", 	  "1001010011001000", 1, AVR_ISA_1200, 0x94c8},
  {"clt",  "", 	  "1001010011101000", 1, AVR_ISA_1200, 0x94e8},
  {"clv",  "", 	  "1001010010111000", 1, AVR_ISA_1200, 0x94b8},
  {"clz",  "", 	  "1001010010011000", 1, AVR_ISA_1200, 0x9498},
  {"icall","", 	  "1001010100001001", 1, AVR_ISA_2xxx, 0x9509},
  {"ijmp", "", 	  "1001010000001001", 1, AVR_ISA_2xxx, 0x9409},
  {"lpm",  "", 	  "1001010111001000", 1, AVR_ISA_2xxx, 0x95c8},
  {"nop",  "", 	  "0000000000000000", 1, AVR_ISA_1200, 0x0000},
  {"ret",  "", 	  "1001010100001000", 1, AVR_ISA_1200, 0x9508},
  {"reti", "", 	  "1001010100011000", 1, AVR_ISA_1200, 0x9518},
  {"sec",  "", 	  "1001010000001000", 1, AVR_ISA_1200, 0x9408},
  {"seh",  "", 	  "1001010001011000", 1, AVR_ISA_1200, 0x9458},
  {"sei",  "", 	  "1001010001111000", 1, AVR_ISA_1200, 0x9478},
  {"sen",  "", 	  "1001010000101000", 1, AVR_ISA_1200, 0x9428},
  {"ses",  "", 	  "1001010001001000", 1, AVR_ISA_1200, 0x9448},
  {"set",  "", 	  "1001010001101000", 1, AVR_ISA_1200, 0x9468},
  {"sev",  "", 	  "1001010000111000", 1, AVR_ISA_1200, 0x9438},
  {"sez",  "", 	  "1001010000011000", 1, AVR_ISA_1200, 0x9418},
  {"sleep","", 	  "1001010110001000", 1, AVR_ISA_1200, 0x9588},
  {"wdr",  "", 	  "1001010110101000", 1, AVR_ISA_1200, 0x95a8},
  {"elpm", "", 	  "1001010111011000", 1, AVR_ISA_MEGA_x03, 0x95d8},
  {NULL, NULL, NULL, 0, 0, 0}
};



#define EXP_MOD_NAME(i) exp_mod[i].name
#define EXP_MOD_RELOC(i) exp_mod[i].reloc
#define EXP_MOD_NEG_RELOC(i) exp_mod[i].neg_reloc
#define HAVE_PM_P(i) exp_mod[i].have_pm

struct exp_mod_s
{
  char * name;
  bfd_reloc_code_real_type reloc;
  bfd_reloc_code_real_type neg_reloc;
  int have_pm;
};

static struct exp_mod_s exp_mod[] = {
  {"hh8",    BFD_RELOC_AVR_HH8_LDI,    BFD_RELOC_AVR_HH8_LDI_NEG,    1},
  {"pm_hh8", BFD_RELOC_AVR_HH8_LDI_PM, BFD_RELOC_AVR_HH8_LDI_PM_NEG, 0},
  {"hi8",    BFD_RELOC_AVR_HI8_LDI,    BFD_RELOC_AVR_HI8_LDI_NEG,    1},
  {"pm_hi8", BFD_RELOC_AVR_HI8_LDI_PM, BFD_RELOC_AVR_HI8_LDI_PM_NEG, 0},
  {"lo8",    BFD_RELOC_AVR_LO8_LDI,    BFD_RELOC_AVR_LO8_LDI_NEG,    1},
  {"pm_lo8", BFD_RELOC_AVR_LO8_LDI_PM, BFD_RELOC_AVR_LO8_LDI_PM_NEG, 0},
  {"hlo8",   -BFD_RELOC_AVR_LO8_LDI,   -BFD_RELOC_AVR_LO8_LDI_NEG,   0},
  {"hhi8",   -BFD_RELOC_AVR_HI8_LDI,   -BFD_RELOC_AVR_HI8_LDI_NEG,   0},
};

/* Opcode hash table.  */
static struct hash_control *avr_hash;

/* Reloc modifiers hash control (hh8,hi8,lo8,pm_xx).  */
static struct hash_control *avr_mod_hash;

#define OPTION_MMCU (OPTION_MD_BASE + 1)

struct option md_longopts[] = {
  {"mmcu", required_argument, NULL, 'm'},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof(md_longopts);

static inline char *
skip_space (s)
     char * s;
{
  while (*s == ' ' || *s == '\t')
    ++s;
  return s;
}

/* Extract one word from FROM and copy it to TO.  */
static char *
extract_word (char *from, char *to, int limit)
{
  char *op_start;
  char *op_end;
  int size = 0;

  /* Drop leading whitespace.  */
  from = skip_space (from);
  *to = 0;
  /* Find the op code end.  */
  for (op_start = op_end = from; *op_end != 0 && is_part_of_name(*op_end); )
    {
      to[size++] = *op_end++;
      if (size + 1 >= limit)
	break;
    }
  to[size] = 0;
  return op_end;
}

int
md_estimate_size_before_relax (fragp, seg)
     fragS *fragp;
     asection *seg;
{
  abort ();
  return 0;
}

void
md_show_usage (stream)
  FILE *stream;
{
  fprintf
    (stream,
     _ ("AVR options:\n"
	"  -mmcu=[avr-name] select microcontroller variant\n"
	"                   [avr-name] can be:\n"
	"                   avr1 - AT90S1200\n"
	"                   avr2 - AT90S2xxx, AT90S4xxx, AT90S85xx, ATtiny22\n"
	"                   avr3 - ATmega103 or ATmega603\n"
	"                   avr4 - ATmega161\n"
	"                   or immediate microcontroller name.\n"));
}

static void
avr_set_arch (dummy)
     int dummy;
{
  char * str;
  str = (char *)alloca (20);
  input_line_pointer = extract_word (input_line_pointer, str, 20);
  md_parse_option ('m', str);
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, avr_mcu->mach);
}

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  char *t = alloca (strlen (arg) + 1);
  char *s = t;
  char *arg1 = arg;
  do
    *t = tolower (*arg1++);
  while (*t++);

  if (c == 'm')
    {
      int i;

      for (i = 0; mcu_types[i].name; ++i)
	if (strcmp (mcu_types[i].name, s) == 0)
	  break;

      if (!mcu_types[i].name)
	as_fatal (_ ("unknown MCU: %s\n"), arg);
      if (avr_mcu == &default_mcu)
	avr_mcu = &mcu_types[i];
      else
	as_fatal (_ ("redefinition of mcu type `%s'"), mcu_types[i].name);
      return 1;
    }
  return 0;
}

symbolS *
md_undefined_symbol (name)
  char *name;
{
  return 0;
}

/* Convert a string pointed to by input_line_pointer into a floating point
   constant of type `type', and store the appropriate bytes to `*litP'.
   The number of LITTLENUMS emitted is stored in `*sizeP'.  Returns NULL if
   OK, or an error message otherwise.  */
char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[4];
  LITTLENUM_TYPE *wordP;
  char *t;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;
    case 'd':
      prec = 4;
      break;
    default:
      *sizeP = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  /* This loop outputs the LITTLENUMs in REVERSE order.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--), sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return NULL;
}

void
md_convert_frag (abfd, sec, fragP)
  bfd *abfd;
  asection *sec;
  fragS *fragP;
{
  abort ();
}


void
md_begin ()
{
  int i;
  struct avr_opcodes_s *opcode;
  avr_hash = hash_new();

  /* Insert unique names into hash table.  This hash table then provides a
     quick index to the first opcode with a particular name in the opcode
     table.  */

  for (opcode = avr_opcodes; opcode->name; opcode++)
    hash_insert (avr_hash, opcode->name, (char *) opcode);

  avr_mod_hash = hash_new ();

  for (i = 0; i < sizeof (exp_mod) / sizeof (exp_mod[0]); ++i)
    hash_insert (avr_mod_hash, EXP_MOD_NAME(i), (void*)(i+10));

  for (i = 0; i < 32; i++)
    {
      char buf[5];

      sprintf (buf, "r%d", i);
      symbol_table_insert (symbol_new (buf, reg_section, i,
				       &zero_address_frag));
      sprintf (buf, "R%d", i);
      symbol_table_insert (symbol_new (buf, reg_section, i,
				       &zero_address_frag));
    }

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, avr_mcu->mach);
}


static unsigned int
avr_operands (opcode, line)
     struct avr_opcodes_s *opcode;
     char **line;
{
  char *op = opcode->constraints;
  unsigned int bin = opcode->bin_opcode;
  char *frag = frag_more (opcode->insn_size * 2);
  char *str = *line;
  int where = frag - frag_now->fr_literal;

  /* Opcode have operands.  */
  if (*op)
    {
      unsigned int reg1 = 0;
      unsigned int reg2 = 0;
      int reg1_present = 0;
      int reg2_present = 0;

      /* Parse first operand.  */
      if (REGISTER_P (*op))
	reg1_present = 1;
      reg1 = avr_operand (opcode, where, op, &str);
      ++op;

      /* Parse second operand.  */
      if (*op)
	{
	  if (*op == ',')
	    ++op;
	  if (*op == '=')
	    {
	      reg2 = reg1;
	      reg2_present = 1;
	    }
	  else
	    {
	      if (REGISTER_P (*op))
		reg2_present = 1;

	      str = skip_space (str);
	      if (*str++ != ',')
		as_bad (_ ("`,' required"));
	      str = skip_space (str);

	      reg2 = avr_operand (opcode, where, op, &str);

	    }
	  if (reg1_present && reg2_present)
	    reg2 = (reg2 & 0xf) | ((reg2 << 5) & 0x200);
	  else if (reg2_present)
	    reg2 <<= 4;
	}
      if (reg1_present)
	reg1 <<= 4;
      bin |= reg1 | reg2;
    }
  if (opcode->insn_size == 2)
    {
      bfd_putl32 ((bfd_vma)bin, frag);
    }
  else
    {
      bfd_putl16 ((bfd_vma)bin, frag);
    }
  *line = str;
  return bin;
}

static unsigned int
avr_get_constant (str, max)
     char * str;
     unsigned int max;
{
  expressionS ex;
  str = skip_space (str);
  input_line_pointer = str;
  expression (&ex);

  if (ex.X_op != O_constant)
    as_bad (_("constant value required"));

  if (ex.X_add_number > max)
    as_bad (_("number must be less than %d"), max+1);
  return ex.X_add_number;
}

static unsigned int
avr_operand (opcode, where, op, line)
     struct avr_opcodes_s *opcode;
     int where;
     char *op;
     char **line;
{
  unsigned int op_mask = 0;
  char *str = *line;
  expressionS op_expr;

  str = skip_space (str);
  switch (*op)
    {
      /* Any register operand.  */
    case 'w':
    case 'd':
    case 'r':
      {
	char r_name[256];
	str = extract_word (str, r_name, sizeof (r_name));
	parse_exp (r_name, &op_expr);
	if (op_expr.X_op == O_register)
	  {
	    op_mask = op_expr.X_add_number;
	    if (op_mask <= 31)
	      {
		if (*op == 'd')
		  {
		    if (op_mask < 16)
		      as_bad (_ ("register number above 15 required"));
		    op_mask -= 16;
		  }
		if (*op == 'w')
		  {
		    op_mask -= 24;
		    if (op_mask & 1 || op_mask > 6)
		      as_bad (_ ("register r24,r26,r28 or r30 required"));
		    op_mask >>= 1;
		  }
		break;
	      }
	  }
	as_bad (_ ("register required"));
      }
      break;

    case 'e':
      {
	char c;
	if (*str == '-')
	  {
	    str = skip_space (str+1);
	    op_mask = 0x1002;
	  }
	c = tolower (*str);
	if (c == 'x')
	  op_mask |= 0x100c;
	else if (c == 'y')
	  op_mask |= 0x8;
	else if (c != 'z')
	  as_bad (_ ("pointer register (X,Y or Z) required"));

	str = skip_space (str+1);
	if (*str == '+')
	  {
	    ++str;
	    if (op_mask & 2)
	      as_bad (_ ("cannot both predecrement and postincrement"));
	    op_mask |= 0x1001;
	  }
      }
      break;

    case 'b':
      {
	char c = tolower (*str++);
	if (c == 'y')
	  op_mask |= 0x8;
	else if (c != 'z')
	  as_bad (_ ("pointer register (Y or Z) required"));
	str = skip_space (str);
	if (*str++ == '+')
	  {
	    unsigned int x;
	    x = avr_get_constant (str, 63);
	    str = input_line_pointer;
	    op_mask |= (x & 7) | ((x & (3 << 3)) << 7) | ((x & (1 << 5)) << 8);
	  }
      }
      break;

    case 'h':
      {
	str = parse_exp (str, &op_expr);
	fix_new_exp (frag_now, where, opcode->insn_size * 2,
		     &op_expr, false, BFD_RELOC_AVR_CALL);

      }
      break;

    case 'L':
      {
	str = parse_exp (str, &op_expr);
	fix_new_exp (frag_now, where, opcode->insn_size * 2,
		     &op_expr, true, BFD_RELOC_AVR_13_PCREL);

      }
      break;

    case 'l':
      {
	str = parse_exp (str, &op_expr);
	fix_new_exp (frag_now, where, opcode->insn_size * 2,
		     &op_expr, true, BFD_RELOC_AVR_7_PCREL);

      }
      break;

    case 'i':
      {
	str = parse_exp (str, &op_expr);
	fix_new_exp (frag_now, where+2, opcode->insn_size * 2,
		     &op_expr, false, BFD_RELOC_16);

      }
      break;

    case 'M':
      {
	bfd_reloc_code_real_type r_type;
 	input_line_pointer = str;
 	r_type = avr_ldi_expression (&op_expr);
 	str = input_line_pointer;
	fix_new_exp (frag_now, where, 3,
		     &op_expr, false, r_type);
      }
      break;

    case 'n':
      {
	unsigned int x;
	x = ~avr_get_constant (str, 255);
	str = input_line_pointer;
	op_mask |= (x & 0xf) | ((x << 4) & 0xf00);
      }
      break;

    case 'K':
      {
	unsigned int x;
	x = avr_get_constant (str, 63);
	str = input_line_pointer;
	op_mask |= (x & 0xf) | ((x & 0x30) << 2);
      }
      break;

    case 'S':
    case 's':
      {
	unsigned int x;
	x = avr_get_constant (str, 7);
	str = input_line_pointer;
	if (*op == 'S')
	  x <<= 4;
	op_mask |= x;
      }
      break;

    case 'P':
      {
	unsigned int x;
	x = avr_get_constant (str, 63);
	str = input_line_pointer;
	op_mask |= (x & 0xf) | ((x & 0x30) << 5);
      }
      break;

    case 'p':
      {
	unsigned int x;
	x = avr_get_constant (str, 31);
	str = input_line_pointer;
	op_mask |= x << 3;
      }
      break;
    default:
      as_bad (_ ("unknown constraint `%c'"), *op);
    }
  *line = str;
  return op_mask;
}

/* GAS will call this function for each section at the end of the assembly,
   to permit the CPU backend to adjust the alignment of a section.  */
valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);
  return ((addr + (1 << align) - 1) & (-1 << align));
}

/* If you define this macro, it should return the offset between the
   address of a PC relative fixup and the position from which the PC
   relative adjustment should be made.  On many processors, the base
   of a PC relative instruction is the next instruction, so this
   macro would return the length of an instruction.  */
long
md_pcrel_from_section (fixp, sec)
     fixS *fixp;
     segT sec;
{
  if (fixp->fx_addsy != (symbolS *)NULL
      && (!S_IS_DEFINED (fixp->fx_addsy)
	  || (S_GET_SEGMENT (fixp->fx_addsy) != sec)))
    return 0;
  return fixp->fx_frag->fr_address + fixp->fx_where;
}

/* GAS will call this for each fixup.  It should store the correct
   value in the object file. */
int
md_apply_fix3 (fixp, valuep, seg)
     fixS *fixp;
     valueT *valuep;
     segT seg;
{
  unsigned char *where;
  unsigned long insn;
  long value;

  if (fixp->fx_addsy == (symbolS *) NULL)
    {
      value = *valuep;
      fixp->fx_done = 1;
    }
  else if (fixp->fx_pcrel)
    {
      segT s = S_GET_SEGMENT (fixp->fx_addsy);
      if (fixp->fx_addsy && (s == seg || s == absolute_section))
	{
	  value = S_GET_VALUE (fixp->fx_addsy) + *valuep;
	  fixp->fx_done = 1;
	}
      else
	value = *valuep;
    }
  else
    {
      value = fixp->fx_offset;
      if (fixp->fx_subsy != (symbolS *) NULL)
	{
	  if (S_GET_SEGMENT (fixp->fx_subsy) == absolute_section)
	    {
	      value -= S_GET_VALUE (fixp->fx_subsy);
	      fixp->fx_done = 1;
	    }
	  else
	    {
	      /* We don't actually support subtracting a symbol.  */
 	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    _ ("expression too complex"));
	    }
	}
    }
  switch (fixp->fx_r_type)
    {
    default:
      fixp->fx_no_overflow = 1;
      break;
    case BFD_RELOC_AVR_7_PCREL:
    case BFD_RELOC_AVR_13_PCREL:
    case BFD_RELOC_32:
    case BFD_RELOC_16:
    case BFD_RELOC_AVR_CALL:
      break;
    }

  if (fixp->fx_done)
    {
      /* Fetch the instruction, insert the fully resolved operand
	 value, and stuff the instruction back again.  */
      where = fixp->fx_frag->fr_literal + fixp->fx_where;
      insn = bfd_getl16 (where);

      switch (fixp->fx_r_type)
	{
	case BFD_RELOC_AVR_7_PCREL:
	  if (value & 1)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("odd address operand: %ld"), value);
	  /* Instruction addresses are always right-shifted by 1.  */
	  value >>= 1;
	  --value;			/* Correct PC.  */
	  if (value < -64 || value > 63)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("operand out of range: %ld"), value);
	  value = (value << 3) & 0x3f8;
	  bfd_putl16 ((bfd_vma) (value | insn), where);
	  break;

	case BFD_RELOC_AVR_13_PCREL:
	  if (value & 1)
	    as_bad_where (fixp->fx_file, fixp->fx_line,
			  _("odd address operand: %ld"), value);
	  /* Instruction addresses are always right-shifted by 1.  */
	  value >>= 1;
	  --value;			/* Correct PC.  */
	  /* XXX AT90S8515 must have WRAP here.  */

	  if (value < -2048 || value > 2047)
	    {
	      if (avr_mcu->mach == bfd_mach_avr2)
		{
		  if (value > 2047)
		    value -= 4096;
		  else
		    value += 4096;
		}
	      else
		as_bad_where (fixp->fx_file, fixp->fx_line,
			      _("operand out of range: %ld"), value);
	    }

	  value &= 0xfff;
	  bfd_putl16 ((bfd_vma) (value | insn), where);
	  break;

	case BFD_RELOC_32:
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_16:
	  bfd_putl16 ((bfd_vma) value, where);
	  break;

	case BFD_RELOC_AVR_16_PM:
	  bfd_putl16 ((bfd_vma) (value>>1), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value), where);
	  break;

	case -BFD_RELOC_AVR_LO8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 16), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 8), where);
	  break;

	case -BFD_RELOC_AVR_HI8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 24), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 16), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value), where);
	  break;

	case -BFD_RELOC_AVR_LO8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 16), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 8), where);
	  break;

	case -BFD_RELOC_AVR_HI8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 24), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 16), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI_PM:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 1), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI_PM:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 9), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI_PM:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (value >> 17), where);
	  break;

	case BFD_RELOC_AVR_LO8_LDI_PM_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 1), where);
	  break;

	case BFD_RELOC_AVR_HI8_LDI_PM_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 9), where);
	  break;

	case BFD_RELOC_AVR_HH8_LDI_PM_NEG:
	  bfd_putl16 ((bfd_vma) insn | LDI_IMMEDIATE (-value >> 17), where);
	  break;

	case BFD_RELOC_AVR_CALL:
	  {
	    unsigned long x;
	    x = bfd_getl16 (where);
	    if (value & 1)
	      as_bad_where (fixp->fx_file, fixp->fx_line,
			    _("odd address operand: %ld"), value);
	    value >>= 1;
	    x |= ((value & 0x10000) | ((value << 3) & 0x1f00000)) >> 16;
	    bfd_putl16 ((bfd_vma) x, where);
	    bfd_putl16 ((bfd_vma) (value & 0xffff), where+2);
	  }
	  break;

	default:
	  as_fatal ( _("line %d: unknown relocation type: 0x%x"),
		     fixp->fx_line, fixp->fx_r_type);
	  break;
	}
    }
  else
    {
      switch (fixp->fx_r_type)
	{
	case -BFD_RELOC_AVR_HI8_LDI_NEG:
	case -BFD_RELOC_AVR_HI8_LDI:
	case -BFD_RELOC_AVR_LO8_LDI_NEG:
	case -BFD_RELOC_AVR_LO8_LDI:
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			_("only constant expression allowed"));
	  fixp->fx_done = 1;
	  break;
	default:
	  break;
	}
      fixp->fx_addnumber = value;
    }
  return 0;
}


/* A `BFD_ASSEMBLER' GAS will call this to generate a reloc.  GAS
   will pass the resulting reloc to `bfd_install_relocation'.  This
   currently works poorly, as `bfd_install_relocation' often does the
   wrong thing, and instances of `tc_gen_reloc' have been written to
   work around the problems, which in turns makes it difficult to fix
   `bfd_install_relocation'. */

/* If while processing a fixup, a reloc really needs to be created
   then it is done here.  */

arelent *
tc_gen_reloc (seg, fixp)
     asection *seg;
     fixS *fixp;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);

  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixp->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
                    _("reloc %d not supported by object file format"),
		    (int)fixp->fx_r_type);
      return NULL;
    }

  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    reloc->address = fixp->fx_offset;

  reloc->addend = fixp->fx_offset;

  return reloc;
}


void
md_assemble (str)
     char *str;
{
  struct avr_opcodes_s * opcode;
  char op[11];

  str = extract_word (str, op, sizeof(op));

  if (!op[0])
    as_bad (_ ("can't find opcode "));

  opcode = (struct avr_opcodes_s *) hash_find (avr_hash, op);

  if (opcode == NULL)
    {
      as_bad (_ ("unknown opcode `%s'"), op);
      return;
    }

  if ((opcode->isa & avr_mcu->isa) != opcode->isa)
    as_bad (_ ("illegal opcode %s for mcu %s"), opcode->name, avr_mcu->name);

  /* We used to set input_line_pointer to the result of get_operands,
     but that is wrong.  Our caller assumes we don't change it.  */
  {
    char *t = input_line_pointer;
    avr_operands (opcode, &str);
    if (*str)
      as_bad (_ ("garbage at end of line"));
    input_line_pointer = t;
  }
}

/* Parse ordinary expression.  */
static char *
parse_exp (s, op)
     char *s;
     expressionS * op;
{
  input_line_pointer = s;
  expression (op);
  if (op->X_op == O_absent)
    as_bad (_("missing operand"));
  return input_line_pointer;
}


/* Parse special expressions (needed for LDI command):
   xx8 (address)
   xx8 (-address)
   pm_xx8 (address)
   pm_xx8 (-address)
   where xx is: hh, hi, lo
*/
static bfd_reloc_code_real_type
avr_ldi_expression (exp)
     expressionS *exp;
{
  char *str = input_line_pointer;
  char *tmp;
  char op[8];
  int mod;
  tmp = str;

  str = extract_word (str, op, sizeof (op));
  if (op[0])
    {
      mod = (int) hash_find (avr_mod_hash, op);
      if (mod)
	{
	  int closes = 0;
	  mod -= 10;
	  str = skip_space (str);
	  if (*str == '(')
	    {
	      int neg_p = 0;
	      ++str;
	      if (strncmp ("pm(", str, 3) == 0
		  || strncmp ("-(pm(", str, 5) == 0)
		{
		  if (HAVE_PM_P(mod))
		    {
		      ++mod;
		      ++closes;
		    }
		  else
		    as_bad (_ ("illegal expression"));
		  if (*str == '-')
		    {
		      neg_p = 1;
		      ++closes;
		      str += 5;
		    }
		  else
		    str += 3;
		}
	      if (*str == '-' && *(str + 1) == '(')
		{
		  neg_p ^= 1;
		  ++closes;
		  str += 2;
		}
	      input_line_pointer = str;
	      expression (exp);
	      do
		{
		  if (*input_line_pointer != ')')
		    {
		      as_bad (_ ("`)' required"));
		      break;
		    }
		  input_line_pointer++;
		}
	      while (closes--);
	      return neg_p ? EXP_MOD_NEG_RELOC (mod) : EXP_MOD_RELOC (mod);
	    }
	}
    }
  input_line_pointer = tmp;
  expression (exp);
  return BFD_RELOC_AVR_LO8_LDI;
}

/* Flag to pass `pm' mode between `avr_parse_cons_expression' and
   `avr_cons_fix_new' */
static int exp_mod_pm = 0;

/* Parse special CONS expression: pm (expression)
   which is used for addressing to a program memory.
   Relocation: BFD_RELOC_AVR_16_PM */
void
avr_parse_cons_expression (exp, nbytes)
     expressionS *exp;
     int nbytes;
{
  char * tmp;

  exp_mod_pm = 0;

  tmp = input_line_pointer = skip_space (input_line_pointer);

  if (nbytes == 2)
    {
      char * pm_name = "pm";
      int len = strlen (pm_name);
      if (strncasecmp (input_line_pointer, pm_name, len) == 0)
	{
	  input_line_pointer = skip_space (input_line_pointer + len);
	  if (*input_line_pointer == '(')
	    {
	      input_line_pointer = skip_space (input_line_pointer + 1);
	      exp_mod_pm = 1;
	      expression (exp);
	      if (*input_line_pointer == ')')
		++input_line_pointer;
	      else
		{
		  as_bad (_ ("`)' required"));
		  exp_mod_pm = 0;
		}
	      return;
	    }
	  input_line_pointer = tmp;
	}
    }
  expression (exp);
}

void
avr_cons_fix_new(frag, where, nbytes, exp)
     fragS *frag;
     int where;
     int nbytes;
     expressionS *exp;
{
  if (exp_mod_pm == 0)
    {
      if (nbytes == 2)
	fix_new_exp (frag, where, nbytes, exp, false, BFD_RELOC_16);
      else if (nbytes == 4)
	fix_new_exp (frag, where, nbytes, exp, false, BFD_RELOC_32);
      else
	as_bad (_ ("illegal %srelocation size: %d"), "", nbytes);
    }
  else
    {
      if (nbytes == 2)
	fix_new_exp (frag, where, nbytes, exp, false, BFD_RELOC_AVR_16_PM);
      else
	as_bad (_ ("illegal %srelocation size: %d"), "`pm' ", nbytes);
      exp_mod_pm = 0;
    }
}
