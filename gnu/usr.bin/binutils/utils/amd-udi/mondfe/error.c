static char _[] = "@(#)error.c	5.20 93/07/30 16:38:29, Srini, AMD.";
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 **       This file defines the error and warning messages in the
 **       monitor.  The error codes which correspond to the messages
 **       are defined in the error.h header file.
 *****************************************************************************
 */


char *error_msg[] = {

/* 0 */ "Dummy error message (0).",

/* 1 (EMUSAGE) */
" Valid command-line options are:\n\
 [-D] - to invoke in interactive debug mode\n\
 -TIP <tip_id_from_udi_config_file> - MUST specify the TIP to use\n\
 [-e <echo_ filename_for_script>] - to capture session in a file\n\
 [-w] - specifies how long to wait, -1 means WaitForever, default 10000\n\
 [-q] - to suppress download messages\n\
 [-le] - to specify little endian target (default is BIG)\n\
 [-c <input_cmd_filename>] - to specify command file for input\n\
 [-ms <mem_stack_size_in_hex> ] - memory stack size to be used for appln\n\
 [-rs <reg_stack_size_in_hex> ] - register stack size to be used for appln\n\
 [-log <log_filename>] - file to log debug session\n\
 [[<pgm>] [<pgm_args>]] - program and its optional arg list\n",
/*  2 */  "EMFAIL:  Unrecoverable error.",
/*  3 */  "EMBADADDR:  Illegal address.",
/*  4 */  "EMBADREG:  Illegal register.",
/*  5 */  "EMSYNTAX:  Illegal command syntax.",
/*  6 */  "EMACCESS:  Could not access memory.",
/*  7 */  "EMALLOC:  Could not allocate memory.",
/*  8 */  "EMTARGET:  Unknown target type.",
/*  9 */  "EMHINIT:  Could not initialize host.",
/* 10 */  "EMCOMM:  Could not open communication channel.",

/* 11 */  "EMBADMSG:  Unknown message type.",
/* 12 */  "EMMSG2BIG:  Message too large for buffer.",

/* 13 */  "EMRESET:  Could not RESET target.",
/* 14 */  "EMCONFIG:  Could not get target CONFIG.",
/* 15 */  "EMSTATUS:  Could not get target STATUS.",
/* 16 */  "EMREAD:  Could not READ target memory.",
/* 17 */  "EMWRITE:  Could not WRITE target memory.",
/* 18 */  "EMBKPTSET:  Could not set breakpoint.",
/* 19 */  "EMBKPTRM:  Could not remove breakpoint.",
/* 20 */  "EMBKPTSTAT:  Could not get breakpoint status.",
/* 21 */  "EMBKPTNONE:  All breakpoints in use.",
/* 22 */  "EMBKPTUSED:  Breakpoint already in use",
/* 23 */  "EMCOPY:  Could not COPY target memory.",
/* 24 */  "EMFILL:  Could not FILL target memory.",
/* 25 */  "EMINIT:  Could not initialize target memory.",
/* 26 */  "EMGO:  Could not start execution.",
/* 27 */  "EMSTEP:  Could not single step.",
/* 28 */  "EMBREAK:  Could not BREAK execution.",
/* 29 */  "EMHIF:  Could not perform HIF service.",
/* 30 */  "EMCHANNEL0:  Could not read CHANNEL0.",
/* 31 */  "EMCHANNEL1:  Could not write CHANNEL1.",
/* 32 */  "EMOPEN:  Could not open COFF file.",
/* 33 */  "EMHDR:  Could not read COFF header.",
/* 34 */  "EMMAGIC:  Bad COFF file magic number.",
/* 35 */  "EMAOUT:  Could not read COFF a.out header.",
/* 36 */  "EMSCNHDR:  Could not read COFF section header.",
/* 37 */  "EMSCN:  Could not read COFF section.",
/* 38 */  "EMCLOSE:  Could not close COFF file.",
/* 39 */  "EMLOGOPEN:  Could not open log file.",
/* 40 */  "EMLOGREAD:  Could not read log file.",
/* 41 */  "EMLOGWRITE:  Could not write log file.",
/* 42 */  "EMLOGCLOSE:  Could not close log file.",
/* 43 */  "EMCMDOPEN:  Could not open command file.",
/* 44 */  "EMCMDREAD:  Could not read comand file.",
/* 45 */  "EMCMDWRITE:  Could not write command file.",
/* 46 */  "EMCMDCLOSE:  Could not close command file.",
/* 47 */  "EMTIMEOUT:  Host timed out waiting for a message.",
/* 48 */  "EMCOMMTYPE:  A '-t' flag must be specified.",
/* 49 */  "EMCOMMERR:  Communication error.",
/* 50 */  "EMBAUD:  Invalid baud rate specified.",
/* 51 */  "EMTIPINIT: TIP initialization failed. Exiting TIP.",
/* 52 */  "EMIOSETF: Host I/O setup failure.",
/* 53 */  "EMIORESETF: Host I/O reset failure.",
/* 54 */  "EMLOADF: Loading COFF file failure.",
/* 55 */  "EMNOFILE: No program to run.",
/* 56 */  "EMECHOPEN: Could not open echo file.",
/* 57 */  "EMCTRLC: Ctrl-C interrupt. Exiting.",
/* 58 */  "EMNOSUCHCMD: Unrecognized command.",
/* 59 */  "EMNOPROCESS: Failed creating process zero.",
/* 60 */  "EMNOTCOMP: DFE and TIP versions not compatible.",
/* 61 */  "EMFATAL: No session in progress.",
/* 62 */  "EMNOINITP: (-n) No process initialized for downloaded program.",
/* 63 */  "EMDOSERR: DOS error. Cannot escape to DOS.",
/* 64 */  "EMSYSERR: System error. Cannot escape to host OS.",
/* 65 */  "EMINCECHOFILE: Invalid echo file. Cannot enable echo.",
/* 66 */  "EMCMDFILENEST: Nesting of command files not allowed."
};


