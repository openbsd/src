static char _[] = "@(#)mtip.c	2.14 92/01/13 18:04:36, AMD.";
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
 *      Engineers: MINIMON DEVELOPMENT TEAM MEMBERS, AMD.
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "messages.h"
#include "coff.h"
#include "memspcs.h"
#include "mtip.h"
#include "macros.h"

/* TIP Breakpoint table */
typedef	unsigned int	BreakIdType;

static struct break_table {
  BreakIdType		id;
  INT32		space;
  ADDR32	offset;
  INT32		count;
  INT32		type;
  ADDR32	BreakInst;	/* actual instruction */
  struct break_table *next;
};
 
struct break_table  *bp_table=NULL;

#define	BUFFER_SIZE	1024

static BYTE	buffer[BUFFER_SIZE];

int   	Mini_core_load PARAMS((char *corefile, INT32 space,
			       INT32 sects, int syms));  
void    add_to_bp_table PARAMS((BreakIdType *id, INT32 space, 
			ADDR32 offset, INT32 count, INT32 type, ADDR32 inst));
int   	get_from_bp_table PARAMS((BreakIdType id, INT32 *space, 
				 ADDR32 *offset, INT32 *count, 
				 INT32 *type, ADDR32 *inst));
int	remove_from_bp_table PARAMS((BreakIdType id));
int   	is_breakpt_at PARAMS((INT32 space, ADDR32 offset));

/* 
** Breakpoint code 
*/

void
add_to_bp_table(id, space, offset, count, type, inst)
BreakIdType	*id;
INT32	space;
ADDR32	offset;
INT32 	count;
INT32	type;
ADDR32	inst;
{
  static BreakIdType	current_break_id=1;
  struct break_table  	*temp, *temp2;

  if (bp_table == NULL) { /* first element */
    bp_table = (struct break_table *) malloc (sizeof(struct break_table));
    bp_table->id = current_break_id;
    bp_table->offset = offset;
    bp_table->space = space;
    bp_table->count = count;
    bp_table->type = type;
    bp_table->BreakInst = inst;
    bp_table->next = NULL;
  } else {
    temp2 = bp_table;
    temp = (struct break_table *) malloc (sizeof(struct break_table));
    temp->id = current_break_id;
    temp->offset = offset;
    temp->space = space;
    temp->count = count;
    temp->type = type;
    temp->BreakInst = inst;
    temp->next = NULL;
    while (temp2->next != NULL)
      temp2 = temp2->next;
    temp2->next = temp;
  };
  *id = current_break_id;
  current_break_id++;
}

int 
get_from_bp_table(id, space, offset, count, type, inst)
BreakIdType	id;
INT32	*space;
ADDR32	*offset;
INT32 	*count;
INT32	*type;
ADDR32	*inst;
{
  struct break_table  *temp;

  temp = bp_table;

  while (temp != NULL) {
    if (temp->id == id) {
       *offset = temp->offset;
       *space = temp->space;
       *count = temp->count;
       *type = temp->type;
       *inst = temp->BreakInst;
       return(0);
    } else {
      temp = temp->next;
    };
  }
  return(-1);
}

int
remove_from_bp_table(id)
BreakIdType	id;
{
  struct  break_table	*temp, *temp2;

  if (bp_table == NULL)
     return (-1);
  else {
    temp = bp_table;
    if (temp->id == id) { /* head of list */
       bp_table = bp_table->next;
       (void) free (temp);
       return (0); /* success */
    } else {
       while (temp->next != NULL) {
          if (temp->next->id == id) {
	     temp2 = temp->next;
	     temp->next = temp->next->next;
	     (void) free (temp2);
	     return (0); /* success */
          } else {
            temp = temp->next;
          }
       };
    }
  };
  return (-1);  /* failed */
}

int 
is_breakpt_at(space, offset)
INT32	space;
ADDR32	offset;
{
  struct break_table  *temp;

  temp = bp_table;

  while (temp != NULL) {
    if ((temp->space == space) && (temp->offset == offset)) {
       return(1); /* TRUE */
    } else {
      temp = temp->next;
    };
  }
  return(0); /* FALSE */
}

/* 
** Miscellaneous functions.
*/ 

