# $Id: Makefile.am,v 1.3 2013/06/17 19:11:35 robert Exp $

include $(top_srcdir)/Makefile.am.common

if OTP
dir_otp = otp
endif
if DCE
dir_dce = dceutils
endif
SUBDIRS = 					\
	  afsutil				\
	  ftp					\
	  login					\
	  $(dir_otp)				\
	  gssmask				\
	  popper				\
	  push					\
	  rsh					\
	  rcp					\
	  su					\
	  xnlock				\
	  telnet				\
	  test					\
	  kx					\
	  kf					\
	  $(dir_dce)

EXTRA_DIST = NTMakefile