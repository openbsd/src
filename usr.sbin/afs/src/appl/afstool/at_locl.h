/*
 * Copyright (c) 2002 - 2003, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "appl_locl.h"
#include <sl.h>
#include <getarg.h>
#include <vers.h>

#include <afs_uuid.h>
#include <cb.cs.h>

/* $arla: at_locl.h,v 1.7 2003/03/06 16:35:06 lha Exp $ */

void mini_cachemanager_init (void);

#define AT_SL_CMD(commandname) int commandname##_cmd(int, char **)

AT_SL_CMD(afstool);
AT_SL_CMD(cm);
AT_SL_CMD(cm_whoareyou);
AT_SL_CMD(cm_localcell);
AT_SL_CMD(fs);
AT_SL_CMD(fs_FlushCPS);
AT_SL_CMD(fs_gettime);
AT_SL_CMD(fs_getcap);
AT_SL_CMD(u);
AT_SL_CMD(u_debug);

struct rx_connection *
cbgetconn(const char *, const char *, const char *, uint32_t, int);


#define AT_STANDARD_SL_CMDS(at_std_name,at_std_cmds,at_std_str)	\
								\
static SL_cmd at_std_cmds[];					\
								\
static int							\
at_std_name##_help_cmd(int argc, char **argv)			\
{								\
    sl_help(at_std_cmds, argc, argv);				\
    return 0;							\
}								\
								\
static int							\
at_std_name##_apropos_cmd(int argc, char **argv)		\
{								\
    if (argc < 2) {						\
	printf ("apropos: missing topic\n");			\
	return 0;						\
    }								\
								\
    sl_apropos(at_std_cmds, argv[1]);				\
    return 0;							\
}								\
								\
int								\
at_std_name##_cmd(int argc, char **argv)			\
{								\
    int ret;							\
								\
    if (argc < 2) {						\
	char *str;						\
	asprintf(&str, "%s%s%s", getprogname(), 		\
		 at_std_str[0] == '\0' ? "": " ",		\
		 at_std_str);					\
	print_version(getprogname());				\
	printf("\nusage:\t%s command ...\n", str);		\
	printf("\ttry \"%s help\" for help\n", str);		\
	free(str);						\
	exit (1);						\
    }								\
								\
    ret = sl_command(at_std_cmds, argc - 1, argv + 1);		\
    if (ret != 0)						\
	sl_help(at_std_cmds, argc, argv);			\
								\
    return 0;							\
}
