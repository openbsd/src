static char _[] = " @(#)bkpt.c	5.20 93/07/30 16:38:20, Srini, AMD ";
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
 **       This module contains the functions used to provide breakpointing
 **	  capability.
 *****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>
#include "memspcs.h"
#include "main.h"
#include "monitor.h"
#include "miniint.h"
#include "error.h"

#ifdef	MSDOS
#include <stdlib.h>
#include <string.h>

#else
#include <strings.h>

#endif

/*
** Definitions
*/

int   get_addr_29k_m PARAMS((char *, struct addr_29k_t *, INT32));
int   addr_29k_ok PARAMS((struct addr_29k_t *));
int   get_word PARAMS((char *, INT32 *));
int   get_word_decimal PARAMS((char *, INT32 *));

INT32   clear_table PARAMS((void));
INT32   clear_bkpt PARAMS((struct addr_29k_t));
INT32   show_table PARAMS((void));
INT32   set_bkpt PARAMS((struct addr_29k_t,
			 INT32, 
			 INT32));

INT32 match_entry PARAMS((ADDR32 offset,
			  INT32 space,
			  int *id));
INT32 remove_from_table PARAMS((int id));

INT32 add_to_table PARAMS((ADDR32 offset, 
			   INT32 space,
			   INT32 passcnt,
			   INT32 bktype,
			   int id));

extern	struct	bkpt_t	*bkpt_table;

/*
** The function below is used in manipulating breakpoints.
** This function is called in the main command loop parser
** of the monitor.  The parameters passed to this function
** are:
**
** token - This is an array of pointers to strings.  Each string
**         referenced by this array is a "token" of the user's
**         input, translated to lower case.
**
** token_count - This is the number of tokens in "token".
**
*/


INT32
bkpt_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   INT32    result;
   struct addr_29k_t addr_29k;
   INT32  pass_count;
   INT32  bkpt_type;

   /*
   ** Clear breakpoint(s)
   */
   if (strcmp(token[0], "bc") == 0) {
      if (token_count == 1) {
         result = clear_table();
         return (result);
         }
      else

      /* Clear a specific breakpoints */
      if (token_count == 2) {
         result = get_addr_29k_m(token[1], &addr_29k, I_MEM);
         if (result != 0)
            return (result);
         result = addr_29k_ok(&addr_29k);
         if (result != 0)
            return (result);
	 if (addr_29k.memory_space == (INT32) GENERIC_SPACE)
	    addr_29k.memory_space = (INT32) I_MEM;
         result = clear_bkpt(addr_29k);
         return (result);
         }
      else  /* token_count != 1 or 2 */
         return (EMSYNTAX);
      }
   else

   /*
   ** Set breakpoint(s)
   */

   if ((strcmp(token[0], "b") == 0) ||
       (strcmp(token[0], "b050p") == 0) ||
       (strcmp(token[0], "b050v") == 0) ||
       (strcmp(token[0], "b050") == 0)) { /* b050 defaults to b050p */

      if (strcmp(token[0], "b") == 0)
         bkpt_type = BKPT_29000;
      else
      if (strcmp(token[0], "b050p") == 0)
         bkpt_type = BKPT_29050_BTE_0;	/* translation disabled */
      else
      if (strcmp(token[0], "b050v") == 0)
         bkpt_type = BKPT_29050_BTE_1;
      else
      if (strcmp(token[0], "b050") == 0)
         bkpt_type = BKPT_29050_BTE_0;	/* translation disabled */
      else
         return (EMSYNTAX);

      if (token_count == 1) {
         result = show_table();
         return (result);
         }
      else

      /* Set breakpoint with pass count of 1 */
      if (token_count == 2) {
         result = get_addr_29k_m(token[1], &addr_29k, I_MEM);
         if (result != 0)
            return (result);
         result = addr_29k_ok(&addr_29k);
         if (result != 0)
            return (result);
         /* The TIP checks the memory space for acceptance */
	 if (addr_29k.memory_space == (INT32) GENERIC_SPACE)
	    addr_29k.memory_space = (INT32) I_MEM;
         result = set_bkpt(addr_29k, (INT32) 1, bkpt_type);
         return (result);
         }
      else

      /* Set breakpoint with pass count */
      if (token_count == 3) {
         result = get_addr_29k_m(token[1], &addr_29k, I_MEM);
         if (result != 0)
            return (result);
         result = addr_29k_ok(&addr_29k);
         if (result != 0)
            return (result);
         result = get_word_decimal(token[2], &pass_count);
         if (result != 0)
            return (EMSYNTAX);
	 if (addr_29k.memory_space == (INT32) GENERIC_SPACE)
	    addr_29k.memory_space = (INT32) I_MEM;
         result = set_bkpt(addr_29k, pass_count, bkpt_type);
         return (result);
         }
      else  /* too many parms for set breakpoint */
      return (EMSYNTAX);
      }
   else /* not a proper 'b" command */
      return (EMSYNTAX);

   }  /* end bkpt_cmd() */



