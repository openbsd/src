/*---------------------------------------------------------------------------

  VMSmunch_private.h

  Contents of three header files from Joe
  Meadows' FILE program.  Used by VMSmunch 

	06-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved "contents of three header files" from 
			vmsmunch.h to vmsmunch_private.h .
  ---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    fatdef.h
  ---------------------------------------------------------------------------*/

/* This header file was created by Joe Meadows, and is not copyrighted
   in any way. No guarantee is made as to the accuracy of the contents
   of this header file. This header file was last modified on Sep. 22th,
   1987. (Modified to include this statement) */
#define FAT$K_LENGTH 32
#define FAT$C_LENGTH 32
#define FAT$S_FATDEF 32

struct fatdef {
  union  {
    unsigned char fat$b_rtype;
    struct  {
      unsigned fat$v_rtype : 4;
      unsigned fat$v_fileorg : 4;
    } fat$r_rtype_bits;
  } fat$r_rtype_overlay;
# define FAT$S_RTYPE 4
# define FAT$V_RTYPE 0
#   define FAT$C_UNDEFINED 0
#   define FAT$C_FIXED 1
#   define FAT$C_VARIABLE 2
#   define FAT$C_VFC 3
#   define FAT$C_STREAM 4
#   define FAT$C_STREAMLF 5
#   define FAT$C_STREAMCR 6
# define FAT$S_FILEORG 4
# define FAT$V_FILEORG 4
#   define FAT$C_SEQUENTIAL 0
#   define FAT$C_RELATIVE 1
#   define FAT$C_INDEXED 2
#   define FAT$C_DIRECT 3
  union  {
    unsigned char fat$b_rattrib;
    struct  {
      unsigned fat$v_fortrancc : 1;
      unsigned fat$v_impliedcc : 1;
      unsigned fat$v_printcc : 1;
      unsigned fat$v_nospan : 1;
    } fat$r_rattrib_bits;
  } fat$r_rattrib_overlay;
#   define FAT$V_FORTRANCC 0
#   define FAT$M_FORTRANCC 1
#   define FAT$V_IMPLIEDCC 1
#   define FAT$M_IMPLIEDCC 2
#   define FAT$V_PRINTCC 2
#   define FAT$M_PRINTCC 4
#   define FAT$V_NOSPAN 3
#   define FAT$M_NOSPAN 8
  unsigned short int fat$w_rsize;
  union
  {
    unsigned long int fat$l_hiblk;
    struct
    {
      unsigned short int fat$w_hiblkh;
      unsigned short int fat$w_hiblkl;
    } fat$r_hiblk_fields;
  } fat$r_hiblk_overlay;
  union
  {
    unsigned long int fat$l_efblk;
    struct
    {
      unsigned short int fat$w_efblkh;
      unsigned short int fat$w_efblkl;
    } fat$r_efblk_fields;
  } fat$r_efblk_overlay;
  unsigned short int fat$w_ffbyte;
  unsigned char fat$b_bktsize;
  unsigned char fat$b_vfcsize;
  unsigned short int fat$w_maxrec;
  unsigned short int fat$w_defext;
  unsigned short int fat$w_gbc;
  char fat$fill[8];
  unsigned short int fat$w_versions;
};

/*---------------------------------------------------------------------------
    fchdef.h
  ---------------------------------------------------------------------------*/

/* This header file was created by Joe Meadows, and is not copyrighted
   in any way. No guarantee is made as to the accuracy of the contents
   of this header file. This header file was last modified on Sep. 22th,
   1987. (Modified to include this statement) */

#define FCH$V_BADACL 0x00B
#define FCH$M_BADACL (1 << FCH$V_ACL)
#define FCH$V_BADBLOCK 0x00E
#define FCH$M_BADBLOCK (1 << FCH$V_BADBLOCK)
#define FCH$V_CONTIG 0x007
#define FCH$M_CONTIG (1 << FCH$V_CONTIG)
#define FCH$V_CONTIGB 0x005
#define FCH$M_CONTIGB (1 << FCH$V_CONTIGB)
#define FCH$V_DIRECTORY 0x00D
#define FCH$M_DIRECTORY (1 << FCH$V_DIRECTORY)
#define FCH$V_ERASE 0x011
#define FCH$M_ERASE (1 << FCH$V_ERASE)
#define FCH$V_LOCKED 0x006
#define FCH$M_LOCKED (1 << FCH$V_LOCKED)
#define FCH$V_MARKDEL 0x00F
#define FCH$M_MARKDEL (1 << FCH$V_MARKDEL)
#define FCH$V_NOBACKUP 0x001
#define FCH$M_NOBACKUP (1 << FCH$V_NOBACKUP)
#define FCH$V_NOCHARGE 0x010
#define FCH$M_NOCHARGE (1 << FCH$V_NOCHARGE)
#define FCH$V_READCHECK 0x003
#define FCH$M_READCHECK (1 << FCH$V_READCHECK)
#define FCH$V_SPOOL 0x00C
#define FCH$M_SPOOL (1 << FCH$V_SPOOL)
#define FCH$V_WRITCHECK 0x004
#define FCH$M_WRITCHECK (1 << FCH$V_WRITCHECK)
#define FCH$V_WRITEBACK 0x002
#define FCH$M_WRITEBACK (1 << FCH$V_WRITEBACK)

struct fchdef  {
  unsigned : 1;
  unsigned fch$v_nobackup : 1 ;
  unsigned fch$v_writeback : 1;
  unsigned fch$v_readcheck : 1;
  unsigned fch$v_writcheck : 1;
  unsigned fch$v_contigb : 1;
  unsigned fch$v_locked : 1;
  unsigned fch$v_contig : 1;
  unsigned : 3;
  unsigned fch$v_badacl : 1;
  unsigned fch$v_spool : 1;
  unsigned fch$v_directory : 1;
  unsigned fch$v_badblock : 1;
  unsigned fch$v_markdel : 1;
  unsigned fch$v_nocharge : 1;
  unsigned fch$v_erase : 1;
};

/*---------------------------------------------------------------------------
    fjndef.h
  ---------------------------------------------------------------------------*/

/* This header file was created by Joe Meadows, and is not copyrighted
   in any way. No guarantee is made as to the accuracy of the contents
   of this header file. This header file was last modified on Sep. 22th,
   1987. (Modified to include this statement) */

#define FJN$M_ONLY_RU 1
#define FJN$M_RUJNL 2
#define FJN$M_BIJNL 4
#define FJN$M_AIJNL 8
#define FJN$M_ATJNL 16
#define FJN$M_NEVER_RU 32
#define FJN$M_JOURNAL_FILE 64
#define FJN$S_FJNDEF 1
struct fjndef  {
  unsigned fjn$v_only_ru : 1;
  unsigned fjn$v_rujnl : 1;
  unsigned fjn$v_bijnl : 1;
  unsigned fjn$v_aijnl : 1;
  unsigned fjn$v_atjnl : 1;
  unsigned fjn$v_never_ru : 1;
  unsigned fjn$v_journal_file:1;
} ;
