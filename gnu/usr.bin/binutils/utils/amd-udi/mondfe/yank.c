static char _[] = "@(#)yank.c	5.20 93/07/30 16:39:05, Srini, AMD. ";
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 **       This module is used to "yank" or load a COFF file
 **       Into target memory.
 *****************************************************************************
 */


#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include "coff.h"
#include "memspcs.h"
#include "main.h"
#include "miniint.h"
#include "macros.h"
#include "error.h"

#ifdef	MSDOS
#include <string.h>
#include <stdlib.h>
#else
#include <string.h>
#endif

/* Definitions */

#define FILE_BUFFER_SIZE     1024

#ifdef MSDOS
#define FILE_OPEN_FLAG   "rb"
#else
#define FILE_OPEN_FLAG   "r"
#endif

#define	FROM_BEGINNING	0

/* Function declarations */
INT32   Mini_load_coff PARAMS((char *fname,
			       INT32 space, 
			       INT32 sym,
			       INT32 sects,
			       int   msg));
INT32	Mini_init_info_ptr PARAMS((INIT_INFO *init));
INT32	Mini_send_init_info PARAMS((INIT_INFO *init));
INT32	Mini_load_file PARAMS((char *fname,
			       INT32 mspace,
			       int fargc,
			       char *fargs,
			       INT32 sym,
			       INT32 sects,
			       int msg));
void  convert32 PARAMS((BYTE *));
void  convert16 PARAMS((BYTE *));
int	SetSections PARAMS((char *));
void	print_ld_msg PARAMS((INT32, int, ADDR32, INT32));
void	print_ign_msg PARAMS((int, ADDR32, INT32));
void	print_end_msg PARAMS((INT32, int, ADDR32, INT32));

/* GLobal */

GLOBAL	INIT_INFO	init_info;

static	BYTE   buffer[FILE_BUFFER_SIZE];
extern	char	CoffFileName[];
static	INT32	Symbols=0;  /* No symbols support yet */
static	INT32	Sections=STYP_ABS | STYP_TEXT | STYP_DATA | STYP_LIT | STYP_BSS;
static	INT32	MSpace=I_MEM;
static	int	FileArgc;
static	char	ArgString[1024];
static	FILE	*coff_in;
static	int	InitializeProgram=1;


/*
** This is the function called by the main program to load
** a COFF file.  It also modifies the global data structure
** init.  The data structure is then sent via an INIT message
** to the target.  In addition, the "argv" parameter string
** is sent to the target with WRITE messages.
**
*/

INT32
yank_cmd(token, token_count)
   char    *token[];
   int      token_count;
   {
   int	i;
   int	j;
   int	SectionsGiven=0;
   int	IPFlag=0;

   for (j=1; j < token_count; j++) { /* parse command */
      switch (token[j][0]) {
	 case	'-':
		if (token[j][1] == '\0')
		   return (EMSYNTAX);
		if (strcmp(token[j],"-ms")==0) {/*mem stack opt */
		  if (++j >= token_count)
		     return (EMSYNTAX);
		  if (sscanf(token[j],"%lx",&(init_info.mem_stack_size)) != 1) 
		     return (EMSYNTAX);
		} else if (strcmp(token[j],"-rs")==0) {/*r stack*/
		  if (++j >= token_count)
		     return (EMSYNTAX);
		  if (sscanf(token[j],"%lx",&(init_info.reg_stack_size)) != 1) 
		     return (EMSYNTAX);
		} else if (strcmp(token[j],"-noi")==0) {/*no init */
		  InitializeProgram = 0;
		  IPFlag = 1;
		} else if (strcmp(token[j],"-i")==0) {/*init*/
		  InitializeProgram = 1;
		  IPFlag = 1;
		} else {
		  if (SetSections(token[j]) == (int) -1) 
		     return (EMSYNTAX);
		  else
		     SectionsGiven=1;
		}
		break;
	 default: /* filename etc. */
		if (!SectionsGiven) {
	          Sections = STYP_ABS|STYP_TEXT|STYP_DATA|STYP_LIT|STYP_BSS; 
		  SectionsGiven=0;
		}
		if (!IPFlag) {
		  InitializeProgram = 1;
		  IPFlag=0;
		}
	        (void) strcpy (&CoffFileName[0], token[j]);
	        FileArgc = token_count - j;
                (void) strcpy(ArgString, token[j]);
                for (i = 1; i < FileArgc; i++) {
                   strcat(ArgString, " ");
                   strcat(ArgString, token[j+i]);
                };
		j = token_count; /* break out of for loop */
		break;
      };
   }
   if (strcmp(CoffFileName,"") == 0)  /* No COFF file given */
     return (EMSYNTAX);

   if (Mini_load_file(&CoffFileName[0], MSpace,
			   FileArgc, ArgString,
			   Symbols, Sections,
			   QuietMode) != SUCCESS)  {
       return(FAILURE);
   } else
     return(SUCCESS);
};