/*
** Functions used by bkpt_cmd()
*/


/*
** This function is used to remove a breakpoint from the
** target and from the host breakpoint table.
*/

INT32
clear_bkpt(addr_29k)
   struct addr_29k_t  addr_29k;
{
   int	breakid;
   INT32	retval;

   (void) match_entry (addr_29k.address, 
		       addr_29k.memory_space,
		       &breakid);

   /* Did we find the breakpoint? */
   if (breakid <= (int) 0) {
      warning (EMBKPTRM);
      return (FAILURE);
   };

   /* if a valid breakpoint entry is found */
   if ((retval = Mini_bkpt_rm (breakid))  != SUCCESS) {
      return(FAILURE);
   } else if (retval == SUCCESS) {
      /* remove breakpoint from table */
      if (remove_from_table(breakid) != SUCCESS) {
	 /* this shouldn't occur */
	 return(FAILURE);
      };
      return(SUCCESS);
   };

}  /* end clear_bkpt() */



/*
** This function is used to set a breakpoint on the
** target and in the host breakpoint table.
*/

INT32
set_bkpt(addr_29k, pass_count, bkpt_type)
   struct   addr_29k_t addr_29k;
   INT32    pass_count;
   INT32    bkpt_type;
   {
   INT32	retval;
   int	breakid;

   /* is there one already at the same place */
   (void) match_entry(addr_29k.address, 
		      addr_29k.memory_space,
		      &breakid);

   if (breakid > (int) 0) {
      warning (EMBKPTUSED);
      return (FAILURE);
   };

   /* else set the breakpoint */
   breakid = (int) 0;
   if ((retval = Mini_bkpt_set (addr_29k.memory_space,
				addr_29k.address,
				pass_count,
				bkpt_type,
				&breakid)) != SUCCESS) {
       return(FAILURE);
   } else  {
      /* Add breakpoint to table */
      if (breakid > (int) 0) { /* double checking */
	 return (add_to_table(addr_29k.address,
		      addr_29k.memory_space,
		      pass_count,
		      bkpt_type,
		      breakid));
      };
      return(FAILURE);
   };
}  /* end set_bkpt() */



INT32
add_to_table(offset, space, passcnt, bktype, id )
ADDR32	offset;
INT32	space;
INT32	passcnt;
INT32	bktype;
int	id;
{
  struct   bkpt_t  *temp, *temp2;

  if ((temp = (struct bkpt_t *) malloc (sizeof(struct bkpt_t))) == NULL) {
     return(EMALLOC);
  } else {
    temp->break_id = id;
    temp->memory_space =  space;
    temp->address = offset;
    temp->pass_count = passcnt;
    temp->curr_count = passcnt;
    temp->bkpt_type = bktype;
    temp->next = NULL;
  };

  if (bkpt_table == NULL) { /* first element */
     bkpt_table = temp;
  } else { /* add at end */
     temp2 = bkpt_table;
     while (temp2->next != NULL)
       temp2 = temp2->next;
     temp2->next = temp;   /* add */
  }
  return(SUCCESS);
}

