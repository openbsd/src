static char _[] = " @(#)dasm.c	5.22 93/08/11 11:43:53, Srini, AMD";
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
 ** 
 **       This code is used to disasseble Am29000 instructions.  Each
 **       instruction is treated as four sequential bytes representing
 **       the instruction in big endian format.  This should permit the
 **       code to run on both big and little endian machines, provided
 **       that the buffer containing the instructions is in big endian
 **       format.
 **
 *****************************************************************************
 */

#include <stdio.h>
#include <string.h>
#ifdef	MSDOS
#include <stdlib.h>
#endif
#include "opcodes.h"
#include "memspcs.h"
#include "main.h"
#include "monitor.h"
#include "miniint.h"
#include "error.h"


/*
** There are approximately 23 different instruction formats for the
** Am29000.  Instructions are disassembled using one of these formats.
** It should be noted that more compact code for doing diassembly
** could have been produced.  It was decided that this approach was
** the easiest to understand and modify and that the structure would
** lend itself to the possibility of automatic code generation in
** future disassemblers (a disassembler-compiler?).
**
** Also note that these functions will correctly disassemble
** instructions in a buffer in big endian format.  Since the data is
** read in from the target as a stream of bytes, no conversion should
** be necessary.  This disassembly code should work on either big or
** little endian hosts.
**
** Note:  Opcodes in the "switch" statement are sorted in numerical
**        order.
**
** Note2: CLASS, CONVERT, SQRT may require a different format.
**
*/


int  get_addr_29k_m PARAMS((char *, struct addr_29k_t *, INT32));
int  addr_29k_ok PARAMS((struct addr_29k_t *));
void convert32 PARAMS((BYTE *));
INT32 match_entry PARAMS((ADDR32 offset,
			  INT32 space,
			  int *id,
			  struct bkpt_t **table));

void dasm_instr PARAMS((ADDR32, struct instr_t *));

void dasm_undefined PARAMS((struct instr_t *, ADDR32));
void dasm_ra_const16n PARAMS((struct instr_t *, ADDR32));
void dasm_ra_const16h PARAMS((struct instr_t *, ADDR32));
void dasm_ra_const16 PARAMS((struct instr_t *, ADDR32));
void dasm_spid_const16 PARAMS((struct instr_t *, ADDR32));
void dasm_ce_cntl_ra_rb PARAMS((struct instr_t *, ADDR32));
void dasm_ce_cntl_ra_const8 PARAMS((struct instr_t *, ADDR32));
void dasm_rc_rb PARAMS((struct instr_t *, ADDR32));
void dasm_rc_const8 PARAMS((struct instr_t *, ADDR32));
void dasm_rc_ra_rb PARAMS((struct instr_t *, ADDR32));
void dasm_rc_ra_const8 PARAMS((struct instr_t *, ADDR32));
void dasm_vn_ra_rb PARAMS((struct instr_t *, ADDR32));
void dasm_vn_ra_const8 PARAMS((struct instr_t *, ADDR32));
void dasm_rc_ra PARAMS((struct instr_t *, ADDR32));
void dasm_none PARAMS((struct instr_t *, ADDR32));
void dasm_one PARAMS((struct instr_t *, ADDR32));
void dasm_atarget PARAMS((struct instr_t *, ADDR32));
void dasm_rtarget PARAMS((struct instr_t *, ADDR32));
void dasm_ra_rtarget PARAMS((struct instr_t *, ADDR32));
void dasm_ra_atarget PARAMS((struct instr_t *, ADDR32));
void dasm_ra_rb PARAMS((struct instr_t *, ADDR32));
void dasm_rb PARAMS((struct instr_t *, ADDR32));
void dasm_rc_spid PARAMS((struct instr_t *, ADDR32));
void dasm_spid_rb PARAMS((struct instr_t *, ADDR32));
void dasm_dc_ra_rb PARAMS((struct instr_t *, ADDR32));
void dasm_convert PARAMS((struct instr_t *, ADDR32));