INT32
Mini_load_file(filename, mspace, fileargc, fileargs, sym, sects, quietmode)
char	*filename;
INT32	mspace;
int	fileargc;
char	*fileargs;
INT32	sym;
INT32	sects;
int	quietmode;
{

   if (Mini_init_info_ptr(&init_info) != SUCCESS)
     return(FAILURE);

   if (Mini_load_coff(filename, mspace, sym, sects, quietmode) != SUCCESS)
       return(FAILURE);

   init_info.argstring = fileargs;   /* complete argv string */

   if (InitializeProgram) {
      if (Mini_send_init_info(&init_info) != SUCCESS)
        return(FAILURE);
   } else {
     warning(EMNOINITP);
   }

   return(SUCCESS);
};

INT32
Mini_init_info_ptr(init_ptr)
INIT_INFO	*init_ptr;
{

   /* Re-initialize INIT message */
   init_ptr->text_start = 0xffffffff;
   init_ptr->text_end = 0;
   init_ptr->data_start = 0xffffffff;
   init_ptr->data_end = 0;
   init_ptr->entry_point = 0;
   if (init_ptr->mem_stack_size == (UINT32) -1)
       init_ptr->mem_stack_size = MEM_STACK_SIZE;
   if (init_ptr->reg_stack_size == (UINT32) -1)
       init_ptr->reg_stack_size = REG_STACK_SIZE;
   init_ptr->argstring = (char *) 0;
  return(SUCCESS);
};


INT32
Mini_send_init_info(info_ptr)
INIT_INFO	*info_ptr;
{
   INT32	retval;

   /* Align INIT values to word boundaries */
   info_ptr->text_start = ALIGN32(info_ptr->text_start);
   info_ptr->text_end = ALIGN32(info_ptr->text_end);
   info_ptr->data_start = ALIGN32(info_ptr->data_start);
   info_ptr->data_end = ALIGN32(info_ptr->data_end);
   info_ptr->mem_stack_size = ALIGN32(info_ptr->mem_stack_size);
   info_ptr->reg_stack_size = ALIGN32(info_ptr->reg_stack_size);

   /* Send INIT message */

   if ((retval = Mini_init (info_ptr->text_start,
			    info_ptr->text_end,
			    info_ptr->data_start,
			    info_ptr->data_end,
			    info_ptr->entry_point,
			    info_ptr->mem_stack_size,
			    info_ptr->reg_stack_size,
			    info_ptr->argstring)) != SUCCESS) {
       warning(EMINIT);
       return(FAILURE);
   }; 
   return (SUCCESS);

}  /* Mini_send_init_info */



/*
** This function is used to load a COFF file.  Depending on
** the global variable "target_interface", data will be loaded
** by either EB29K shared memory, PCEB shared memory, EB030
** shared memory or serially.
**
** In addition, the global data structure "init" is updated.
** This data structure maintains the entry point and various
** other target initialization parameters.
*/

