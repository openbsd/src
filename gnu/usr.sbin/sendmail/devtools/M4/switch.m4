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
#	$Sendmail: switch.m4,v 8.13 1999/09/07 17:21:50 ca Exp $
#
divert(0)dnl
include(confBUILDTOOLSDIR`/M4/string.m4')dnl
include(confBUILDTOOLSDIR`/M4/list.m4')dnl
include(confBUILDTOOLSDIR`/M4/subst_ext.m4')dnl
define(`bldDEPENDENCY_SECTION', `3')dnl
define(`bldTARGETS_SECTION', `6')dnl
define(`bldPUSH_TARGET',
	`bldLIST_PUSH_ITEM(`bldTARGETS', `$1')'dnl
)dnl

define(`bldPUSH_INSTALL_TARGET',
	`bldLIST_PUSH_ITEM(`bldINSTALL_TARGETS', `$1')'dnl
)dnl

define(`bldPUSH_CLEAN_TARGET',
	`bldLIST_PUSH_ITEM(`bldCLEAN_TARGETS', `$1')'dnl
)dnl

define(`bldPUSH_ALL_SRCS',
	`bldLIST_PUSH_ITEM(`bldALL_SRCS', `$1')'dnl
)dnl

define(`bldPUSH_SMDEPLIB',
	`bldLIST_PUSH_ITEM(`bldSMDEPLIBS', `$1')'dnl
)dnl

define(`bldPUSH_SMLIB',
	`bldPUSH_TARGET(`../lib$1/lib$1.a')
bldPUSH_SMDEPLIB(`../lib$1/lib$1.a')
PREPENDDEF(`confLIBDIRS', `-L../lib$1')
PREPENDDEF(`confLIBS', `-l$1')
divert(bldTARGETS_SECTION)
../lib$1/lib$1.a:
	(cd ${SRCDIR}/lib$1; sh Build ${SENDMAIL_BUILD_FLAGS})
divert
')dnl

define(`bldPUSH_STRIP_TARGET',
	`bldLIST_PUSH_ITEM(`bldSTRIP_TARGETS', `$1')'dnl
)dnl

define(`bldPRODUCT_START',
`define(`bldCURRENT_PRODUCT', `$2')dnl
define(`bldCURRENT_PRD', translit(`$2', `.', `_'))dnl
define(`bldPRODUCT_TYPE', `$1')dnl'
)dnl

define(`bldM4_TYPE_DIR',ifdef(`confNT', `NT', ``UNIX''))dnl

define(`bldPRODUCT_END',
`include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/'bldPRODUCT_TYPE`.m4')'
)dnl

define(`bldFINISH', 
ifdef(`bldDONT_INCLUDE_ALL', ,``include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/all.m4')'')dnl
undivert(bldTARGETS_SECTION)dnl
)dnl