INT32
dasm_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   static INT32  memory_space=I_MEM;
   static ADDR32 address=0;
   INT32  byte_count=(16*sizeof(INST32));
   int    i;
   int    result;
   INT32    bytes_returned;
   int    bkpt_index;
   struct instr_t instr;
   struct addr_29k_t addr_29k;
   struct addr_29k_t addr_29k_start;
   struct addr_29k_t addr_29k_end;

   INT32	retval;
   INT32	hostendian;
   BYTE		*read_buffer;


   /* Is it an 'l' (disassemble) command? */
   if (strcmp(token[0], "l") != 0)
      return (EMSYNTAX);

   /*
   ** Parse parameters
   */

   if (token_count == 1) {
      address = (address & 0xfffffffc) + byte_count;
      }
   else
   if (token_count == 2) {
      result = get_addr_29k_m(token[1], &addr_29k_start, I_MEM);
      if (result != 0)
         return (EMSYNTAX);
      result = addr_29k_ok(&addr_29k_start);
      if (result != 0)
         return (result);
      address = (addr_29k_start.address & 0xfffffffc);
      memory_space = addr_29k_start.memory_space;
      }
   else
   if (token_count == 3) {
      result = get_addr_29k_m(token[1], &addr_29k_start, I_MEM);
      if (result != 0)
         return (EMSYNTAX);
      result = addr_29k_ok(&addr_29k_start);
      if (result != 0)
         return (result);
      result = get_addr_29k_m(token[2], &addr_29k_end, I_MEM);
      if (result != 0)
         return (EMSYNTAX);
      result = addr_29k_ok(&addr_29k_end);
      if (result != 0)
         return (result);
      if (addr_29k_start.memory_space != addr_29k_end.memory_space)
         return (EMBADADDR);
      if (addr_29k_start.address > addr_29k_end.address)
         return (EMBADADDR);
      address = (addr_29k_start.address & 0xfffffffc);
      memory_space = addr_29k_start.memory_space;
      byte_count = (addr_29k_end.address & 0xfffffffc) -
                   (addr_29k_start.address & 0xfffffffc) +
                   sizeof(INT32);
      }
   else
   /* Too many args */
      return (EMSYNTAX);

   /* Will the data overflow the message buffer? Done in TIP */

   if ((read_buffer = (BYTE *) malloc((unsigned int) byte_count)) == NULL) {
      warning(EMALLOC);
      return(FAILURE);
   };

   hostendian = FALSE;
   if ((retval = Mini_read_req(memory_space,
			       address,
			       (byte_count/4),
			       (INT16) 4,  /* I_MEM/I_ROM always size 4 */
			       &bytes_returned,
			       read_buffer,
			       hostendian)) != SUCCESS) {
	return(FAILURE);
   } else if (retval == SUCCESS) {
       for (i=0; i<(int)(bytes_returned*4); i=i+sizeof(INST32)) {

          addr_29k.memory_space = memory_space;
          addr_29k.address = address + (ADDR32) i;
    
	  if (host_config.target_endian == LITTLE) {
             instr.op = read_buffer[i+3];
             instr.c = read_buffer[i+2];
             instr.a = read_buffer[i+1];
             instr.b = read_buffer[i];
	  } else { /* BIG endian assumed */
             instr.op = read_buffer[i];
             instr.c = read_buffer[i+1];
             instr.a = read_buffer[i+2];
             instr.b = read_buffer[i+3];
	  }
    
	  if (io_config.echo_mode == (INT32) TRUE) 
             fprintf(io_config.echo_file, "%08lx    ", addr_29k.address);
          fprintf(stderr, "%08lx    ", addr_29k.address);
    
          /* Is there an breakpoint at this location? */
          match_entry(addr_29k.address, addr_29k.memory_space, &bkpt_index, &bkpt_table);
	  if (io_config.echo_mode == (INT32) TRUE)
            if (bkpt_index <= (int) 0)
             fprintf(io_config.echo_file, "%02x%02x%02x%02x    ", instr.op, instr.c,
                    instr.a, instr.b);
             else
                fprintf(io_config.echo_file, "%02x%02x%02x%02x   *", instr.op, instr.c,
                       instr.a, instr.b);

          if (bkpt_index <= (int) 0)
             fprintf(stderr, "%02x%02x%02x%02x    ", instr.op, instr.c,
                    instr.a, instr.b);
             else
                fprintf(stderr, "%02x%02x%02x%02x   *", instr.op, instr.c,
                       instr.a, instr.b);
    
          dasm_instr((address + (ADDR32) i),
                     &instr);
	  if (io_config.echo_mode == (INT32) TRUE) 
             fprintf(io_config.echo_file, "\n");
          fprintf(stderr, "\n");
       }  /* end for loop */
   };

   (void) free ((char *) read_buffer);
   return (0);
   }