INT32
Mini_load_coff(filename, mspace, sym, Section, quietmode)
   char *filename;
   int   quietmode;
   INT32	sym;
   INT32	Section;
   INT32	mspace;
   {
   unsigned short  COFF_sections;
   INT32  flags;
   INT32  memory_space;
   INT32  address;
   INT32  byte_count;
   INT32  temp_byte_count;
   INT32	bytes_ret;

   struct  filehdr      COFF_header;
   struct  aouthdr      COFF_aout_header;
   struct  scnhdr      COFF_section_header;

    if (!quietmode) {
       fprintf(stderr, "loading %s\n", filename);
       if (io_config.echo_mode == (INT32) TRUE)
          fprintf(io_config.echo_file, "loading %s\n", filename);
    }

   /* Open the COFF input file (if we can) */
   if ((coff_in = fopen(filename, FILE_OPEN_FLAG)) == NULL) {
      warning (EMOPEN); return(FAILURE);
   };

   /* Read in COFF header information */
   if (fread((char *)&COFF_header, sizeof(struct filehdr), 1, coff_in) != 1) {
      fclose(coff_in); warning(EMHDR); return (FAILURE);
   };


   /* Is it an Am29000 COFF File? */
   if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x7a01) &&
       (COFF_header.f_magic != 0x17b) && (COFF_header.f_magic != 0x7b01)) {
      fclose(coff_in); warning (EMMAGIC); return (FAILURE);
   }

   /* Get number of COFF sections */
   if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x017b))
      convert16((BYTE *) &COFF_header.f_nscns);
   COFF_sections = (unsigned short) COFF_header.f_nscns;

   /* Read in COFF a.out header information (if we can) */
   if (COFF_header.f_opthdr > 0) {
      if (fread((char *)&COFF_aout_header, sizeof(struct aouthdr), 
						   1, coff_in) != 1) {
         fclose(coff_in); warning (EMAOUT); return (FAILURE);
      };
      /* Set entry point in INIT message */
      init_info.entry_point = COFF_aout_header.entry;
      if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x017b)) {
         convert16((BYTE *) &COFF_header.f_opthdr);
         convert32((BYTE *) &init_info.entry_point);
      }
   }


   /*
   ** Process COFF section headers
   */

   /* Process all sections */
   while ((int) COFF_sections--) {

      fseek (coff_in, (long) (FILHSZ+(int)COFF_header.f_opthdr+
			      SCNHSZ*(COFF_header.f_nscns-COFF_sections-1)), 
			      FROM_BEGINNING);

      if (fread(&COFF_section_header, 1, SCNHSZ, coff_in) != SCNHSZ) {
          fclose(coff_in); warning (EMSCNHDR); return (FAILURE);
      }

      if ((COFF_header.f_magic != 0x17a) && (COFF_header.f_magic != 0x017b)) {
         convert32((BYTE *) &(COFF_section_header.s_paddr));
         convert32((BYTE *) &(COFF_section_header.s_scnptr));
         convert32((BYTE *) &(COFF_section_header.s_size));
         convert32((BYTE *) &(COFF_section_header.s_flags));
       }

      address = COFF_section_header.s_paddr;
      byte_count = COFF_section_header.s_size;
      flags = COFF_section_header.s_flags;

      /* Print downloading messages (if necessary) */
      if ((flags == (INT32) STYP_TEXT) || (flags == (INT32) (STYP_TEXT | STYP_ABS))) {
	 memory_space = I_MEM;
         init_info.text_start = MIN((ADDR32) address,
				    (ADDR32) init_info.text_start);
         init_info.text_end = MAX((ADDR32) (address + byte_count), 
				  (ADDR32) init_info.text_end);
      } else if ((flags == (INT32) STYP_DATA) || (flags == (INT32) (STYP_DATA | STYP_ABS)) ||
          (flags == (INT32) STYP_LIT) || (flags == (INT32) (STYP_LIT | STYP_ABS)) ||
          (flags == (INT32) STYP_BSS) || (flags == (INT32) (STYP_BSS | STYP_ABS))) {
	 memory_space = D_MEM;
         init_info.data_start = MIN((ADDR32) address,
				    (ADDR32) init_info.data_start);
         init_info.data_end = MAX((ADDR32) (address + byte_count),
				  (ADDR32)  init_info.data_end);
      } else {
	 print_ign_msg(quietmode, address, byte_count);
	 flags = (INT32) 0;
      }

      if ((flags == (INT32) STYP_BSS) || (flags == (INT32) (STYP_BSS | STYP_ABS))) {
      /* Clear BSS section */
   	if (flags & Section) {
           print_ld_msg(flags,quietmode,address,byte_count);
           if (Mini_fill ((INT32)  D_MEM,
                           (ADDR32) address,
                           (INT32)  (byte_count+3)/4,
			   4 /* fill zeroes */,
			   "\0\0\0") != SUCCESS)  {
	           (void) fclose(coff_in); warning(EMFILL); return(FAILURE);
	    };
           print_end_msg(flags,quietmode,address,byte_count);
	 }
      } else if (flags & Section) { /* not a BSS or COmment */
	 if (flags == (INT32) (flags & Section)) {
	   fseek (coff_in, COFF_section_header.s_scnptr, FROM_BEGINNING);
           while (byte_count > 0) {
             temp_byte_count = MIN((INT32) byte_count, (INT32) sizeof(buffer));
             if (fread((char *) buffer, (int) temp_byte_count, 1, coff_in) != 1) {
                fclose(coff_in); warning (EMSCN); return (FAILURE);
	     };
	     print_ld_msg(flags, quietmode,address, temp_byte_count);
             /* Write to 29K memory*/
	     if (Mini_write_req ((INT32)  memory_space,
                            (ADDR32) address,
                            (INT32)  (temp_byte_count+3)/4,
			    (INT16) 4, /* size */
			    &bytes_ret,
                            (BYTE *) buffer,
			    (INT32) FALSE) != SUCCESS) {
	            warning(EMWRITE); 
		    return(FAILURE);
	     }
             address = address + temp_byte_count;
             byte_count = byte_count - temp_byte_count;
           };
	   print_end_msg(flags, quietmode, COFF_section_header.s_paddr,
					 COFF_section_header.s_size);
	 };
      }
   }  /* end while */

   (void) fclose(coff_in);
   return (SUCCESS);

   }   /* end Mini_loadcoff() */


