/************************************************************************/
/*	Copyright (C) 1986-1991 Phar Lap Software, Inc.			*/
/*	Unpublished - rights reserved under the Copyright Laws of the	*/
/*	United States.  Use, duplication, or disclosure by the 		*/
/*	Government is subject to restrictions as set forth in 		*/
/*	subparagraph (c)(1)(ii) of the Rights in Technical Data and 	*/
/*	Computer Software clause at 252.227-7013.			*/
/*	Phar Lap Software, Inc., 60 Aberdeen Ave., Cambridge, MA 02138	*/
/************************************************************************/
/* REALCOPY.C:  copy real mode code to conventional memory */

/*
 * The routine in this file allocates conventional memory and copies
 * real mode code to it.
 */
#include <stdio.h>
#include <pltypes.h>
#include <pharlap.h>

realcopy(start_offs, end_offs, real_basep, prot_basep, rmem_adrp)
ULONG	start_offs;
ULONG	end_offs;
REALPTR	*real_basep;
FARPTR	*prot_basep;
USHORT	*rmem_adrp;

/*
Description:
	This routine allocates conventional memory for the specified block
	of code (which must be within the first 64K of the protected mode
	program segment) and copies the code to it.

	The caller should free up the conventional memory block when it
	is done with the conventional memory.

	NOTE THIS ROUTINE REQUIRES 386|DOS-EXTENDER 3.0 OR LATER.

Calling arguments:
	start_offs	start of real mode code in program segment
	end_offs	1 byte past end of real mode code in program segment
	real_basep	returned;  real mode ptr to use as a base for the
				real mode code (eg, to get the real mode FAR
				addr of a function foo(), take
				real_basep + (ULONG) foo).
				This pointer is constructed such that
				offsets within the real mode segment are
				the same as the link-time offsets in the
				protected mode program segment
	prot_basep	returned;  prot mode ptr to use as a base for getting
				to the conventional memory, also constructed
				so that adding the prot mode offset of a
				function or variable to the base gets you a 
				ptr to the function or variable in the 
				conventional memory block.
	rmem_adrp	returned;  real mode para addr of allocated 
				conventional memory block, to be used to free
				up the conventional memory when done.  DO NOT
				USE THIS TO CONSTRUCT A REAL MODE PTR, USE
				REAL_BASEP INSTEAD SO THAT OFFSETS WORK OUT
				CORRECTLY. 

Returned values:
	TRUE		if error
	FALSE		if success
*/
{
	ULONG	rm_base;	/* base real mode para addr for accessing */
					/* allocated conventional memory */
	UCHAR	*sp;		/* source pointer for copy */
	FARPTR	dp;		/* destination pointer for copy */
	ULONG	len;		/* number of bytes to copy */
	ULONG	temp;
	USHORT	stemp;

/*
 * First check for valid inputs
 */
 	if (start_offs >= end_offs || end_offs > 0x10000)
	{
		return TRUE;
	}

/*
 * Round start_offs down to a paragraph (16-byte) boundary so we can set up
 * the real mode pointer easily.
 */
 	start_offs &= ~15;

/*
 * Allocate the conventional memory for our real mode code.  Remember to
 * round byte count UP to 16-byte paragraph size.  We alloc it
 * above the DOS data buffer so both the DOS data buffer and the appl
 * conventional mem block can still be resized.
 *
 * First just try to alloc it;  if we can't get it, shrink the appl mem
 * block down to the minimum, try to alloc the memory again, then grow the
 * appl mem block back to the maximum.  (Don't try to shrink the DOS data 
 * buffer to free conventional memory;  it wouldn't be good for this routine 
 * to have the possible side effect of making file I/O run slower.)
 */
	len = ((end_offs - start_offs) + 15) >> 4; 
	if (_dx_real_above(len, rmem_adrp, &stemp) != _DOSE_NONE)
	{
		if (_dx_cmem_usage(0, FALSE, &temp, &temp) != _DOSE_NONE)
		{
			return TRUE;
		}
		if (_dx_real_above(len, rmem_adrp, &stemp) != _DOSE_NONE)
			*rmem_adrp = 0;
		if (_dx_cmem_usage(0, TRUE, &temp, &temp) != _DOSE_NONE)
		{
			if (*rmem_adrp != 0)
				_dx_real_free(*rmem_adrp);
			return TRUE;
		}
		if (*rmem_adrp == 0)
		{
			return TRUE;
		}
	}

/*
 * Construct real mode & protected mode pointers to access the allocated 
 * memory.  Note we know start_offs is aligned on a paragraph (16-byte) 
 * boundary, because we rounded it down.
 *
 * We make the offsets come out rights by backing off the real mode selector
 * by start_offs.
 */
	rm_base = ((ULONG) *rmem_adrp) - (start_offs >> 4);
 	RP_SET(*real_basep, 0, rm_base);
	FP_SET(*prot_basep, rm_base << 4, SS_DOSMEM);

/*
 * Copy the real mode code to the allocated memory
 */
 	sp = (UCHAR *) start_offs;
	dp = *prot_basep + start_offs; 
	len = end_offs - start_offs; 
	while (len-- > 0)
		*dp++ = *sp++;
 	return FALSE;
}