/*
** This function is used to disassemble a singe Am29000 instruction.
** A pointer to the next free space in the buffer is returned.
*/


void
dasm_instr(addr, instr)
   ADDR32   addr;
   struct   instr_t *instr;
   {

   switch (instr->op) {   

      /* Opcodes 0x00 to 0x0F */
      case ILLEGAL_00:  dasm_undefined(instr, addr);
                        break;
      case CONSTN:      dasm_ra_const16n(instr, addr);
                        break;
      case CONSTH:      dasm_ra_const16h(instr, addr);
                        break;
      case CONST:       dasm_ra_const16(instr, addr);
                        break;
      case MTSRIM:      dasm_spid_const16(instr, addr);
                        break;
      case CONSTHZ:     dasm_ra_const16(instr, addr);
                        break;
      case LOADL0:      dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case LOADL1:      dasm_ce_cntl_ra_const8(instr, addr);
                        break;
      case CLZ0:        dasm_rc_rb(instr, addr);
                        break;
      case CLZ1:        dasm_rc_const8(instr, addr);
                        break;
      case EXBYTE0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case EXBYTE1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case INBYTE0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case INBYTE1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case STOREL0:     dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case STOREL1:     dasm_ce_cntl_ra_const8(instr, addr);
                        break;

      /* Opcodes 0x10 to 0x1F */
      case ADDS0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case ADDS1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case ADDU0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case ADDU1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case ADD0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case ADD1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case LOAD0:       dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case LOAD1:       dasm_ce_cntl_ra_const8(instr, addr);
                        break;
      case ADDCS0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case ADDCS1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case ADDCU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case ADDCU1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case ADDC0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case ADDC1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case STORE0:      dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case STORE1:      dasm_ce_cntl_ra_const8(instr, addr);
                        break;

      /* Opcodes 0x20 to 0x2F */
      case SUBS0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBS1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBU0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBU1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case SUB0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case SUB1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case LOADSET0:    dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case LOADSET1:    dasm_ce_cntl_ra_const8(instr, addr);
                        break;
      case SUBCS0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBCS1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBCU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBCU1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBC0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBC1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case CPBYTE0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case CPBYTE1:     dasm_rc_ra_const8(instr, addr);
                        break;


      /* Opcodes 0x30 to 0x3F */
      case SUBRS0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBRS1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBRU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBRU1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBR0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBR1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case LOADM0:      dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case LOADM1:      dasm_ce_cntl_ra_const8(instr, addr);
                        break;
      case SUBRCS0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBRCS1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBRCU0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBRCU1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case SUBRC0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case SUBRC1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case STOREM0:     dasm_ce_cntl_ra_rb(instr, addr);
                        break;
      case STOREM1:     dasm_ce_cntl_ra_const8(instr, addr);
                        break;

      /* Opcodes 0x40 to 0x4F */
      case CPLT0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case CPLT1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case CPLTU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case CPLTU1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case CPLE0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case CPLE1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case CPLEU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case CPLEU1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case CPGT0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case CPGT1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case CPGTU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case CPGTU1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case CPGE0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case CPGE1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case CPGEU0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case CPGEU1:      dasm_rc_ra_const8(instr, addr);
                        break;

      /* Opcodes 0x50 to 0x5F */
      case ASLT0:       dasm_vn_ra_rb(instr, addr);
                        break;
      case ASLT1:       dasm_vn_ra_const8(instr, addr);
                        break;
      case ASLTU0:      dasm_vn_ra_rb(instr, addr);
                        break;
      case ASLTU1:      dasm_vn_ra_const8(instr, addr);
                        break;
      case ASLE0:       dasm_vn_ra_rb(instr, addr);
                        break;
      case ASLE1:       dasm_vn_ra_const8(instr, addr);
                        break;
      case ASLEU0:      dasm_vn_ra_rb(instr, addr);
                        break;
      case ASLEU1:      dasm_vn_ra_const8(instr, addr);
                        break;
      case ASGT0:       dasm_vn_ra_rb(instr, addr);
                        break;
      case ASGT1:       dasm_vn_ra_const8(instr, addr);
                        break;
      case ASGTU0:      dasm_vn_ra_rb(instr, addr);
                        break;
      case ASGTU1:      dasm_vn_ra_const8(instr, addr);
                        break;
      case ASGE0:       dasm_vn_ra_rb(instr, addr);
                        break;
      case ASGE1:       dasm_vn_ra_const8(instr, addr);
                        break;
      case ASGEU0:      dasm_vn_ra_rb(instr, addr);
                        break;
      case ASGEU1:      dasm_vn_ra_const8(instr, addr);
                        break;

      /* Opcodes 0x60 to 0x6F */
      case CPEQ0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case CPEQ1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case CPNEQ0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case CPNEQ1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case MUL0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case MUL1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case MULL0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case MULL1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case DIV0_OP0:    dasm_rc_rb(instr, addr);
                        break;
      case DIV0_OP1:    dasm_rc_const8(instr, addr);
                        break;
      case DIV_OP0:    dasm_rc_ra_rb(instr, addr);
                        break;
      case DIV_OP1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case DIVL0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case DIVL1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case DIVREM0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case DIVREM1:     dasm_rc_ra_const8(instr, addr);
                        break;

      /* Opcodes 0x70 to 0x7F */
      case ASEQ0:       dasm_vn_ra_rb(instr, addr);
                        break;
      case ASEQ1:       dasm_vn_ra_const8(instr, addr);
                        break;
      case ASNEQ0:      dasm_vn_ra_rb(instr, addr);
                        break;
      case ASNEQ1:      dasm_vn_ra_const8(instr, addr);
                        break;
      case MULU0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case MULU1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case ILLEGAL_76:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_77:  dasm_undefined(instr, addr);
                        break;
      case INHW0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case INHW1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case EXTRACT0:    dasm_rc_ra_rb(instr, addr);
                        break;
      case EXTRACT1:    dasm_rc_ra_const8(instr, addr);
                        break;
      case EXHW0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case EXHW1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case EXHWS:       dasm_rc_ra(instr, addr);
                        break;
      case ILLEGAL_7F:  dasm_undefined(instr, addr);
                        break;

      /* Opcodes 0x80 to 0x8F */
      case SLL0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case SLL1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case SRL0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case SRL1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case ILLEGAL_84:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_85:  dasm_undefined(instr, addr);
                        break;
      case SRA0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case SRA1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case IRET:        dasm_none(instr, addr);
                        break;
      case HALT_OP:     dasm_none(instr, addr);
                        break;
      case ILLEGAL_8A:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_8B:  dasm_undefined(instr, addr);
                        break;
      case IRETINV:     
			if ((target_config.processor_id & 0x60) == 0x60)
			   dasm_one(instr, addr);
			else
			   dasm_none(instr, addr);
                        break;
      case ILLEGAL_8D:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_8E:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_8F:  dasm_undefined(instr, addr);
                        break;

      /* Opcodes 0x90 to 0x9F */
      case AND_OP0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case AND_OP1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case OR_OP0:      dasm_rc_ra_rb(instr, addr);
                        break;
      case OR_OP1:      dasm_rc_ra_const8(instr, addr);
                        break;
      case XOR_OP0:     dasm_rc_ra_rb(instr, addr);
                        break;
      case XOR_OP1:     dasm_rc_ra_const8(instr, addr);
                        break;
      case XNOR0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case XNOR1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case NOR0:        dasm_rc_ra_rb(instr, addr);
                        break;
      case NOR1:        dasm_rc_ra_const8(instr, addr);
                        break;
      case NAND0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case NAND1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case ANDN0:       dasm_rc_ra_rb(instr, addr);
                        break;
      case ANDN1:       dasm_rc_ra_const8(instr, addr);
                        break;
      case SETIP:       dasm_rc_ra_rb(instr, addr);
                        break;
      case INV:         
			if ((target_config.processor_id & 0x60) == 0x60)
			   dasm_one(instr, addr);
			else
			   dasm_none(instr, addr);
                        break;

      /* Opcodes 0xA0 to 0xAF */
      case JMP0:        dasm_rtarget(instr, addr);
                        break;
      case JMP1:        dasm_atarget(instr, addr);
                        break;
      case ILLEGAL_A2:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_A3:  dasm_undefined(instr, addr);
                        break;
      case JMPF0:       dasm_ra_rtarget(instr, addr);
                        break;
      case JMPF1:       dasm_ra_atarget(instr, addr);
                        break;
      case ILLEGAL_A6:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_A7:  dasm_undefined(instr, addr);
                        break;
      case CALL0:       dasm_ra_rtarget(instr, addr);
                        break;
      case CALL1:       dasm_ra_atarget(instr, addr);
                        break;
      case ORN_OP0:  	dasm_undefined(instr, addr);
                        break;
      case ORN_OP1:  	dasm_undefined(instr, addr);
                        break;
      case JMPT0:       dasm_ra_rtarget(instr, addr);
                        break;
      case JMPT1:       dasm_ra_atarget(instr, addr);
                        break;
      case ILLEGAL_AE:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_AF:  dasm_undefined(instr, addr);
                        break;

      /* Opcodes 0xB0 to 0xBF */
      case ILLEGAL_B0:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_B1:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_B2:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_B3:  dasm_undefined(instr, addr);
                        break;
      case JMPFDEC0:    dasm_ra_rtarget(instr, addr);
                        break;
      case JMPFDEC1:    dasm_ra_atarget(instr, addr);
                        break;
      case MFTLB:       dasm_rc_ra(instr, addr);
                        break;
      case ILLEGAL_B7:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_B8:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_B9:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_BA:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_BB:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_BC:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_BD:  dasm_undefined(instr, addr);
                        break;
      case MTTLB:       dasm_ra_rb(instr, addr);
                        break;
      case ILLEGAL_BF:  dasm_undefined(instr, addr);
                        break;

      /* Opcodes 0xC0 to 0xCF */
      case JMPI:        dasm_rb(instr, addr);
                        break;
      case ILLEGAL_C1:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_C2:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_C3:  dasm_undefined(instr, addr);
                        break;
      case JMPFI:       dasm_ra_rb(instr, addr);
                        break;
      case ILLEGAL_C5:  dasm_undefined(instr, addr);
                        break;
      case MFSR:        dasm_rc_spid(instr, addr);
                        break;
      case ILLEGAL_C7:  dasm_undefined(instr, addr);
                        break;
      case CALLI:       dasm_ra_rb(instr, addr);
                        break;
      case ILLEGAL_C9:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_CA:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_CB:  dasm_undefined(instr, addr);
                        break;
      case JMPTI:       dasm_ra_rb(instr, addr);
                        break;
      case ILLEGAL_CD:  dasm_undefined(instr, addr);
                        break;
      case MTSR:        dasm_spid_rb(instr, addr);
                        break;
      case ILLEGAL_CF:  dasm_undefined(instr, addr);
                        break;

      /* Opcodes 0xD0 to 0xDF */
      case ILLEGAL_D0:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D1:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D2:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D3:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D4:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D5:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D6:  dasm_undefined(instr, addr);
                        break;
      case EMULATE:     dasm_vn_ra_rb(instr, addr);
                        break;
      case ILLEGAL_D8:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_D9:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_DA:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_DB:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_DC:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_DD:  dasm_undefined(instr, addr);
                        break;
      case MULTM:       dasm_rc_ra_rb(instr, addr);
                        break;
      case MULTMU:      dasm_rc_ra_rb(instr, addr);
                        break;

      /* Opcodes 0xE0 to 0xEF */
      case MULTIPLY:    dasm_rc_ra_rb(instr, addr);
                        break;
      case DIVIDE:      dasm_rc_ra_rb(instr, addr);
                        break;
      case MULTIPLU:    dasm_rc_ra_rb(instr, addr);
                        break;
      case DIVIDU:      dasm_rc_ra_rb(instr, addr);
                        break;
      case CONVERT:     dasm_convert(instr, addr);
                        break;
      case SQRT:        dasm_rc_ra_const8(instr, addr);
                        break;
      case CLASS:       dasm_dc_ra_rb(instr, addr);
                        break;
      case ILLEGAL_E7:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_E8:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_E9:  dasm_undefined(instr, addr);
                        break;
      case FEQ:         dasm_rc_ra_rb(instr, addr);
                        break;
      case DEQ:         dasm_rc_ra_rb(instr, addr);
                        break;
      case FGT:         dasm_rc_ra_rb(instr, addr);
                        break;
      case DGT:         dasm_rc_ra_rb(instr, addr);
                        break;
      case FGE:         dasm_rc_ra_rb(instr, addr);
                        break;
      case DGE:         dasm_rc_ra_rb(instr, addr);
                        break;

      /* Opcodes 0xF0 to 0xFF */
      case FADD:        dasm_rc_ra_rb(instr, addr);
                        break;
      case DADD:        dasm_rc_ra_rb(instr, addr);
                        break;
      case FSUB:        dasm_rc_ra_rb(instr, addr);
                        break;
      case DSUB:        dasm_rc_ra_rb(instr, addr);
                        break;
      case FMUL:        dasm_rc_ra_rb(instr, addr);
                        break;
      case DMUL:        dasm_rc_ra_rb(instr, addr);
                        break;
      case FDIV:        dasm_rc_ra_rb(instr, addr);
                        break;
      case DDIV:        dasm_rc_ra_rb(instr, addr);
                        break;
      case ILLEGAL_F8:  dasm_undefined(instr, addr);
                        break;
      case FDMUL:       dasm_rc_ra_rb(instr, addr);
                        break;
      case ILLEGAL_FA:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_FB:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_FC:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_FD:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_FE:  dasm_undefined(instr, addr);
                        break;
      case ILLEGAL_FF:  dasm_undefined(instr, addr);
                        break;
      }  /* end switch */

   }  /* End dasm_instr() */




