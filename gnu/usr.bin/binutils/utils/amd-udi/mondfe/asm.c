static char _[] = " @(#)asm.c	5.23 93/10/26 10:17:03, Srini, AMD";
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
 * This module supports the assemble command to assemble 29K instructions
 * in memory.
 *****************************************************************************
 */


#include <stdio.h>
#include "opcodes.h"
#include "memspcs.h"
#include "main.h"
#include "monitor.h"
#include "macros.h"
#include "miniint.h"
#include "error.h"

#ifdef	MSDOS
#include <string.h>
#define	strcasecmp	stricmp
#else
#include <string.h>
#endif


/*
** There are approximately 23 different instruction formats for the
** Am29000.  Instructions are assembled using one of these formats.
**
** Note:  Opcodes in the "switch" statement are sorted in numerical
**        order.
**
*/


int  	get_addr_29k_m PARAMS((char *, struct addr_29k_t *, INT32));
int  	addr_29k_ok PARAMS((struct addr_29k_t *));
void 	convert32 PARAMS((BYTE *));
int  	set_data PARAMS((BYTE *, BYTE *, int));

int  asm_instr PARAMS((struct instr_t *, char **, int));

int  asm_arith_logic PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_load_store PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_vector PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_no_parms PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_one_parms PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_float PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_call_jmp PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_calli_jmpi PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_class PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_clz PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_const PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_consth PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_convert PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_div0 PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_exhws PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_jmp PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_jmpi PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_mfsr PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_mtsr PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_mtsrim PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_mftlb PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_mttlb PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_sqrt PARAMS((struct instr_t *, struct addr_29k_t *, int));
int  asm_emulate PARAMS((struct instr_t *, struct addr_29k_t *, int));

extern	void	Mini_poll_kbd PARAMS((char  *cmd_buffer, int size, int mode));
extern	int	Mini_cmdfile_input PARAMS((char  *cmd_buffer, int size));
extern	int   	tokenize_cmd PARAMS((char *, char **));
extern 	void 	lcase_tokens PARAMS((char **, int));
extern	INT32 	do_assemble PARAMS(( struct addr_29k_t	addr_29k,
				     char	*token[],
				     int	token_count));
#ifndef XRAY

extern 	char  	cmd_buffer[];

#define	MAX_ASM_TOKENS	15
static	char	*asm_token[MAX_ASM_TOKENS];
static	int	asm_token_count;

/*
** This function is used to assemble an instruction.  The command
** takes as parameters an array of strings (*token[]) and a
** count (token_count) which gives the number of tokens in the
** array.  These tokens should have the following values:
**
** token[0] - 'a' (the assemble command)
** token[1] - <address> (the address to assemble instruction at)
** token[2] - <opcode>  (the 29K opcode nmemonic)
** token[3] to token[n] - parameters to the assembly instruction.
**
*/

INT32
asm_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   INT32		result;
   struct addr_29k_t 	addr_29k;
   int		asm_done;

   /*
   ** Parse parameters
   */

   if ((token_count < 2) || (token_count > 9)) {
      return (EMSYNTAX);
   } else if (token_count == 2) {
      /* Get address of assembly */
      result = get_addr_29k_m(token[1], &addr_29k, I_MEM);
      if (result != 0)
         return (result);
      result = addr_29k_ok(&addr_29k);
      if (result != 0)
         return (result);
      asm_done = 0;
      fprintf(stderr, "0x%08lx:\t", addr_29k.address);
      do {
        if (io_config.cmd_file_io == TRUE) {
	     if (Mini_cmdfile_input(cmd_buffer, BUFFER_SIZE) == SUCCESS) {
               fprintf(stderr, "%s", cmd_buffer); 
	     } else {
               Mini_poll_kbd(cmd_buffer, BUFFER_SIZE, BLOCK); /* block */
	     }
	} else {
             Mini_poll_kbd(cmd_buffer, BUFFER_SIZE, BLOCK); /* block */
	}
	if (io_config.log_file)  /* make a log file */
#ifdef	MSDOS
            fprintf(io_config.log_file, "%s\n", cmd_buffer);
#else
            fprintf(io_config.log_file, "%s", cmd_buffer);
#endif
     	if (io_config.echo_mode == (INT32) TRUE)
#ifdef	MSDOS
            fprintf(io_config.echo_file, "%s\n", cmd_buffer);
#else
            fprintf(io_config.echo_file, "%s", cmd_buffer);
#endif
        asm_token_count = tokenize_cmd(cmd_buffer, asm_token);
        lcase_tokens(asm_token, asm_token_count);
	if (strcmp(token[0], ".") == 0)
	  asm_done = 1;
	else {
          result= do_assemble(addr_29k, &asm_token[0], asm_token_count);
	  if (result != SUCCESS)
	    warning (result);
	  else
	    addr_29k.address = addr_29k.address + 4;
	  fprintf(stderr, "0x%08lx:\t", addr_29k.address);
	}
      } while (asm_done != 1);
   } else {
      /* Get address of assembly */
      result = get_addr_29k_m(token[1], &addr_29k, I_MEM);
      if (result != 0)
         return (result);
      result = addr_29k_ok(&addr_29k);
      if (result != 0)
         return (result);
      return (do_assemble(addr_29k, &token[2], (token_count-2)));
   }
   return (SUCCESS);
}


