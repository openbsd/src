/*	$OpenBSD: sbcreg.h,v 1.1 1996/05/26 19:02:10 briggs Exp $	*/
/*	$NetBSD: sbcreg.h,v 1.1 1996/04/25 22:26:56 scottr Exp $	*/

/*
 * Register map for the Mac II SCSI Interface (sbc)
 * This register map is for the SYM/NCR5380 SCSI Bus Interface
 * Controller (SBIC), with the wonderful 16 bytes/register layout
 * that Macs have.
 */

/*
 * Am5380 Register map (with padding)
 */
typedef union {
	volatile u_char sci_reg;
	volatile u_char pad[16];
} ncr5380_padded_reg_t;

struct sbc_regs {
	ncr5380_padded_reg_t sci_pr0;
	ncr5380_padded_reg_t sci_pr1;
	ncr5380_padded_reg_t sci_pr2;
	ncr5380_padded_reg_t sci_pr3;
	ncr5380_padded_reg_t sci_pr4;
	ncr5380_padded_reg_t sci_pr5;
	ncr5380_padded_reg_t sci_pr6;
	ncr5380_padded_reg_t sci_pr7;
};