/*
** The following functions are used to format an instruction
** into human-readable form.  All of the Am29000 instruction
** formats are supported below.
*/


/*
** Format:  0xnnnnnnnn
*/
/*ARGSUSED*/

void
dasm_undefined(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
    fprintf(io_config.echo_file, ".word    0x%02x%02x%02x%02x", instr->op,
                 instr->c, instr->a, instr->b);
   (void) fprintf(stderr, ".word    0x%02x%02x%02x%02x", instr->op,
                 instr->c, instr->a, instr->b);
   }



/*
** Format:  <Mnemonic>  ra, const16
**
** (See CONSTN)
*/
/*ARGSUSED*/

void
dasm_ra_const16n(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   const16 = (INT32) ((instr->b | (instr->c << 8)) | 0xffff0000);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], const16);
   (void) fprintf(stderr, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], const16);
   }



/*
** Format:  <Mnemonic>  ra, const16
**
** (See CONSTH)
*/
/*ARGSUSED*/

void
dasm_ra_const16h(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const32;
   INT32  i_15_8;
   INT32  i_7_0;

   i_15_8 = (INT32) instr->c;
   i_7_0  = (INT32) instr->b;
   const32 = ((i_15_8 << 24) | (i_7_0 << 16));
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], const32);
   (void) fprintf(stderr, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], const32);
   }



