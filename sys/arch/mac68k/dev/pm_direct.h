/*  pm_direct.h 1.0 01/02/97 Takashi Hamada */

/*
 * Copyright (C) 1997 Takashi Hamada
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
 *  This product includes software developed by Takashi Hamada.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Public declarations that other routines may need.
 */

/* data structure of the command of the Power Manager */
typedef	struct	{
	short	command;		/* command of the Power Manager 	*/
	short	num_data;		/* number of data 					*/
	char	*s_buf;			/* pointer to buffer for sending 	*/
	char	*r_buf;			/* pointer to buffer for receiving	*/
	char	data[32];		/* data buffer (is it too much?)	*/
}	PMData;

int		pmgrop(PMData *);

extern void pm_setup_adb __P((void));
extern void pm_check_adb_devices __P((int));
extern void pm_intr __P((void));
extern int pm_adb_op __P((u_char *, void *, void *, int));
extern void pm_init_adb_device __P((void));

