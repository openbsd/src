/*******************************************************************************
*
*  Perl/Pollution/Portability
*
********************************************************************************
*
*  $Revision: 9 $
*  $Author: mhx $
*  $Date: 2007/01/02 12:32:27 +0100 $
*
********************************************************************************
*
*  Version 3.x, Copyright (C) 2004-2007, Marcus Holland-Moritz.
*  Version 2.x, Copyright (C) 2001, Paul Marquess.
*  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
*
*  This program is free software; you can redistribute it and/or
*  modify it under the same terms as Perl itself.
*
*******************************************************************************/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifndef PATCHLEVEL
#include "patchlevel.h"
#endif

#define NEED_newCONSTSUB_GLOBAL
#define NEED_PL_signals_GLOBAL
#include "ppport.h"

void call_newCONSTSUB_2(void)
{
  newCONSTSUB(gv_stashpv("Devel::PPPort", FALSE), "test_value_2", newSViv(2));
}

U32 get_PL_signals_2(void)
{
  return PL_signals;
}