/*
** Format:  <Mnemonic>  ra, const16
**
** (See CONST)
*/
/*ARGSUSED*/

void
dasm_ra_const16(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   const16 = (INT32) (instr->b | (instr->c << 8));
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%x", opcode_name[instr->op],
                 reg[instr->a], const16);
   (void) fprintf(stderr, "%s %s,0x%x", opcode_name[instr->op],
                 reg[instr->a], const16);
   }



/*
** Format:  <Mnemonic>  spid, const16
**
** (See MTSRIM)
*/
/*ARGSUSED*/

void
dasm_spid_const16(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   INT32  i_15_8;
   INT32  i_7_0;

   i_15_8 = (INT32) instr->c;
   i_7_0  = (INT32) instr->b;

   const16 = ((i_15_8 << 8) | i_7_0);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%x", opcode_name[instr->op],
                 spreg[instr->a], const16);
   (void) fprintf(stderr, "%s %s,0x%x", opcode_name[instr->op],
                 spreg[instr->a], const16);
   }




/*
** Format:  <Mnemonic>  ce, cntl, ra, rb
**
** (See LOADM, LOADSET, STORE, STOREL, etc...)
*/
/*ARGSUSED*/

void
dasm_ce_cntl_ra_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   int ce;
   int cntl;

   ce = (int) ((instr->c >> 7) & 0x01);
   cntl = (int) (instr->c & 0x7f);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %x,0x%x,%s,%s",
                 opcode_name[instr->op], ce, cntl,
                 reg[instr->a], reg[instr->b]);
   (void) fprintf(stderr, "%s %x,0x%x,%s,%s",
                 opcode_name[instr->op], ce, cntl,
                 reg[instr->a], reg[instr->b]);
   }



