/*	$NetBSD: ncr5380var.h,v 1.3 1995/09/26 19:24:26 thorpej Exp $	*/

/*
 * Copyright (C) 1994 Adam Glass, Gordon W. Ross
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define SCI_PHASE_DISC		0	/* sort of ... */
#define SCI_CLR_INTR(regs)	((volatile)(regs->sci_iack))
#define SCI_ACK(ptr,phase)	(ptr)->sci_tcmd = (phase)
#define SCSI_TIMEOUT_VAL	1000000
#define WAIT_FOR_NOT_REQ(ptr) {	\
	int scsi_timeout = SCSI_TIMEOUT_VAL; \
	while ( ((ptr)->sci_bus_csr & SCI_BUS_REQ) && \
		 ((ptr)->sci_bus_csr & SCI_BUS_REQ) && \
		 ((ptr)->sci_bus_csr & SCI_BUS_REQ) && \
		 (--scsi_timeout) ); \
	if (!scsi_timeout) { \
		printf("scsi timeout--WAIT_FOR_NOT_REQ---%s, line %d.\n", \
			__FILE__, __LINE__); \
		goto scsi_timeout_error; \
	} \
	}
#define WAIT_FOR_REQ(ptr) {	\
	int scsi_timeout = SCSI_TIMEOUT_VAL; \
	while ( (((ptr)->sci_bus_csr & SCI_BUS_REQ) == 0) && \
		(((ptr)->sci_bus_csr & SCI_BUS_REQ) == 0) && \
		(((ptr)->sci_bus_csr & SCI_BUS_REQ) == 0) && \
		 (--scsi_timeout) ); \
	if (!scsi_timeout) { \
		printf("scsi timeout--WAIT_FOR_REQ---%s, line %d.\n", \
			__FILE__, __LINE__); \
		goto scsi_timeout_error; \
	} \
	}
#define WAIT_FOR_BSY(ptr) {	\
	int scsi_timeout = SCSI_TIMEOUT_VAL; \
	while ( (((ptr)->sci_bus_csr & SCI_BUS_BSY) == 0) && \
		(((ptr)->sci_bus_csr & SCI_BUS_BSY) == 0) && \
		(((ptr)->sci_bus_csr & SCI_BUS_BSY) == 0) && \
		 (--scsi_timeout) ); \
	if (!scsi_timeout) { \
		printf("scsi timeout--WAIT_FOR_BSY---%s, line %d.\n", \
			__FILE__, __LINE__); \
		goto scsi_timeout_error; \
	} \
	}

#define ARBITRATION_RETRIES 1000

#ifndef DDB
#define Debugger() panic("Should call Debugger here %s:%d", \
			 __FILE__, __LINE__)
#endif /* ! DDB */

struct ncr5380_softc {
	struct device	sc_dev;
	struct intrhand	sc_ih;	/* interrupt info */
	volatile void	*sc_regs;
	int		sc_adapter_type;
	int		sc_adapter_iv_am; /* int. vec + address modifier */
	struct scsi_link sc_link;
};

static int	ncr5380_reset_scsibus __P((struct ncr5380_softc *));
static int	ncr5380_poll __P((int, int));
static void	ncr5380_sbc_intr __P((struct ncr5380_softc *));
static int	ncr5380_send_cmd __P((struct scsi_xfer *));
static int	ncr5380_scsi_cmd __P((struct scsi_xfer *));
static int	ncr5380_data_in __P((sci_regmap_t *, int, int, u_char *));
static int	ncr5380_data_out __P((sci_regmap_t *, int, int, u_char *));
static int	ncr5380_select_target __P((volatile sci_regmap_t *, u_char,
					   u_char, int));
static int	ncr5380_command_transfer __P((volatile sci_regmap_t *, int,
					      u_char *, u_char *, u_char *));
static int	ncr5380_data_transfer __P((volatile sci_regmap_t *, int,
					   u_char *, u_char *, u_char *));
static int	ncr5380_dorequest __P((struct ncr5380_softc *, int, int,
				       u_char *, int, char *, int, int *));

static int	ncr5380_generic __P((void *, int, int, struct scsi_generic *,
				     int, void *, int));
static int	ncr5380_group0 __P((void *, int, int, int, int, int,
				    int, caddr_t, int));