int
SetSections(string)
char	*string;
{
  int	i;

  if (string[0] != '-')
    return (-1);  /* not section options */

  Sections = STYP_ABS;
  for (i=1; string[i] != '\0'; i++) {
     switch (string[i]) {
       case	't':
       case	'T':
	 Sections = Sections | STYP_TEXT;
	 break;
       case	'd':
       case	'D':
	 Sections = Sections | STYP_DATA;
	 break;
       case	'b':
       case	'B':
	 Sections = Sections | STYP_BSS;
	 break;
       case	'l':
       case	'L':
	 Sections = Sections | STYP_LIT;
	 break;
       default:
         return (EMSYNTAX);
     }
  }
  return (0);
}

void
print_ld_msg(flags, mode, address, byte_count)
INT32	flags;
int	mode;
ADDR32	address;
INT32	byte_count;
{
   if (!mode) {
     if (flags & (INT32) STYP_BSS)
       fprintf(stderr, "Clearing ");
     else
       fprintf(stderr, "Loading ");

     if ((flags == (INT32) STYP_TEXT) || (flags == (INT32) (STYP_TEXT|STYP_ABS)))
       fprintf(stderr, "TEXT ");
     else if (flags & (INT32) STYP_DATA)
       fprintf(stderr, "DATA ");
     else if (flags & (INT32) STYP_LIT)
       fprintf(stderr, "LIT ");
     else if (flags & (INT32) STYP_BSS)
       fprintf(stderr, "BSS ");
     fprintf(stderr, "section from 0x%08lx to 0x%08lx\r", 
	       address, (ADDR32) (address+byte_count));
   }
}

void
print_ign_msg(mode, address, byte_count)
int	mode;
ADDR32	address;
INT32	byte_count;
{
  if (!mode) 
    fprintf(stderr, "Ignoring COMMENT section (%ld bytes) ...\n", byte_count);
}

void
print_end_msg(flags, mode,address,size)
INT32	flags;
int	mode;
ADDR32	address;
INT32	size;
{
   if (!mode) {
     if (flags & (INT32) STYP_BSS)
       fprintf(stderr, "Cleared  ");
     else
       fprintf(stderr, "Loaded  ");
     if (io_config.echo_mode == (INT32) TRUE) {
       if (flags & (INT32) STYP_BSS)
         fprintf(io_config.echo_file, "Cleared  ");
       else
         fprintf(io_config.echo_file, "Loaded  ");
     }

     if ((flags == (INT32) STYP_TEXT) || 
		     (flags == (INT32) (STYP_TEXT|STYP_ABS)))
       fprintf(stderr, "TEXT ");
     else if (flags & (INT32) STYP_DATA)
       fprintf(stderr, "DATA ");
     else if (flags & (INT32) STYP_LIT)
       fprintf(stderr, "LIT ");
     else if (flags & (INT32) STYP_BSS)
       fprintf(stderr, "BSS ");

     fprintf(stderr, "section from 0x%08lx to 0x%08lx\n", 
	       address, (ADDR32) (address+size));
     if (io_config.echo_mode == (INT32) TRUE) {
       if ((flags == (INT32) STYP_TEXT) || 
		     (flags == (INT32) (STYP_TEXT|STYP_ABS)))
         fprintf(io_config.echo_file, "TEXT ");
       else if (flags & (INT32) STYP_DATA)
         fprintf(io_config.echo_file, "DATA ");
       else if (flags & (INT32) STYP_LIT)
         fprintf(io_config.echo_file, "LIT ");
       else if (flags & (INT32) STYP_BSS)
         fprintf(io_config.echo_file, "BSS ");
  
       fprintf(io_config.echo_file, "section from 0x%08lx to 0x%08lx\n", 
	         address, (ADDR32) (address+size));
     }
   }
}