/*
** Format:  <Mnemonic>  ce, cntl, ra, const8
**
** (See LOADM, LOADSET, STORE, STOREL, etc...)
*/
/*ARGSUSED*/

void
dasm_ce_cntl_ra_const8(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   int ce;
   int cntl;

   ce = (int) ((instr->c >> 7) & 0x01);
   cntl = (int) (instr->c & 0x7f);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %x,0x%x,%s,0x%x",
                 opcode_name[instr->op], ce, cntl,
                 reg[instr->a], instr->b);
   (void) fprintf(stderr, "%s %x,0x%x,%s,0x%x",
                 opcode_name[instr->op], ce, cntl,
                 reg[instr->a], instr->b);
   }



/*
** Format:  <Mnemonic>  rc, rb
**
** (See CLZ, DIV0)
*/
/*ARGSUSED*/

void
dasm_rc_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->b]);
   (void) fprintf(stderr, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->b]);
   }



/*
** Format:  <Mnemonic>  rc, const8
**
** (See CLZ, DIV0)
*/
/*ARGSUSED*/

void
dasm_rc_const8(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%x", opcode_name[instr->op],
                 reg[instr->c], instr->b);
   (void) fprintf(stderr, "%s %s,0x%x", opcode_name[instr->op],
                 reg[instr->c], instr->b);
   }



