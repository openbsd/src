/*	$OpenBSD: exception_vectors2.h,v 1.6 2001/12/22 17:57:11 smurph Exp $ */
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
/*#define M88110_UNDEFINED PREDEFINED_BY_ROM*/
#ifndef __MACHINE_EXECPTION_VECTORS2_H__
#define __MACHINE_EXECPTION_VECTORS2_H__
#ifndef M88110_UNDEFINED
#define M88110_UNDEFINED _m88110_unknown_handler
#endif

/* vector 0x00 (#0)   */  word   _m88110_reset_handler
/* vector 0x01 (#1)   */  word   _m88110_interrupt_handler
/* vector 0x02 (#2)   */  word   _m88110_instruction_access_handler
/* vector 0x03 (#3)   */  word   _m88110_data_exception_handler
/* vector 0x04 (#4)   */  word   _m88110_misaligned_handler
/* vector 0x05 (#5)   */  word   _m88110_unimplemented_handler
/* vector 0x06 (#6)   */  word   _m88110_privilege_handler
/* vector 0x07 (#7)   */  word   _m88110_bounds_handler
/* vector 0x08 (#8)   */  word   _m88110_divide_handler
/* vector 0x09 (#9)   */  word   _m88110_overflow_handler
/* vector 0x0a (#10)  */  word   _m88110_error_handler
/* vector 0x0b (#11)  */  word   _m88110_nonmaskable
/* vector 0x0c (#12)  */  word   _m88110_data_read_miss
/* vector 0x0d (#13)  */  word   _m88110_data_write_miss
/* vector 0x0e (#14)  */  word   _m88110_inst_atc_miss
/* vector 0x0f (#15)  */  word   _m88110_trace
/* vector 0x10 (#16)  */  word   M88110_UNDEFINED
/* vector 0x11 (#17)  */  word   M88110_UNDEFINED
/* vector 0x12 (#18)  */  word   M88110_UNDEFINED
/* vector 0x13 (#19)  */  word   M88110_UNDEFINED
/* vector 0x14 (#20)  */  word   M88110_UNDEFINED
/* vector 0x15 (#21)  */  word   M88110_UNDEFINED
/* vector 0x16 (#22)  */  word   M88110_UNDEFINED
/* vector 0x17 (#23)  */  word   M88110_UNDEFINED
/* vector 0x18 (#24)  */  word   M88110_UNDEFINED
/* vector 0x19 (#25)  */  word   M88110_UNDEFINED
/* vector 0x1a (#26)  */  word   M88110_UNDEFINED
/* vector 0x1b (#27)  */  word   M88110_UNDEFINED
/* vector 0x1c (#28)  */  word   M88110_UNDEFINED
/* vector 0x1d (#29)  */  word   M88110_UNDEFINED
/* vector 0x1e (#30)  */  word   M88110_UNDEFINED
/* vector 0x1f (#31)  */  word   M88110_UNDEFINED
/* vector 0x20 (#32)  */  word   M88110_UNDEFINED
/* vector 0x21 (#33)  */  word   M88110_UNDEFINED
/* vector 0x22 (#34)  */  word   M88110_UNDEFINED
/* vector 0x23 (#35)  */  word   M88110_UNDEFINED
/* vector 0x24 (#36)  */  word   M88110_UNDEFINED
/* vector 0x25 (#37)  */  word   M88110_UNDEFINED
/* vector 0x26 (#38)  */  word   M88110_UNDEFINED
/* vector 0x27 (#39)  */  word   M88110_UNDEFINED
/* vector 0x28 (#40)  */  word   M88110_UNDEFINED
/* vector 0x29 (#41)  */  word   M88110_UNDEFINED
/* vector 0x2a (#42)  */  word   M88110_UNDEFINED
/* vector 0x2b (#43)  */  word   M88110_UNDEFINED
/* vector 0x2c (#44)  */  word   M88110_UNDEFINED
/* vector 0x2d (#45)  */  word   M88110_UNDEFINED
/* vector 0x2e (#46)  */  word   M88110_UNDEFINED
/* vector 0x2f (#47)  */  word   M88110_UNDEFINED
/* vector 0x30 (#48)  */  word   M88110_UNDEFINED
/* vector 0x31 (#49)  */  word   M88110_UNDEFINED
/* vector 0x32 (#50)  */  word   M88110_UNDEFINED
/* vector 0x33 (#51)  */  word   M88110_UNDEFINED
/* vector 0x34 (#52)  */  word   M88110_UNDEFINED
/* vector 0x35 (#53)  */  word   M88110_UNDEFINED
/* vector 0x36 (#54)  */  word   M88110_UNDEFINED
/* vector 0x37 (#55)  */  word   M88110_UNDEFINED
/* vector 0x38 (#56)  */  word   M88110_UNDEFINED
/* vector 0x39 (#57)  */  word   M88110_UNDEFINED
/* vector 0x3a (#58)  */  word   M88110_UNDEFINED
/* vector 0x3b (#59)  */  word   M88110_UNDEFINED
/* vector 0x3c (#60)  */  word   M88110_UNDEFINED
/* vector 0x3d (#61)  */  word   M88110_UNDEFINED
/* vector 0x3e (#62)  */  word   M88110_UNDEFINED
/* vector 0x3f (#63)  */  word   M88110_UNDEFINED
/* vector 0x40 (#64)  */  word   M88110_UNDEFINED
/* vector 0x41 (#65)  */  word   M88110_UNDEFINED
/* vector 0x42 (#66)  */  word   M88110_UNDEFINED
/* vector 0x43 (#67)  */  word   M88110_UNDEFINED
/* vector 0x44 (#68)  */  word   M88110_UNDEFINED
/* vector 0x45 (#69)  */  word   M88110_UNDEFINED
/* vector 0x46 (#70)  */  word   M88110_UNDEFINED
/* vector 0x47 (#71)  */  word   M88110_UNDEFINED
/* vector 0x48 (#72)  */  word   M88110_UNDEFINED
/* vector 0x49 (#73)  */  word   M88110_UNDEFINED
/* vector 0x4a (#74)  */  word   M88110_UNDEFINED
/* vector 0x4b (#75)  */  word   M88110_UNDEFINED
/* vector 0x4c (#76)  */  word   M88110_UNDEFINED
/* vector 0x4d (#77)  */  word   M88110_UNDEFINED
/* vector 0x4e (#78)  */  word   M88110_UNDEFINED
/* vector 0x4f (#79)  */  word   M88110_UNDEFINED
/* vector 0x50 (#80)  */  word   M88110_UNDEFINED
/* vector 0x51 (#81)  */  word   M88110_UNDEFINED
/* vector 0x52 (#82)  */  word   M88110_UNDEFINED
/* vector 0x53 (#83)  */  word   M88110_UNDEFINED
/* vector 0x54 (#84)  */  word   M88110_UNDEFINED
/* vector 0x55 (#85)  */  word   M88110_UNDEFINED
/* vector 0x56 (#86)  */  word   M88110_UNDEFINED
/* vector 0x57 (#87)  */  word   M88110_UNDEFINED
/* vector 0x58 (#88)  */  word   M88110_UNDEFINED
/* vector 0x59 (#89)  */  word   M88110_UNDEFINED
/* vector 0x5a (#90)  */  word   M88110_UNDEFINED
/* vector 0x5b (#91)  */  word   M88110_UNDEFINED
/* vector 0x5c (#92)  */  word   M88110_UNDEFINED
/* vector 0x5d (#93)  */  word   M88110_UNDEFINED
/* vector 0x5e (#94)  */  word   M88110_UNDEFINED
/* vector 0x5f (#95)  */  word   M88110_UNDEFINED
/* vector 0x60 (#96)  */  word   M88110_UNDEFINED
/* vector 0x61 (#97)  */  word   M88110_UNDEFINED
/* vector 0x62 (#98)  */  word   M88110_UNDEFINED
/* vector 0x63 (#99)  */  word   M88110_UNDEFINED
/* vector 0x64 (#100) */  word   M88110_UNDEFINED
/* vector 0x65 (#101) */  word   M88110_UNDEFINED
/* vector 0x66 (#102) */  word   M88110_UNDEFINED
/* vector 0x67 (#103) */  word   M88110_UNDEFINED
/* vector 0x68 (#104) */  word   M88110_UNDEFINED
/* vector 0x69 (#105) */  word   M88110_UNDEFINED
/* vector 0x6a (#106) */  word   M88110_UNDEFINED
/* vector 0x6b (#107) */  word   M88110_UNDEFINED
/* vector 0x6c (#108) */  word   M88110_UNDEFINED
/* vector 0x6d (#109) */  word   M88110_UNDEFINED
/* vector 0x6e (#110) */  word   M88110_UNDEFINED
/* vector 0x6f (#111) */  word   M88110_UNDEFINED
/* vector 0x70 (#112) */  word   M88110_UNDEFINED
/* vector 0x71 (#113) */  word   M88110_UNDEFINED
/* vector 0x72 (#114) */  word   _m88110_fp_precise_handler
/* vector 0x73 (#115) */  word   M88110_UNDEFINED
/* vector 0x74 (#116) */  word   _m88110_unimplemented_handler
/* vector 0x75 (#117) */  word   M88110_UNDEFINED
/* vector 0x76 (#118) */  word   _m88110_unimplemented_handler
/* vector 0x77 (#119) */  word   M88110_UNDEFINED
/* vector 0x78 (#120) */  word   _m88110_unimplemented_handler
/* vector 0x79 (#121) */  word   M88110_UNDEFINED
/* vector 0x7a (#122) */  word   _m88110_unimplemented_handler
/* vector 0x7b (#123) */  word   M88110_UNDEFINED
/* vector 0x7c (#124) */  word   _m88110_unimplemented_handler
/* vector 0x7d (#125) */  word   M88110_UNDEFINED
/* vector 0x7e (#126) */  word   _m88110_unimplemented_handler
/* vector 0x7f (#127) */  word   M88110_UNDEFINED
/* vector 0x80 (#128) */  word   _m88110_syscall_handler
/* vector 0x81 (#129) */  word   _m88110_syscall_handler
/* vector 0x82 (#130) */  word   _m88110_break
/* vector 0x83 (#131) */  word   _m88110_trace
/* vector 0x84 (#132) */  word   _m88110_entry
#endif /* __MACHINE_EXECPTION_VECTORS2_H__ */

