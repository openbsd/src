/*
 * Mach Operating System
 * Copyright (c) 1991, 1992 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
#ifndef UNDEFINED
# define UNDEFINED _unknown_handler
/* vector 0x00 (#0)     word   _address_handler */
#endif
/* vector 0x00 (#0)   */  word   _reset_handler
/* vector 0x01 (#1)   */  word   _interrupt_handler
/* vector 0x02 (#2)   */  word   _instruction_access_handler
/* vector 0x03 (#3)   */  word   _data_exception_handler
/* vector 0x04 (#4)   */  word   _misaligned_handler
/* vector 0x05 (#5)   */  word   _unimplemented_handler
/* vector 0x06 (#6)   */  word   _privilege_handler
/* vector 0x07 (#7)   */  word   _bounds_handler
/* vector 0x08 (#8)   */  word   _divide_handler
/* vector 0x09 (#9)   */  word   _overflow_handler
/* vector 0x0a (#10)  */  word   _error_handler
/* vector 0x0b (#11)  */  word   UNDEFINED
/* vector 0x0c (#12)  */  word   UNDEFINED
/* vector 0x0d (#13)  */  word   UNDEFINED
/* vector 0x0e (#14)  */  word   UNDEFINED
/* vector 0x0f (#15)  */  word   UNDEFINED
/* vector 0x10 (#16)  */  word   UNDEFINED
/* vector 0x11 (#17)  */  word   UNDEFINED
/* vector 0x12 (#18)  */  word   UNDEFINED
/* vector 0x13 (#19)  */  word   UNDEFINED
/* vector 0x14 (#20)  */  word   UNDEFINED
/* vector 0x15 (#21)  */  word   UNDEFINED
/* vector 0x16 (#22)  */  word   UNDEFINED
/* vector 0x17 (#23)  */  word   UNDEFINED
/* vector 0x18 (#24)  */  word   UNDEFINED
/* vector 0x19 (#25)  */  word   UNDEFINED
/* vector 0x1a (#26)  */  word   UNDEFINED
/* vector 0x1b (#27)  */  word   UNDEFINED
/* vector 0x1c (#28)  */  word   UNDEFINED
/* vector 0x1d (#29)  */  word   UNDEFINED
/* vector 0x1e (#30)  */  word   UNDEFINED
/* vector 0x1f (#31)  */  word   UNDEFINED
/* vector 0x20 (#32)  */  word   UNDEFINED
/* vector 0x21 (#33)  */  word   UNDEFINED
/* vector 0x22 (#34)  */  word   UNDEFINED
/* vector 0x23 (#35)  */  word   UNDEFINED
/* vector 0x24 (#36)  */  word   UNDEFINED
/* vector 0x25 (#37)  */  word   UNDEFINED
/* vector 0x26 (#38)  */  word   UNDEFINED
/* vector 0x27 (#39)  */  word   UNDEFINED
/* vector 0x28 (#40)  */  word   UNDEFINED
/* vector 0x29 (#41)  */  word   UNDEFINED
/* vector 0x2a (#42)  */  word   UNDEFINED
/* vector 0x2b (#43)  */  word   UNDEFINED
/* vector 0x2c (#44)  */  word   UNDEFINED
/* vector 0x2d (#45)  */  word   UNDEFINED
/* vector 0x2e (#46)  */  word   UNDEFINED
/* vector 0x2f (#47)  */  word   UNDEFINED
/* vector 0x30 (#48)  */  word   UNDEFINED
/* vector 0x31 (#49)  */  word   UNDEFINED
/* vector 0x32 (#50)  */  word   UNDEFINED
/* vector 0x33 (#51)  */  word   UNDEFINED
/* vector 0x34 (#52)  */  word   UNDEFINED
/* vector 0x35 (#53)  */  word   UNDEFINED
/* vector 0x36 (#54)  */  word   UNDEFINED
/* vector 0x37 (#55)  */  word   UNDEFINED
/* vector 0x38 (#56)  */  word   UNDEFINED
/* vector 0x39 (#57)  */  word   UNDEFINED
/* vector 0x3a (#58)  */  word   UNDEFINED
/* vector 0x3b (#59)  */  word   UNDEFINED
/* vector 0x3c (#60)  */  word   UNDEFINED
/* vector 0x3d (#61)  */  word   UNDEFINED
/* vector 0x3e (#62)  */  word   UNDEFINED
/* vector 0x3f (#63)  */  word   UNDEFINED
/* vector 0x40 (#64)  */  word   UNDEFINED
/* vector 0x41 (#65)  */  word   UNDEFINED
/* vector 0x42 (#66)  */  word   UNDEFINED
/* vector 0x43 (#67)  */  word   UNDEFINED
/* vector 0x44 (#68)  */  word   UNDEFINED
/* vector 0x45 (#69)  */  word   UNDEFINED
/* vector 0x46 (#70)  */  word   UNDEFINED
/* vector 0x47 (#71)  */  word   UNDEFINED
/* vector 0x48 (#72)  */  word   UNDEFINED
/* vector 0x49 (#73)  */  word   UNDEFINED
/* vector 0x4a (#74)  */  word   UNDEFINED
/* vector 0x4b (#75)  */  word   UNDEFINED
/* vector 0x4c (#76)  */  word   UNDEFINED
/* vector 0x4d (#77)  */  word   UNDEFINED
/* vector 0x4e (#78)  */  word   UNDEFINED
/* vector 0x4f (#79)  */  word   UNDEFINED
/* vector 0x50 (#80)  */  word   UNDEFINED
/* vector 0x51 (#81)  */  word   UNDEFINED
/* vector 0x52 (#82)  */  word   UNDEFINED
/* vector 0x53 (#83)  */  word   UNDEFINED
/* vector 0x54 (#84)  */  word   UNDEFINED
/* vector 0x55 (#85)  */  word   UNDEFINED
/* vector 0x56 (#86)  */  word   UNDEFINED
/* vector 0x57 (#87)  */  word   UNDEFINED
/* vector 0x58 (#88)  */  word   UNDEFINED
/* vector 0x59 (#89)  */  word   UNDEFINED
/* vector 0x5a (#90)  */  word   UNDEFINED
/* vector 0x5b (#91)  */  word   UNDEFINED
/* vector 0x5c (#92)  */  word   UNDEFINED
/* vector 0x5d (#93)  */  word   UNDEFINED
/* vector 0x5e (#94)  */  word   UNDEFINED
/* vector 0x5f (#95)  */  word   UNDEFINED
/* vector 0x60 (#96)  */  word   UNDEFINED
/* vector 0x61 (#97)  */  word   UNDEFINED
/* vector 0x62 (#98)  */  word   UNDEFINED
/* vector 0x63 (#99)  */  word   UNDEFINED
/* vector 0x64 (#100) */  word   UNDEFINED
/* vector 0x65 (#101) */  word   UNDEFINED
/* vector 0x66 (#102) */  word   UNDEFINED
/* vector 0x67 (#103) */  word   UNDEFINED
/* vector 0x68 (#104) */  word   UNDEFINED
/* vector 0x69 (#105) */  word   UNDEFINED
/* vector 0x6a (#106) */  word   UNDEFINED
/* vector 0x6b (#107) */  word   UNDEFINED
/* vector 0x6c (#108) */  word   UNDEFINED
/* vector 0x6d (#109) */  word   UNDEFINED
/* vector 0x6e (#110) */  word   UNDEFINED
/* vector 0x6f (#111) */  word   UNDEFINED
/* vector 0x70 (#112) */  word   UNDEFINED
/* vector 0x71 (#113) */  word   UNDEFINED
/* vector 0x72 (#114) */  word   fp_precise_handler
/* vector 0x73 (#115) */  word   fp_imprecise_handler
/* vector 0x74 (#116) */  word   _unimplemented_handler
/* vector 0x75 (#117) */  word   UNDEFINED
/* vector 0x76 (#118) */  word   _unimplemented_handler
/* vector 0x77 (#119) */  word   UNDEFINED
/* vector 0x78 (#120) */  word   _unimplemented_handler
/* vector 0x79 (#121) */  word   UNDEFINED
/* vector 0x7a (#122) */  word   _unimplemented_handler
/* vector 0x7b (#123) */  word   UNDEFINED
/* vector 0x7c (#124) */  word   _unimplemented_handler
/* vector 0x7d (#125) */  word   UNDEFINED
/* vector 0x7e (#126) */  word   _unimplemented_handler
/* vector 0x7f (#127) */  word   UNDEFINED
/* vector 0x80 (#128) */  word   _syscall_handler
/* vector 0x81 (#129) */  word   _syscall_handler
/* vector 0x82 (#130) */  word   break
/* vector 0x83 (#131) */  word   trace
/* vector 0x84 (#132) */  word   _entry
#if defined(RAW_PRINTF) && RAW_PRINTF
/* vector 0x85 (#133) */  word   user_raw_putstr /* for USER raw_printf() */
/* vector 0x85 (#134) */  word   user_raw_xpr 	 /* for USER raw_xpr() */
#endif