/*
** Format:  <Mnemonic>  rc, ra, rb
**
** (See ADD, AND, etc...)
*/
/*ARGSUSED*/

void
dasm_rc_ra_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a], reg[instr->b]);
   (void) fprintf(stderr, "%s %s,%s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a], reg[instr->b]);
   }



/*
** Format:  <Mnemonic>  rc, ra, const8
**
** (See ADD, AND, etc...)
*/
/*ARGSUSED*/

void
dasm_rc_ra_const8(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s,0x%x", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a], instr->b);
   (void) fprintf(stderr, "%s %s,%s,0x%x", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a], instr->b);
   }



/*
** Format:  <Mnemonic>  vn, ra, rb
**
** (See ASEQ, ASGE, etc...)
**
** Note:  This function also prints out a "nop" if the
**        instruction is an ASEQ and RA == RB.
**
*/
/*ARGSUSED*/

void
dasm_vn_ra_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if ((instr->op == ASEQ0) &&
       (instr->a == instr->b))  {
      if (io_config.echo_mode == (INT32) TRUE) 
      (void) fprintf(io_config.echo_file, "nop");
      (void) fprintf(stderr, "nop");
      } else {
	 if (io_config.echo_mode == (INT32) TRUE) 
         (void) fprintf(io_config.echo_file, "%s 0x%x,%s,%s", opcode_name[instr->op],
                       instr->c, reg[instr->a], reg[instr->b]);
         (void) fprintf(stderr, "%s 0x%x,%s,%s", opcode_name[instr->op],
                       instr->c, reg[instr->a], reg[instr->b]);
      }
   }


/*
** Format:  <Mnemonic>  vn, ra, const8
**
** (See ASEQ, ASGE, etc...)
*/
/*ARGSUSED*/

void
dasm_vn_ra_const8(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s 0x%x,%s,0x%x", opcode_name[instr->op],
                 instr->c, reg[instr->a], instr->b);
   (void) fprintf(stderr, "%s 0x%x,%s,0x%x", opcode_name[instr->op],
                 instr->c, reg[instr->a], instr->b);
   }



/*
** Format:  <Mnemonic>  rc, ra
**
** (See MFTBL)
*/
/*ARGSUSED*/

void
dasm_rc_ra(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a]);
   (void) fprintf(stderr, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a]);
   }



/*
** Format:  <Mnemonic>
**
** (See HALT, IRET)
*/
/*ARGSUSED*/

void
dasm_none(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   (void) fprintf(stderr, "%s", opcode_name[instr->op]);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s", opcode_name[instr->op]);
   }

void
dasm_one(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   (void) fprintf(stderr, "%s 0x%x", opcode_name[instr->op],(int) (instr->c & 0x3));
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s 0x%x", opcode_name[instr->op], (int) (instr->c & 0x3));
   }


/*
** Format:  <Mnemonic>  target
**
** (See JMP, etc...)
*/
/*ARGSUSED*/

void
dasm_atarget(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   INT32  i_17_10;
   INT32  i_9_2;

   i_17_10 = ((INT32) instr->c) << 10;
   i_9_2 = ((INT32) instr->b) << 2;
   const16 = (i_17_10 | i_9_2);
   (void) fprintf(stderr, "%s 0x%lx", opcode_name[instr->op],
                 const16);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s 0x%lx", opcode_name[instr->op],
                 const16);
   }



/*
** Format:  <Mnemonic>  target+pc
**
** (See JMP, etc...)
*/