INT32
do_assemble(addr_29k, token, token_count)
struct addr_29k_t	addr_29k;
char			*token[];
int			token_count;
{
   INT32    result;
   struct instr_t    instr;

   INT32	retval;
   BYTE		*write_data;
   INT32	bytes_ret;
   INT32	hostendian;	/* for UDI conformant */

   /* Initialize instr */
   instr.op = 0;
   instr.c  = 0;
   instr.a  = 0;
   instr.b  = 0;

   /* Assemble instruction */
   result = asm_instr(&instr, &(token[0]), token_count);

   if (result != 0)
      return (EMSYNTAX);

   /* Will the data overflow the message buffer?  done in TIP */
   write_data = (BYTE *) &instr;

   hostendian = FALSE;
   if ((retval = Mini_write_req (addr_29k.memory_space,
				 addr_29k.address, 
				 1, /* count */
				 (INT16) sizeof(INST32),  /* size */
				 &bytes_ret,
				 write_data,
				 hostendian)) != SUCCESS) {
      return(FAILURE);
   }; 
   return (SUCCESS);
}
#endif

/*
** This function is used to assemble a single Am29000 instruction.
** The token[] array contains the lower-case tokens for a single
** assembler instruction.  The token_count contains the number of
** tokens in the array.  This number should be at least 1 (as in the
** cases of instructions like IRET) and at most 5 (for instructions
** like LOAD).
*/

#ifdef XRAY
  extern struct t_inst_table {
	char  *inst_mnem;
	unsigned char	oprn_fmt;
} inst_table[];
#endif

