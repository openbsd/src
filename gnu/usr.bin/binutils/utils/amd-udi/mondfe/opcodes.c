static char _[] = "@(#)opcodes.c	5.21 93/08/10 17:46:46,Srini, AMD.";
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
 **       This file contains a data structure which gives the ASCII
 **       string associated with each Am29000 opcode.  These strings
 **       are all in lower case.  The string associated with illegal
 **       opcodes are null ("").  Opcodes are sorted in numerical
 **       order.
 **
 *****************************************************************************
 */
char *opcode_name[] =
   {

   /* Opcodes 0x00 to 0x0F */
   "",            "constn  ",    "consth  ",    "const   ",
   "mtsrim  ",    "consthz ",    "loadl   ",    "loadl   ",
   "clz     ",    "clz     ",    "exbyte  ",    "exbyte  ",
   "inbyte  ",    "inbyte  ",    "storel  ",    "storel  ",

   /* Opcodes 0x10 to 0x1F */
   "adds    ",    "adds    ",    "addu    ",    "addu    ",
   "add     ",    "add     ",    "load    ",    "load    ",
   "addcs   ",    "addcs   ",    "addcu   ",    "addcu   ",
   "addc    ",    "addc    ",    "store   ",    "store   ",

   /* Opcodes 0x20 to 0x2F */
   "subs    ",    "subs    ",    "subu    ",    "subu    ",
   "sub     ",    "sub     ",    "loadset ",    "loadset ",
   "subcs   ",    "subcs   ",    "subcu   ",    "subcu   ",
   "subc    ",    "subc    ",    "cpbyte  ",    "cpbyte  ",

   /* Opcodes 0x30 to 0x3F */
   "subrs   ",    "subrs   ",    "subru   ",    "subru   ",
   "subr    ",    "subr    ",    "loadm   ",    "loadm   ",
   "subrcs  ",    "subrcs  ",    "subrcu  ",    "subrcu  ",
   "subrc   ",    "subrc   ",    "storem  ",    "storem  ",

   /* Opcodes 0x40 to 0x4F */
   "cplt    ",    "cplt    ",    "cpltu   ",    "cpltu   ",
   "cple    ",    "cple    ",    "cpleu   ",    "cpleu   ",
   "cpgt    ",    "cpgt    ",    "cpgtu   ",    "cpgtu   ",
   "cpge    ",    "cpge    ",    "cpgeu   ",    "cpgeu   ",

   /* Opcodes 0x50 to 0x5F */
   "aslt    ",    "aslt    ",    "asltu   ",    "asltu   ",
   "asle    ",    "asle    ",    "asleu   ",    "asleu   ",
   "asgt    ",    "asgt    ",    "asgtu   ",    "asgtu   ",
   "asge    ",    "asge    ",    "asgeu   ",    "asgeu   ",

   /* Opcodes 0x60 to 0x6F */
   "cpeq    ",    "cpeq    ",    "cpneq   ",    "cpneq   ",
   "mul     ",    "mul     ",    "mull    ",    "mull    ",
   "div0    ",    "div0    ",    "div     ",    "div     ",
   "divl    ",    "divl    ",    "divrem  ",    "divrem  ",

   /* Opcodes 0x70 to 0x7F */
   "aseq    ",    "aseq    ",    "asneq   ",    "asneq   ",
   "mulu    ",    "mulu    ",    "",            "",
   "inhw    ",    "inhw    ",    "extract ",    "extract ",
   "exhw    ",    "exhw    ",    "exhws   ",    "",

   /* Opcodes 0x80 to 0x8F */
   "sll     ",    "sll     ",    "srl     ",    "srl     ",
   "",            "",            "sra     ",    "sra     ",
   "iret    ",    "halt    ",    "",            "",
   "iretinv ",    "",            "",            "",

   /* Opcodes 0x90 to 0x9F */
   "and     ",    "and     ",    "or      ",    "or      ",
   "xor     ",    "xor     ",    "xnor    ",    "xnor    ",
   "nor     ",    "nor     ",    "nand    ",    "nand    ",
   "andn    ",    "andn    ",    "setip   ",    "inv     ",

   /* Opcodes 0xA0 to 0xAF */
   "jmp     ",    "jmp     ",    "",            "",
   "jmpf    ",    "jmpf    ",    "",            "",
   "call    ",    "call    ",    "orn     ",    "orn     ",
   "jmpt    ",    "jmpt    ",    "",            "",

   /* Opcodes 0xB0 to 0xBF */
   "",            "",            "",            "",
   "jmpfdec ",    "jmpfdec ",    "mftlb   ",    "",
   "",            "",            "",            "",
   "",            "",            "mttlb   ",    "",

   /* Opcodes 0xC0 to 0xCF */
   "jmpi    ",    "",            "",            "",
   "jmpfi   ",    "",            "mfsr    ",    "",
   "calli   ",    "",            "",            "",
   "jmpti   ",    "",            "mtsr    ",    "",

   /* Opcodes 0xD0 to 0xDF */
   "",            "",            "",            "",
   "",            "",            "",            "emulate ",
   "",            "",            "",            "",
   "",            "",            "multm   ",    "multmu  ",

   /* Opcodes 0xE0 to 0xEF */
   "multiply",    "divide  ",    "multiplu",    "dividu  ",
   "convert ",    "sqrt    ",    "class   ",    "",
   "",            "",            "feq     ",    "deq     ",
   "fgt     ",    "dgt     ",    "fge     ",    "dge     ",

   /* Opcodes 0xF0 to 0xFF */
   "fadd    ",    "dadd    ",    "fsub    ",    "dsub    ",
   "fmul    ",    "dmul    ",    "fdiv    ",    "ddiv    ",
   "",            "fdmul   ",    "",            "",
   "",            "",            "",            ""
   };