void
dasm_rtarget(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   INT32  i_17_10;
   INT32  i_9_2;

   i_17_10 = ((INT32) instr->c) << 10;
   i_9_2 = ((INT32) instr->b) << 2;
   const16 = (i_17_10 | i_9_2);
   if ((const16 & 0x00020000) != 0)  /* Sign extend (bit 17) */
      const16 = (const16 | 0xfffc0000);
   (void) fprintf(stderr, "%s 0x%lx", opcode_name[instr->op],
                 (const16+pc));
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s 0x%lx", opcode_name[instr->op],
                 (const16+pc));
   }



/*
** Format:  <Mnemonic>  ra, target
**
** (See CALL, JMPFDEC, JMPT, etc...)
*/
/*ARGSUSED*/

void
dasm_ra_atarget(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   INT32  i_17_10;
   INT32  i_9_2;

   i_17_10 = ((INT32) instr->c) << 10;
   i_9_2 = ((INT32) instr->b) << 2;
   const16 = (i_17_10 | i_9_2);
   (void) fprintf(stderr, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], const16);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], const16);
   }



/*
** Format:  <Mnemonic>  ra, target
**
** (See CALL, JMPFDEC, JMPT, etc...)
*/

void
dasm_ra_rtarget(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   INT32  const16;
   INT32  i_17_10;
   INT32  i_9_2;

   i_17_10 = ((INT32) instr->c) << 10;
   i_9_2 = ((INT32) instr->b) << 2;
   const16 = (i_17_10 | i_9_2);
   if ((const16 & 0x00020000) != 0)  /* Sign extend (bit 17) */
      const16 = (const16 | 0xfffc0000);
   (void) fprintf(stderr, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], (const16+pc));
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,0x%lx", opcode_name[instr->op],
                 reg[instr->a], (const16+pc));
   }



/*
** Format:  <Mnemonic>  ra, rb
**
** (See CALLI, JMPFI)
*/
/*ARGSUSED*/

void
dasm_ra_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   (void) fprintf(stderr, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->a], reg[instr->b]);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->a], reg[instr->b]);
   }



/*
** Format:  <Mnemonic>  rb
**
** (See JMPI)
*/
/*ARGSUSED*/

void
dasm_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   (void) fprintf(stderr, "%s %s", opcode_name[instr->op],
                 reg[instr->b]);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s", opcode_name[instr->op],
                 reg[instr->b]);
   }



/*
** Format:  <Mnemonic>  rc, spid
**
** (See MFSR)
*/
/*ARGSUSED*/

void
dasm_rc_spid(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   (void) fprintf(stderr, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], spreg[instr->a]);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], spreg[instr->a]);
   }



/*
** Format:  <Mnemonic>  spid, rb
**
** (See MTSR)
*/
/*ARGSUSED*/

void
dasm_spid_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s", opcode_name[instr->op],
                 spreg[instr->a], reg[instr->b]);
   (void) fprintf(stderr, "%s %s,%s", opcode_name[instr->op],
                 spreg[instr->a], reg[instr->b]);
   }



/*
** Format:  <Mnemonic>  dc, ra, rb
**
** (See CLASS)
*/
/*ARGSUSED*/

void
dasm_dc_ra_rb(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   (void) fprintf(stderr, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a]);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s", opcode_name[instr->op],
                 reg[instr->c], reg[instr->a]);
   }



/*
** Format:  <Mnemonic>  rc, ra, UI, RND, FD, FS
**
** (See CONVERT)
*/
/*ARGSUSED*/

void
dasm_convert(instr, pc)
   struct   instr_t *instr;
   ADDR32   pc;
   {
   int ui;
   int rnd;
   int fd;
   int fs;

   ui = (int) ((instr->b >> 7) & 0x01);
   rnd = (int) ((instr->b >> 4) & 0x07);
   fd = (int) ((instr->b >> 2) & 0x03);
   fs = (int) (instr->b & 0x03);
   if (io_config.echo_mode == (INT32) TRUE) 
   (void) fprintf(io_config.echo_file, "%s %s,%s,%x,%x,%x,%x",
                 opcode_name[instr->op],
                 reg[instr->c], reg[instr->a],
                 ui, rnd, fd, fs);
   (void) fprintf(stderr, "%s %s,%s,%x,%x,%x,%x",
                 opcode_name[instr->op],
                 reg[instr->c], reg[instr->a],
                 ui, rnd, fd, fs);
   }



