
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/


#include <curses.priv.h>

#include <term.h>

MODULE_ID("Id: lib_print.c,v 1.8 1996/12/21 14:24:06 tom Exp $")

int mcprint(char *data, int len)
/* ship binary character data to the printer via mc4/mc5/mc5p */
{
    char	*mybuf, *switchon;
    size_t	onsize,	offsize, res;

    errno = 0;
    if (!prtr_non && (!prtr_on || !prtr_off))
    {
	errno = ENODEV;
	return(ERR);
    }

    if (prtr_non)
    {
	switchon = tparm(prtr_non, len);
	onsize = strlen(switchon);
	offsize = 0;
    }
    else
    {
	switchon = prtr_on;
	onsize = strlen(prtr_on);
	offsize = strlen(prtr_off);
    }

    if ((mybuf = malloc(onsize + len + offsize + 1)) == (char *)NULL)
    {
	errno = ENOMEM;
	return(ERR);
    }

    (void) strcpy(mybuf, switchon);
    memcpy(mybuf + onsize, data, len);
    if (offsize)
      (void) strcpy(mybuf + onsize + len, prtr_off);

    /*
     * We're relying on the atomicity of UNIX writes here.  The
     * danger is that output from a refresh() might get interspersed
     * with the printer data after the write call returns but before the
     * data has actually been shipped to the terminal.  If the write(2)
     * operation is truly atomic we're protected from this.
     */
    res = write(cur_term->Filedes, mybuf, onsize + len + offsize);

    /*
     * By giving up our scheduler slot here we increase the odds that the
     * kernel will ship the contiguous clist items from the last write
     * immediately.
     */
    (void) sleep(0);

    free(mybuf);
    return(res);
}
