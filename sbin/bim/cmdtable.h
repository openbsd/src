/*	$NetBSD: cmdtable.h,v 1.2 1995/03/18 12:28:13 cgd Exp $	*/

/* 
 * Copyright (c) 1994 Philip A. Nelson.
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
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  cmdtable.h - this is where the command table is defined.
 *
 *  The user must complete this command table.  It is started.
 */

/* procedure definitions. */
#ifndef NO_HELP
CMD_PROC (help);
#endif

/* hdsetup commands. */
CMD_PROC (write_exit);
CMD_PROC (set_default_image);
CMD_PROC (add_image);
CMD_PROC (delete_image);
CMD_PROC (display_part);
CMD_PROC (display_image);
CMD_PROC (display_head);
CMD_PROC (initialize);

/* The command definitions. This is where the user should add new
   command definitions.

   Field definition:

   { proc_name, "command_name", "Syntax", "Help" }

   NOTE:  For an alphabetical list from the help command, list the
   commands in alphabetical order on the name field. */

CONST
struct command cmd_table [] = {		/* Command Table */

{ add_image, "add",
"ADD <file_name> [image_name].",
"Add the executable file to the image table.  The image name will\n\
be the same as the <file_name> unless otherwise specified."
},

{ delete_image, "delete",
"DELETE <image_id>.",
"Delete the specified image from the image table."
},

{ set_default_image, "default", "DEFAULT <image_number>.",
"Specifies the image that shall be used during an autoboot or a monitor\n\
boot command without any arguments."
},

{ write_exit, "exit", "EXIT",
"Terminates the command processor writing the header if necessary."
},

{ display_head, "header", "HEADER",
"Display the disk label header information without the partitions."
},

#ifndef NO_HELP
{ help, "help", "HELP [<command> ...].",
"Provides help for all listed <command>s.  If there none, prints a list \n\
of the commands."
},
#endif

{ display_image, "images", "IMAGES",
"Display the image information."
},

{ initialize, "init", "INIT",
"Initialize the image information."
},

{ display_part, "partitions", "PARTITIONS",
"Display the partition information.  Unused partitions are not displayed."
},

#ifndef NO_HELP
{ help, "?", "",
"Prints a list of commands."
},
#endif

{ write_exit, "quit", "QUIT",
"Same as exit."
},

};

#define CMDLEN  (sizeof (cmd_table) / sizeof (struct command))


/* The prompt! */
#define PROMPT "bim (? for help): "