int 
Mini_core_load(filename, space, sects, syms)
char *filename;
INT32	space;
INT32	sects;
int	syms;
  { 
   
   FILE  *coff_in;
   INT32  COFF_sections;
   INT32  i;
   int    read_count;
   int    section_type;
   int    host_endian;
   INT32  flags;
   INT32  memory_space;
   INT32  address;
   INT32  byte_count;
   INT32	write_count;
   INT32  temp_byte_count;
   INT16  temp_magic;
   INT16  temp_sections;

   struct  filehdr      COFF_header;
   struct  aouthdr      COFF_aout_header;
   struct  scnhdr      *COFF_section_header;
   struct  scnhdr      *temp_COFF_section_header;

   /* Open the COFF input file (if we can) */
   coff_in = fopen(filename, FILE_OPEN_FLAG);
   if (coff_in == NULL) {
      /* warning (EMOPEN);  */
      return(-1);
   }

   /*
   ** Process COFF header(s)
   */

   /* Read in COFF header information */
   read_count = fread((char *)&COFF_header,
                      sizeof(struct filehdr),
                      1, coff_in);

   /* Did we get it all? */
   if (read_count != 1) {
      fclose(coff_in);
      /* warning(EMHDR); */
      return (-1);
      }

   /* Is it an Am29000 COFF File? */
   temp_magic = COFF_header.f_magic;
   tip_convert16((BYTE *) &temp_magic);
   if (COFF_header.f_magic == SIPFBOMAGIC) {
      host_endian = TRUE;
      }
   else
   if (temp_magic == SIPFBOMAGIC) {
      host_endian = FALSE;
      }
   else
      {
      fclose(coff_in);
      /* warning (EMMAGIC); */
      return (-1);
      }

   /* Get number of COFF sections */
   temp_sections = COFF_header.f_nscns;
   if (host_endian == FALSE)
      tip_convert16((BYTE *) &temp_sections);
   COFF_sections = (INT32) temp_sections;

   /* Read in COFF a.out header information (if we can) */
   if (COFF_header.f_opthdr > 0) {
      read_count = fread((char *)&COFF_aout_header,
                         sizeof(struct aouthdr),
                         1, coff_in);

      /* Did we get it all? */
      if (read_count != 1) {
         fclose(coff_in);
         /* warning (EMAOUT); */
         return (-1);
         }

      }

   /*
   ** Process COFF section headers
   */

   /* Allocate space for section headers */
   (char *)COFF_section_header = (char *)
       malloc((unsigned) (COFF_sections * sizeof(struct scnhdr)));

   if (COFF_section_header == NULL) {
      fclose(coff_in);
      /* warning (EMALLOC); */
      return (-1);
      }

   /* Save the pointer to the malloc'ed data, so
   ** we can free it later. */
   temp_COFF_section_header = COFF_section_header;

   read_count = fread((char *)COFF_section_header,
                      sizeof(struct scnhdr),
                      (int) COFF_sections, coff_in);

   /* Did we get it all? */
   if (read_count != (int) COFF_sections) {
      fclose(coff_in);
      /*  warning (EMSCNHDR); */
      return (-1);
      }


   /* Process all sections */
   for (i=0; i<COFF_sections; i=i+1) {

      address = COFF_section_header->s_paddr;
      byte_count = COFF_section_header->s_size;
      flags = COFF_section_header->s_flags;

      if (host_endian == FALSE) {
         tip_convert32((BYTE *) &address);
         tip_convert32((BYTE *) &byte_count);
         tip_convert32((BYTE *) &flags);
         }

      /* Print downloading messages (if necessary) */
      if ((flags == STYP_TEXT) ||
          (flags == (STYP_TEXT | STYP_ABS))) {
         section_type = TEXT_SECTION;
         memory_space = I_MEM;
         }
      else
      if ((flags == STYP_DATA) ||
          (flags == (STYP_DATA | STYP_ABS))) {
         section_type = DATA_SECTION;
         memory_space = D_MEM;
         }
      else
      if ((flags == STYP_LIT) ||
          (flags == (STYP_LIT | STYP_ABS))) {
         section_type = LIT_SECTION;
         memory_space = D_MEM;
         }
      else
      if ((flags == STYP_BSS) ||
          (flags == (STYP_BSS | STYP_ABS))) {
         section_type = BSS_SECTION;
         memory_space = D_MEM;
         }
      else {
         section_type = UNKNOWN_SECTION;
      }

      /* Clear BSS sections in 29K data memory */
      if (section_type == BSS_SECTION) {
	 (void) memset ((char *) buffer, (int) '\0', sizeof(buffer));
	 while (byte_count > 0) {
	   write_count = (byte_count < (INT32) sizeof(buffer)) ?
				byte_count : (INT32) sizeof (buffer);
	   if(Mini_write_memory ((INT32) memory_space,
			      (ADDR32) address,
			      (INT32) write_count,
			      (BYTE *) buffer) != SUCCESS) {
   		(void) fclose(coff_in);
		return(-1);
	   }
	   address = address + write_count;
	   byte_count = byte_count - write_count;
	 }
       } else

      /* Else send data to the target */
      while (byte_count > 0) {

         temp_byte_count = MIN(byte_count, (INT32) sizeof(buffer));

         read_count = fread((char *) buffer,
                            (int) temp_byte_count,
                            1, coff_in);

         /* Did we get it all? */
         if (read_count != 1) {
            fclose(coff_in);
            /* warning (EMSCN); */
            return (-1);
            }

         /* Write to 29K memory*/
         if (section_type != UNKNOWN_SECTION) {
           if (Mini_write_memory ((INT32)  memory_space,
                            	(ADDR32) address,
                            	(INT32)  temp_byte_count,
                            	(BYTE *) buffer) != SUCCESS) {
	     /* warning(EMWRITE); */
   	     (void) fclose(coff_in);
	     return(-1);
	   };
	 }

         address = address + temp_byte_count;

         byte_count = byte_count - temp_byte_count;

         }  /* end while */

      COFF_section_header++;

   }  /* end for loop */

   (void) free((char *)temp_COFF_section_header);
   (void) fclose(coff_in);

   return (0);

   }   /* end Mini_loadcoff() */