int
asm_instr(instr, token, token_count)
   struct   instr_t *instr;
   char    *token[];
   int      token_count;
   {
   int    i;
   int    result;
   struct addr_29k_t parm[10];
   char   temp_opcode[20];
   char  *temp_ptr;
   int    opcode_found;
   static char  *str_0x40="0x40";
   static char  *str_gr1="gr1";


   /* Is token_count valid, and is the first token a string? */
   if ((token_count < 1) ||
       (token_count > 7) ||
       (strcmp(token[0], "") == 0))
      return (EMSYNTAX);

   /* Get opcode */

   /*
   ** Note:  Since the opcode_name[] string used in the disassembler
   ** uses padded strings, we cannot do a strcmp().  We canot do a
   ** strncmp() of the length of token[0] either, since "and" will
   ** match up (for instance) with "andn".  So we copy the string,
   ** null terminate at the first pad character (space), and then
   ** compare.  This is inefficient, but necessary.
   */

   i=0;
   opcode_found = FALSE;
   while ((i<256) && (opcode_found != TRUE)) {
#ifdef XRAY
      result = strcasecmp(token[0], inst_table[i].inst_mnem);
#else
      temp_ptr = strcpy(temp_opcode, opcode_name[i]);

      if (strcmp(temp_ptr, "") != 0)
         temp_ptr = strtok(temp_opcode, " ");
      result = strcmp(token[0], temp_ptr);
#endif

      if (result == 0) {
         opcode_found = TRUE;
         instr->op = (BYTE) i;
         }
      i = i + 1;
      }  /* end while */

   /* Check for a NOP */
   if ((opcode_found == FALSE) &&
       (strcasecmp(token[0], "nop") == 0)) {
      opcode_found = TRUE;
      instr->op = ASEQ0;
      /* Fake up tokens to give "aseq 0x40,gr1,gr1" */
      token_count = 4;
      token[1] = str_0x40;
      token[2] = str_gr1;
      token[3] = str_gr1;
      }

   if (opcode_found == FALSE)
      return (EMSYNTAX);

   if ((strcasecmp(token[0], "iretinv") == 0) ||
       (strcasecmp(token[0], "inv") == 0) ) {
       /* iretinv and inv instructions */
      for (i=1; i<token_count; i=i+1) {
         result = get_addr_29k_m(token[i], &(parm[i-1]), GENERIC_SPACE);
         if (result != 0)
            return (result);
      }
   } else {
   /* Make the other tokens into addr_29k */
   for (i=1; i<token_count; i=i+1) {
      result = get_addr_29k_m(token[i], &(parm[i-1]), I_MEM);
      if (result != 0)
         return (result);
      /* Check if register values are legal */
      if (ISREG(parm[i-1].memory_space)) {
         result = addr_29k_ok(&(parm[i-1]));
         if (result != 0)
            return (EMBADREG);
         }
      /* Set bit 7 if a LOCAL_REG */
      if (parm[i-1].memory_space == LOCAL_REG)
         parm[i-1].address = (parm[i-1].address | 0x80);
      }
   }


   switch (instr->op) {   

      /* Opcodes 0x00 to 0x0F */
      case ILLEGAL_00:  result = EMSYNTAX;
                        break;
      case CONSTN:      result = asm_const(instr, parm, 2);
                        break;
      case CONSTH:      result = asm_consth(instr, parm, 2);
                        break;
      case CONST:       result = asm_const(instr, parm, 2);
                        break;
      case MTSRIM:      result = asm_mtsrim(instr, parm, 2);
                        break;
      case CONSTHZ:     result = asm_const(instr, parm, 2);
                        break;
      case LOADL0:      result = asm_load_store(instr, parm, 4);
                        break;
      case LOADL1:      result = asm_load_store(instr, parm, 4);
                        break;
      case CLZ0:        result = asm_clz(instr, parm, 2);
                        break;
      case CLZ1:        result = asm_clz(instr, parm, 2);
                        break;
      case EXBYTE0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case EXBYTE1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case INBYTE0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case INBYTE1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case STOREL0:     result = asm_load_store(instr, parm, 4);
                        break;
      case STOREL1:     result = asm_load_store(instr, parm, 4);
                        break;

      /* Opcodes 0x10 to 0x1F */
      case ADDS0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDS1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDU0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDU1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADD0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADD1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case LOAD0:       result = asm_load_store(instr, parm, 4);
                        break;
      case LOAD1:       result = asm_load_store(instr, parm, 4);
                        break;
      case ADDCS0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDCS1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDCU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDCU1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDC0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ADDC1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case STORE0:      result = asm_load_store(instr, parm, 4);
                        break;
      case STORE1:      result = asm_load_store(instr, parm, 4);
                        break;

      /* Opcodes 0x20 to 0x2F */
      case SUBS0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBS1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBU0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBU1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUB0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUB1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case LOADSET0:    result = asm_load_store(instr, parm, 4);
                        break;
      case LOADSET1:    result = asm_load_store(instr, parm, 4);
                        break;
      case SUBCS0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBCS1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBCU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBCU1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBC0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBC1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPBYTE0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPBYTE1:     result = asm_arith_logic(instr, parm, 3);
                        break;


      /* Opcodes 0x30 to 0x3F */
      case SUBRS0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRS1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRU1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBR0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBR1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case LOADM0:      result = asm_load_store(instr, parm, 4);
                        break;
      case LOADM1:      result = asm_load_store(instr, parm, 4);
                        break;
      case SUBRCS0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRCS1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRCU0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRCU1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRC0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case SUBRC1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case STOREM0:     result = asm_load_store(instr, parm, 4);
                        break;
      case STOREM1:     result = asm_load_store(instr, parm, 4);
                        break;

      /* Opcodes 0x40 to 0x4F */
      case CPLT0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLT1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLTU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLTU1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLE0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLE1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLEU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPLEU1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGT0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGT1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGTU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGTU1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGE0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGE1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGEU0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPGEU1:      result = asm_arith_logic(instr, parm, 3);
                        break;

      /* Opcodes 0x50 to 0x5F */
      case ASLT0:       result = asm_vector(instr, parm, 3);
                        break;
      case ASLT1:       result = asm_vector(instr, parm, 3);
                        break;
      case ASLTU0:      result = asm_vector(instr, parm, 3);
                        break;
      case ASLTU1:      result = asm_vector(instr, parm, 3);
                        break;
      case ASLE0:       result = asm_vector(instr, parm, 3);
                        break;
      case ASLE1:       result = asm_vector(instr, parm, 3);
                        break;
      case ASLEU0:      result = asm_vector(instr, parm, 3);
                        break;
      case ASLEU1:      result = asm_vector(instr, parm, 3);
                        break;
      case ASGT0:       result = asm_vector(instr, parm, 3);
                        break;
      case ASGT1:       result = asm_vector(instr, parm, 3);
                        break;
      case ASGTU0:      result = asm_vector(instr, parm, 3);
                        break;
      case ASGTU1:      result = asm_vector(instr, parm, 3);
                        break;
      case ASGE0:       result = asm_vector(instr, parm, 3);
                        break;
      case ASGE1:       result = asm_vector(instr, parm, 3);
                        break;
      case ASGEU0:      result = asm_vector(instr, parm, 3);
                        break;
      case ASGEU1:      result = asm_vector(instr, parm, 3);
                        break;

      /* Opcodes 0x60 to 0x6F */
      case CPEQ0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPEQ1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPNEQ0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case CPNEQ1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case MUL0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case MUL1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case MULL0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case MULL1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case DIV0_OP0:    result = asm_div0(instr, parm, 2);
                        break;
      case DIV0_OP1:    result = asm_div0(instr, parm, 2);
                        break;
      case DIV_OP0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case DIV_OP1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case DIVL0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case DIVL1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case DIVREM0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case DIVREM1:     result = asm_arith_logic(instr, parm, 3);
                        break;

      /* Opcodes 0x70 to 0x7F */
      case ASEQ0:       result = asm_vector(instr, parm, 3);
                        break;
      case ASEQ1:       result = asm_vector(instr, parm, 3);
                        break;
      case ASNEQ0:      result = asm_vector(instr, parm, 3);
                        break;
      case ASNEQ1:      result = asm_vector(instr, parm, 3);
                        break;
      case MULU0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case MULU1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ILLEGAL_76:  result = EMSYNTAX;
                        break;
      case ILLEGAL_77:  result = EMSYNTAX;
                        break;
      case INHW0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case INHW1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case EXTRACT0:    result = asm_arith_logic(instr, parm, 3);
                        break;
      case EXTRACT1:    result = asm_arith_logic(instr, parm, 3);
                        break;
      case EXHW0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case EXHW1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case EXHWS:       result = asm_exhws(instr, parm, 2);
                        break;
      case ILLEGAL_7F:  result = EMSYNTAX;
                        break;

      /* Opcodes 0x80 to 0x8F */
      case SLL0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case SLL1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case SRL0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case SRL1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case ILLEGAL_84:  result = EMSYNTAX;
                        break;
      case ILLEGAL_85:  result = EMSYNTAX;
                        break;
      case SRA0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case SRA1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case IRET:        
			result = asm_no_parms(instr, parm, 0);
                        break;
      case HALT_OP:     result = asm_no_parms(instr, parm, 0);
                        break;
      case ILLEGAL_8A:  result = EMSYNTAX;
                        break;
      case ILLEGAL_8B:  result = EMSYNTAX;
                        break;
      case IRETINV:     
			if (token_count > 1)
			    result = asm_one_parms(instr, parm, 1);
		        else
			    result = asm_no_parms(instr, parm, 0);
                        break;
      case ILLEGAL_8D:  result = EMSYNTAX;
                        break;
      case ILLEGAL_8E:  result = EMSYNTAX;
                        break;
      case ILLEGAL_8F:  result = EMSYNTAX;
                        break;

      /* Opcodes 0x90 to 0x9F */
      case AND_OP0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case AND_OP1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case OR_OP0:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case OR_OP1:      result = asm_arith_logic(instr, parm, 3);
                        break;
      case XOR_OP0:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case XOR_OP1:     result = asm_arith_logic(instr, parm, 3);
                        break;
      case XNOR0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case XNOR1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case NOR0:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case NOR1:        result = asm_arith_logic(instr, parm, 3);
                        break;
      case NAND0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case NAND1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ANDN0:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case ANDN1:       result = asm_arith_logic(instr, parm, 3);
                        break;
      case SETIP:       result = asm_float(instr, parm, 3);
                        break;
      case INV:         
			  if (token_count > 1)
			    result = asm_one_parms(instr, parm, 1);
			  else
			    result = asm_no_parms(instr, parm, 0);
                        break;

      /* Opcodes 0xA0 to 0xAF */
      case JMP0:        result = asm_jmp(instr, parm, 1);
                        break;
      case JMP1:        result = asm_jmp(instr, parm, 1);
                        break;
      case ILLEGAL_A2:  result = EMSYNTAX;
                        break;
      case ILLEGAL_A3:  result = EMSYNTAX;
                        break;
      case JMPF0:       result = asm_call_jmp(instr, parm, 2);
                        break;
      case JMPF1:       result = asm_call_jmp(instr, parm, 2);
                        break;
      case ILLEGAL_A6:  result = EMSYNTAX;
                        break;
      case ILLEGAL_A7:  result = EMSYNTAX;
                        break;
      case CALL0:       result = asm_call_jmp(instr, parm, 2);
                        break;
      case CALL1:       result = asm_call_jmp(instr, parm, 2);
                        break;
      case ORN_OP0:  	result = EMSYNTAX;
                        break;
      case ORN_OP1:  	result = EMSYNTAX;
                        break;
      case JMPT0:       result = asm_call_jmp(instr, parm, 2);
                        break;
      case JMPT1:       result = asm_call_jmp(instr, parm, 2);
                        break;
      case ILLEGAL_AE:  result = EMSYNTAX;
                        break;
      case ILLEGAL_AF:  result = EMSYNTAX;
                        break;

      /* Opcodes 0xB0 to 0xBF */
      case ILLEGAL_B0:  result = EMSYNTAX;
                        break;
      case ILLEGAL_B1:  result = EMSYNTAX;
                        break;
      case ILLEGAL_B2:  result = EMSYNTAX;
                        break;
      case ILLEGAL_B3:  result = EMSYNTAX;
                        break;
      case JMPFDEC0:    result = asm_call_jmp(instr, parm, 2);
                        break;
      case JMPFDEC1:    result = asm_call_jmp(instr, parm, 2);
                        break;
      case MFTLB:       result = asm_mftlb(instr, parm, 2);
                        break;
      case ILLEGAL_B7:  result = EMSYNTAX;
                        break;
      case ILLEGAL_B8:  result = EMSYNTAX;
                        break;
      case ILLEGAL_B9:  result = EMSYNTAX;
                        break;
      case ILLEGAL_BA:  result = EMSYNTAX;
                        break;
      case ILLEGAL_BB:  result = EMSYNTAX;
                        break;
      case ILLEGAL_BC:  result = EMSYNTAX;
                        break;
      case ILLEGAL_BD:  result = EMSYNTAX;
                        break;
      case MTTLB:       result = asm_mttlb(instr, parm, 2);
                        break;
      case ILLEGAL_BF:  result = EMSYNTAX;
                        break;

      /* Opcodes 0xC0 to 0xCF */
      case JMPI:        result = asm_jmpi(instr, parm, 1);
                        break;
      case ILLEGAL_C1:  result = EMSYNTAX;
                        break;
      case ILLEGAL_C2:  result = EMSYNTAX;
                        break;
      case ILLEGAL_C3:  result = EMSYNTAX;
                        break;
      case JMPFI:       result = asm_calli_jmpi(instr, parm, 2);
                        break;
      case ILLEGAL_C5:  result = EMSYNTAX;
                        break;
      case MFSR:        result = asm_mfsr(instr, parm, 2);
                        break;
      case ILLEGAL_C7:  result = EMSYNTAX;
                        break;
      case CALLI:       result = asm_calli_jmpi(instr, parm, 2);
                        break;
      case ILLEGAL_C9:  result = EMSYNTAX;
                        break;
      case ILLEGAL_CA:  result = EMSYNTAX;
                        break;
      case ILLEGAL_CB:  result = EMSYNTAX;
                        break;
      case JMPTI:       result = asm_calli_jmpi(instr, parm, 2);
                        break;
      case ILLEGAL_CD:  result = EMSYNTAX;
                        break;
      case MTSR:        result = asm_mtsr(instr, parm, 2);
                        break;
      case ILLEGAL_CF:  result = EMSYNTAX;
                        break;

      /* Opcodes 0xD0 to 0xDF */
      case ILLEGAL_D0:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D1:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D2:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D3:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D4:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D5:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D6:  result = EMSYNTAX;
                        break;
      case EMULATE:     result = asm_emulate(instr, parm, 3);
                        break;
      case ILLEGAL_D8:  result = EMSYNTAX;
                        break;
      case ILLEGAL_D9:  result = EMSYNTAX;
                        break;
      case ILLEGAL_DA:  result = EMSYNTAX;
                        break;
      case ILLEGAL_DB:  result = EMSYNTAX;
                        break;
      case ILLEGAL_DC:  result = EMSYNTAX;
                        break;
      case ILLEGAL_DD:  result = EMSYNTAX;
                        break;
      case MULTM:       result = asm_float(instr, parm, 3);
                        break;
      case MULTMU:      result = asm_float(instr, parm, 3);
                        break;

      /* Opcodes 0xE0 to 0xEF */
      case MULTIPLY:    result = asm_float(instr, parm, 3);
                        break;
      case DIVIDE:      result = asm_float(instr, parm, 3);
                        break;
      case MULTIPLU:    result = asm_float(instr, parm, 3);
                        break;
      case DIVIDU:      result = asm_float(instr, parm, 3);
                        break;
      case CONVERT:     result = asm_convert(instr, parm, 6);
                        break;
      case SQRT:        result = asm_sqrt(instr, parm, 3);
                        break;
      case CLASS:       result = asm_class(instr, parm, 3);
                        break;
      case ILLEGAL_E7:  result = EMSYNTAX;
                        break;
      case ILLEGAL_E8:  result = EMSYNTAX;
                        break;
      case ILLEGAL_E9:  result = EMSYNTAX;
                        break;
      case FEQ:         result = asm_float(instr, parm, 3);
                        break;
      case DEQ:         result = asm_float(instr, parm, 3);
                        break;
      case FGT:         result = asm_float(instr, parm, 3);
                        break;
      case DGT:         result = asm_float(instr, parm, 3);
                        break;
      case FGE:         result = asm_float(instr, parm, 3);
                        break;
      case DGE:         result = asm_float(instr, parm, 3);
                        break;

      /* Opcodes 0xF0 to 0xFF */
      case FADD:        result = asm_float(instr, parm, 3);
                        break;
      case DADD:        result = asm_float(instr, parm, 3);
                        break;
      case FSUB:        result = asm_float(instr, parm, 3);
                        break;
      case DSUB:        result = asm_float(instr, parm, 3);
                        break;
      case FMUL:        result = asm_float(instr, parm, 3);
                        break;
      case DMUL:        result = asm_float(instr, parm, 3);
                        break;
      case FDIV:        result = asm_float(instr, parm, 3);
                        break;
      case DDIV:        result = asm_float(instr, parm, 3);
                        break;
      case ILLEGAL_F8:  result = EMSYNTAX;
                        break;
      case FDMUL:       result = asm_float(instr, parm, 3);
                        break;
      case ILLEGAL_FA:  result = EMSYNTAX;
                        break;
      case ILLEGAL_FB:  result = EMSYNTAX;
                        break;
      case ILLEGAL_FC:  result = EMSYNTAX;
                        break;
      case ILLEGAL_FD:  result = EMSYNTAX;
                        break;
      case ILLEGAL_FE:  result = EMSYNTAX;
                        break;
      case ILLEGAL_FF:  result = EMSYNTAX;
                        break;
      }  /* end switch */

   return (result);

   }  /* End asm_instr() */




