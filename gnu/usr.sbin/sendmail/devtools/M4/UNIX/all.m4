divert(-1)
#
# Copyright (c) 1999 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Sendmail: all.m4,v 8.7 1999/10/13 07:08:36 gshapiro Exp $
#
divert(0)dnl
ALL=${BEFORE} ${LINKS} bldTARGETS

all: ${ALL}

clean: bldCLEAN_TARGETS

define(`bldADD_SRC', ${$1SRCS} )dnl
SRCS=bldFOREACH(`bldADD_SRC(', bldC_PRODUCTS)
define(`bldADD_OBJS', ${$1OBJS} )dnl
OBJS=bldFOREACH(`bldADD_OBJS(', bldC_PRODUCTS)

ifdef(`bldNO_INSTALL', `divert(-1)')
install: bldINSTALL_TARGETS

install-strip: bldINSTALL_TARGETS ifdef(`bldSTRIP_TARGETS', `bldSTRIP_TARGETS')
ifdef(`bldNO_INSTALL', `divert(0)')

divert(bldDEPENDENCY_SECTION)
################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
divert(0)