INT32
match_entry(offset, space, id)
ADDR32	offset;
INT32	space;
int	*id;
{
  struct  bkpt_t  *temp;

  if (bkpt_table == NULL) { /* empty, so no match */
    *id = (int) 0;
    return(SUCCESS);
  } else {
    temp = bkpt_table;
    if ((temp->address == offset) && 
		     (temp->memory_space == space)) { /* match */
       *id = temp->break_id;
       return(SUCCESS);
    } else {
       while (temp->next != NULL) {
	  if ((temp->next->address == offset) && 
	      (temp->next->memory_space == space)) {
	     *id = temp->next->break_id;
	     return(SUCCESS);
	  } else {
	     temp = temp->next;
	  };
       }
       *id = (int) 0;
       return(SUCCESS);
    };
  };
}

INT32
remove_from_table(id)
int   id;
{
  struct  bkpt_t  *temp;

  if (bkpt_table == NULL) { /* empty table */
     return(FAILURE);
  } else {
     temp = bkpt_table;
     if (bkpt_table->break_id == id) { /* remove first element */
	bkpt_table = bkpt_table->next;
	return(SUCCESS);
     } else {
	while (temp->next != NULL) {
	  if (temp->next->break_id == id) {
	     temp->next = temp->next->next;
	     return(SUCCESS);
	  } else {
	     temp = temp->next;
	  };
	}
	return(FAILURE);
     };
  }
}

INT32
clear_table()
{
  struct bkpt_t  *tmp;
  INT32	retval;


  if (bkpt_table == NULL) { /* no entries */
    fprintf(stderr, "no breakpoints in use.\n");
    if (io_config.echo_mode == (INT32) TRUE)
       fprintf(io_config.echo_file, "no breakpoints in use.\n");
  } else {
     while (bkpt_table) {
        if ((retval = Mini_bkpt_rm (bkpt_table->break_id))  != SUCCESS) {
           return(FAILURE);
        } else if (retval == SUCCESS) {
           /* remove breakpoint from table */
	   tmp = bkpt_table;
           bkpt_table = bkpt_table->next;
	   (void) free ((char *) tmp);
        };
     };
  }
  return(SUCCESS);
}

INT32
show_table()
{
  struct  bkpt_t  *temp;
   INT32	retval;
   ADDR32	temp_addr;
   INT32	temp_space;
   INT32	curr_count;
   INT32	temp_passcnt, temp_bktype;
   int		i;

  if (bkpt_table == NULL) { /* no entries */
  } else {
    do {
       temp = bkpt_table;
       bkpt_table = bkpt_table->next;
       (void) free ((char *) temp);
    } while (bkpt_table != NULL);
  };

  for (i = 1;1;i++) {
     retval = Mini_bkpt_stat( i,
			      &temp_addr,
			      &temp_space,
			      &temp_passcnt,
			      &temp_bktype,
			      &curr_count);
     if ((retval == (INT32) MONBreakInvalid) ||
         (retval == (INT32) FAILURE)) {
	continue;
     } else if (retval == (INT32) MONBreakNoMore) {
	return (SUCCESS);
     } else {
	 /* add entry in the table */
	 if ((retval = add_to_table ((ADDR32) temp_addr,
		       		(INT32) temp_space,
		       		(INT32) temp_passcnt,
		       		(INT32) temp_bktype,
		       		i)) != SUCCESS)
		return (retval);
         /* Mark Am29050 breakpoints with a '*' */
         if (temp_bktype == BKPT_29050)
             fprintf(stderr, "*");
         else
             fprintf(stderr, " ");
         fprintf(stderr, "(%#05d: %#08lx[%#02d])\n", i, 
			       temp_addr, 
			       temp_passcnt);
	 if (io_config.echo_mode == (INT32) TRUE)  {
             if (temp_bktype == BKPT_29050)
                 fprintf(io_config.echo_file, "*");
             else
                fprintf(io_config.echo_file, " ");
             fprintf(io_config.echo_file, "(%#05d: %#08lx[%#02d])\n", i, 
				       temp_addr, 
				       temp_passcnt);
	 }
     };
  }
  return(SUCCESS);
}