/*
** The following functions are used to convert instruction
** parameters as an arrays of addr_29k_t memory space / address
** pairs into a 32 bit Am29000 binary instruction.
** All of the Am29000 instruction formats are supported below.
*/


/*
** Formats:   <nmemonic>, RC, RA, (RB or I)
** Examples:  ADD, OR, SLL, all arithmetic and
**            logic instructions
**
*/

int
asm_arith_logic(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 3)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISGENERAL(parm[2].memory_space)) {
      /* Make sure M flag is cleared */
      instr->op = (BYTE) (instr->op & 0xfe);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0xff);
      }
   else
   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISMEM(parm[2].memory_space)) {
      /* Make sure M flag is set */
      instr->op = (BYTE) (instr->op | 0x01);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_arith_logic() */



/*
** Formats:   <nmemonic>, VN, RA, (RB or I)
** Examples:  ASSEQ, ASLE, ASLT, all trap assertion
**            instructions
**
*/

int
asm_vector(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 3)
      return (EMSYNTAX);

   if (ISMEM(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISGENERAL(parm[2].memory_space)) {
      /* Make sure M flag is cleared */
      instr->op = (BYTE) (instr->op & 0xfe);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0xff);
      }
   else
   if (ISMEM(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISMEM(parm[2].memory_space)) {
      /* Make sure M flag is set */
      instr->op = (BYTE) (instr->op | 0x01);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_vector() */



/*
** Formats:   <nmemonic>, CE, CNTL, RA, (RB or I)
** Examples:  LOAD, LOADM, STORE, all load and store
**            instructions
**
*/

int
asm_load_store(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   int  ce;
   int  cntl;

   if (parm_count != 4)
      return (EMSYNTAX);

   if (ISMEM(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space) &&
       ISGENERAL(parm[2].memory_space) &&
       ISGENERAL(parm[3].memory_space)) {
      /* Make sure M flag is cleared */
      instr->op = (BYTE) (instr->op & 0xfe);
      if (parm[0].address > 1)
         return (EMSYNTAX);
      if (parm[1].address > 0x7f)
         return (EMSYNTAX);
      ce =   (int) ((parm[0].address << 7) & 0x80);
      cntl = (int)  (parm[1].address & 0x7f);
      instr->c = (BYTE) (ce | cntl);
      instr->a = (BYTE) (parm[2].address & 0xff);
      instr->b = (BYTE) (parm[3].address & 0xff);
      }
   else
   if (ISMEM(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space) &&
       ISGENERAL(parm[2].memory_space) &&
       ISMEM(parm[3].memory_space)) {
      /* Make sure M flag is set */
      instr->op = (BYTE) (instr->op | 0x01);
      if (parm[0].address > 1)
         return (EMSYNTAX);
      if (parm[1].address > 0x7f)
         return (EMSYNTAX);
      if (parm[3].address > 0xff)
         return (EMSYNTAX);
      ce =   (int) ((parm[0].address << 7) & 0x80);
      cntl = (int)  (parm[1].address & 0x7f);
      instr->c = (BYTE) (ce | cntl);
      instr->a = (BYTE) (parm[2].address & 0xff);
      instr->b = (BYTE) (parm[3].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_load_store() */



/*
** Formats:   <nmemonic>
** Examples:  HALT, INV, IRET
*/
/*ARGSUSED*/

int
asm_no_parms(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 0)
      return (EMSYNTAX);

   /* Put zeros in the "reserved" fields */
   instr->c = 0;
   instr->a = 0;
   instr->b = 0;

   return (0);
   }  /* end asm_no_parms() */


int
asm_one_parms(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 1)
      return (EMSYNTAX);

   instr->c = (BYTE) (parm[0].address & 0x3);

   /* Put zeros in the "reserved" fields */
   instr->a = 0;
   instr->b = 0;

   return (0);
   } /* end asm_one_parms */


/*
** Formats:   <nmemonic>, RC, RA, RB
** Examples:  DADD, FADD, all floating point
**            instructions
**
*/

int
asm_float(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 3)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISGENERAL(parm[2].memory_space)) {
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_float() */



/*
** Formats:   <nmemonic> RA, <target>
** Examples:  CALL, JMPF, JMPFDEC, JMPT
**
** Note:  This function is used only with the CALL,
**        JMPF, JMPFDEC and JMPT operations.
*/

int
asm_call_jmp(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space)) {
      /* Make sure M flag is set */
      if (parm[1].memory_space != PC_RELATIVE)
         instr->op = (BYTE) (instr->op | 0x01);
      else
         instr->op = (BYTE) instr->op ;
      instr->c = (BYTE) ((parm[1].address >> 10) & 0xff);
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) ((parm[1].address >> 2) & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_call_jmp() */




/*
** Formats:   <nmemonic> RA, RB
** Examples:  CALLI, JMPFI, JMPTI
**
** Note:  This function is used only with the CALLI,
**        JMPFI and JMPTI (but not JMPI) operations.
*/

int
asm_calli_jmpi(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISREG(parm[1].memory_space)) {
      instr->c = 0;
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_calli_jmpi() */



/*
** Formats:   <nmemonic> RC, RB, FS
** Examples:  CLASS
**
** Note:  This function is used only with the CLASS
**        operation.
*/

int
asm_class(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 3)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISMEM(parm[2].memory_space)) {
      if (parm[2].address > 0x03)
         return (EMSYNTAX);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0x03);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_class() */


/*
** Formats:   <nmemonic> RC, (RB or I)
** Examples:  CLZ
**
** Note:  This function is used only with the CLZ
**        operation.
*/

int
asm_clz(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space)) {
      /* Make sure M flag is cleared */
      instr->op = (BYTE) (instr->op & 0xfe);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = 0;
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
   if (ISGENERAL(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space)) {
      /* Check param1 */
      if ((parm[1].address) > 0xff)
         return(EMSYNTAX);
      /* Make sure M flag is set */
      instr->op = (BYTE) (instr->op | 0x01);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = 0;
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_clz() */



/*
** Formats:   <nmemonic> RA, <const16>
** Examples:  CONST, CONSTN
**
*/

int
asm_const(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space)) {
      instr->c = (BYTE) ((parm[1].address >> 8) & 0xff);
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_const() */



/*
** Formats:   <nmemonic> RA, <const16>
** Examples:  CONSTH
**
*/

int
asm_consth(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space)) {
      instr->c = (BYTE) ((parm[1].address >> 24) & 0xff);
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) ((parm[1].address >> 16) & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_consth() */



/*
** Formats:   <nmemonic> RC, RA, UI, RND, FD, FS
** Examples:  CONVERT
**
** Note:  This function is used only with the CONVERT
**        operation.
**
** Note:  Some assembler examples show this operation with
**        only five parameters.  It should have six.
*/

int
asm_convert(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   BYTE  ui;
   BYTE  rnd;
   BYTE  fd;
   BYTE  fs;

   if (parm_count != 6)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISMEM(parm[2].memory_space) &&
       ISMEM(parm[3].memory_space) &&
       ISMEM(parm[4].memory_space) &&
       ISMEM(parm[5].memory_space)) {
      if (parm[2].address > 1)
         return (EMSYNTAX);
      if (parm[3].address > 0x07)
         return (EMSYNTAX);
      if (parm[4].address > 0x03)
         return (EMSYNTAX);
      if (parm[5].address > 0x03)
         return (EMSYNTAX);
      ui =  (BYTE) ((parm[2].address << 7) & 0x80);
      rnd = (BYTE) ((parm[3].address << 4) & 0x70);
      fd =  (BYTE) ((parm[4].address << 2) & 0x0c);
      fs =  (BYTE) (parm[5].address & 0x03);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (ui | rnd | fd | fs);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_convert() */



/*
** Formats:   <nmemonic> RC, RA
** Examples:  DIV0
**
** Note:  This function is used only with the DIV0
**        operation.
*/

int
asm_div0(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space)) {
      /* Make sure M flag is cleared */
      instr->op = (BYTE) (instr->op & 0xfe);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = 0;
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
   if (ISGENERAL(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space)) {
      /* Check immediate value */
      if (parm[1].address > 0xff)
         return (EMSYNTAX);
      /* Make sure M flag is set */
      instr->op = (BYTE) (instr->op | 0x01);
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = 0;
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_div0() */


/*
** Formats:   <nmemonic> RC, RA
** Examples:  EXHWS
**
** Note:  This function is used only with the EXHWS
**        operation.
*/

int
asm_exhws(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space)){
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = 0;
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_exhws() */




/*
** Formats:   <nmemonic>  <target>
** Examples:  JMP
**
** Note:  This function is used only with the JMP
**        operation.
**
** Note:  This function will only do absolute jumps.
*/

int
asm_jmp(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 1)
      return (EMSYNTAX);

   if (ISMEM(parm[0].memory_space)) {
      /* Make sure M flag is set */
      if (parm[0].memory_space != PC_RELATIVE)
        instr->op = (BYTE) (instr->op | 0x01);
      else
        instr->op = (BYTE) instr->op ;
      instr->c = (BYTE) ((parm[0].address >> 10) & 0xff);
      instr->a = 0;
      instr->b = (BYTE) ((parm[0].address >> 2) & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_jmp() */



/*
** Formats:   <nmemonic> RB
** Examples:  JMPI
**
** Note:  This function is used only with the JMPI
**        operation.
*/

int
asm_jmpi(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 1)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space)) {
      instr->c = 0;
      instr->a = 0;
      instr->b = (BYTE) (parm[0].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_jmpi() */



/*
** Formats:   <nmemonic> RC, SA
** Examples:  MFSR
**
** Note:  This function is used only with the MFSR
**        operation.
*/

int
asm_mfsr(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISSPECIAL(parm[1].memory_space)) {
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = 0;
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_mfsr() */



/*
** Formats:   <nmemonic> SA, RB
** Examples:  MTSR
**
** Note:  This function is used only with the MTSR
**        operation.
*/

int
asm_mtsr(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISSPECIAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space)) {
      instr->c = 0;
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_mtsr() */




/*
** Formats:   <nmemonic> SA, <const16>
** Examples:  MTSRIM
**
** Note:  This function is used only with the MTSRIM
**        operation.
*/

int
asm_mtsrim(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISSPECIAL(parm[0].memory_space) &&
       ISMEM(parm[1].memory_space)) {
      instr->c = (BYTE) ((parm[1].address >> 8) & 0xff);
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_mtsrim() */




/*
** Formats:   <nmemonic> RC, RA
** Examples:  MFTLB
**
** Note:  This function is used only with the MFTLB
**        operation.
*/

int
asm_mftlb(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space)) {
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = 0;
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_mftlb() */


/*
** Formats:   <nmemonic> RA, RB
** Examples:  MTTLB
**
** Note:  This function is used only with the MTTLB
**        operation.
*/

int
asm_mttlb(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 2)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space)) {
      instr->c = 0;
      instr->a = (BYTE) (parm[0].address & 0xff);
      instr->b = (BYTE) (parm[1].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_mttlb() */




/*
** Formats:   <nmemonic> RC, RA, FS
** Examples:  SQRT
**
** Note:  This function is used only with the SQRT
**        operation.
*/

int
asm_sqrt(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 3)
      return (EMSYNTAX);

   if (ISGENERAL(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISMEM(parm[2].memory_space)) {
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0x03);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_sqrt() */



/*
** Formats:   <nmemonic>, VN, RA, RB
** Examples:  EMULATE
**
** Note:  This function is used only with the EMULATE
**        operation.
**
*/

int
asm_emulate(instr, parm, parm_count)
   struct   instr_t    *instr;
   struct   addr_29k_t *parm;
   int      parm_count;
   {
   if (parm_count != 3)
      return (EMSYNTAX);

   if (ISMEM(parm[0].memory_space) &&
       ISGENERAL(parm[1].memory_space) &&
       ISGENERAL(parm[2].memory_space)) {
      instr->c = (BYTE) (parm[0].address & 0xff);
      instr->a = (BYTE) (parm[1].address & 0xff);
      instr->b = (BYTE) (parm[2].address & 0xff);
      }
   else
      return(EMSYNTAX);

   return (0);
   }  /* end asm_emulate() */





