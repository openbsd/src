#	$OpenBSD: L_ENTRY.AWK,v 1.2 1996/05/29 21:05:21 niklas Exp $
#	$NetBSD: L_ENTRY.AWK,v 1.2 1994/10/26 07:48:34 cgd Exp $

#	MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
#	M68000 Hi-Performance Microprocessor Division
#	M68040 Software Package 
#
#	M68040 Software Package Copyright (c) 1993, 1994 Motorola Inc.
#	All rights reserved.
#
#	THE SOFTWARE is provided on an "AS IS" basis and without warranty.
#	To the maximum extent permitted by applicable law,
#	MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
#	INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A
#	PARTICULAR PURPOSE and any warranty against infringement with
#	regard to the SOFTWARE (INCLUDING ANY MODIFIED VERSIONS THEREOF)
#	and any accompanying written materials. 
#
#	To the maximum extent permitted by applicable law,
#	IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
#	(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS
#	PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR
#	OTHER PECUNIARY LOSS) ARISING OF THE USE OR INABILITY TO USE THE
#	SOFTWARE.  Motorola assumes no responsibility for the maintenance
#	and support of the SOFTWARE.  
#
#	You are hereby granted a copyright license to use, modify, and
#	distribute the SOFTWARE so long as this entire notice is retained
#	without alteration in any modified and/or redistributed versions,
#	and that such modified versions are clearly identified as such.
#	No licenses are granted by implication, estoppel or otherwise
#	under any patents or trademarks of Motorola, Inc.

#	L_ENTRY.AWK 1.1 3/27/91

BEGIN{
	print "echo \"	section 8\""
	print "echo \"	include	l_fpsp.h\""
	print "echo \"\""
	print "echo \"	xref	tag\""
	print "echo \"	xref	szero\""
	print "echo \"	xref	sinf\""
	print "echo \"	xref	sopr_inf\""
	print "echo \"	xref	sone\""
	print "echo \"	xref	spi_2\""
	print "echo \"	xref	szr_inf\""
	print "echo \"	xref	src_nan\""
	print "echo \"	xref	t_operr\""
	print "echo \"	xref	t_dz2\""
	print "echo \"	xref	snzrinx\""
	print "echo \"	xref	ld_pone\""
	print "echo \"	xref	ld_pinf\""
	print "echo \"	xref	ld_ppi2\""
	print "echo \"	xref	ssincosz\""
	print "echo \"	xref	ssincosi\""
	print "echo \"	xref	ssincosnan\""
	print "echo \"	xref	setoxm1i\""
	utmp = 100
	}

$4=="MONADIC"{
	printf "sed 's/_OPs_/" $1 "/g' MONADIC." SYS " | "
	printf "sed 's/_OPd_/" $2 "/g' | "
	printf "sed 's/_OPx_/" $3 "/g' | "
	printf "sed 's/_OPr_/" $5 "/g' | "
	printf "sed 's/_OPz_/" $6 "/g' | "
	printf "sed 's/_OPi_/" $7 "/g' | "
	printf "sed 's/_OPn_/" $8 "/g' | "
	printf "sed 's/_OPm_/" $9 "/g' | "
	utmp += 1
	printf "sed 's/_TMP_/" PREFIX utmp "/g'\n "
	}

$4=="DYADIC"{
	printf "sed 's/_OPs_/" $1 "/g' DYADIC." SYS " | "
	printf "sed 's/_OPd_/" $2 "/g' | "
	printf "sed 's/_OPx_/" $3 "/g' | "
	printf "sed 's/_OPa_/" $5 "/g' | "
	utmp += 1
	printf "sed 's/_TMP_/" PREFIX utmp "/g'\n"
	}

END{
	print "echo \"	end\""
	}
