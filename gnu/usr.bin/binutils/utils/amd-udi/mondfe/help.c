static char _[] = "@(#)help.c	5.22 93/08/23 15:30:33, Srini, AMD.";
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
 **       This file contains the help screens for the monitor.
 *****************************************************************************
 */

/*
** Main help
*/

char *help_main[] = {

"Use 'h <letter>' for individual command help",
" ",
" --------------------- MONDFE Monitor Commands -----------------------------",
" a - Assemble Instruction        | b,b050,bc - Set/Clear/Display Breakpoint",
" c - Print Configuration         | caps - DFE and TIP Capabilities",
" cp - Create UDI Process         | con - Connect to a UDI Debug Session",
" ch0 - 29K Terminal Control      | d,dw,dh,db,df,dd - Dump Memory/Registers",
" dp - Destroy UDI Process        | disc - Temporarily Disconnect UDI Session",
" ex - Exit UDI Session           | esc - Escape to Host Operating System",
" eon - Turn Echo Mode ON         | eoff - Turn Echo Mode OFF",
" g - Start/Resume Execution      | f,fw,fh,ff,fd,fs - Fill Memory/Registers",
" h - Help Command                | init - Initialize Current UDI Process",
" ix,il - Display Am2903X Cache   | k - Kill Running Program on 29K Target",
" logon - Turn ON log mode        | logoff - Turn OFF log mode",
" l - List/Disassemble Memory     | m - Move Data to Memory/Registers",
" pid - Set UDI Process ID        | q - Quit mondfe",
" qon - Turn Quiet Mode ON        | qoff - Turn Quiet Mode OFF",
" sid - Set UDI Session ID        | r - Reset (software reset) 29K Target",
" t - Trace/Single Step Execution | s,sw,sh,sb,sf,sd - Set Memory/Registers",
" ver - Montip Version Command    | tip - Montip Transparent Mode Command",
" y - Yank/Download COFF File     | xp - Display Protected Special Registers",
" ze - Echo File For Echo Mode    | zc - Execute commands from command file",
" zl - Use log file for log mode  | | - Comment character (in Command File)",
" ----------------------------------------------------------------------------",
""
};


/*
** Assemble
*/

char *help_a[] = {

"A <address> <instruction>",
" ",
"Assemble instructions into memory.",
" ",
"The address, is the memory address for the instruction.",
" ",
"The instruction will be assembled and placed in memory at the",
"specified address.",
" ",
"Memory addresses:",
" ",
"<hex>m - data memory             <hex>i - instruction memory",
"<hex>r - rom memory              <hex>u - unspecified (no addr check)",
""
};


/*
** Breakpoint
*/

char *help_b[] = {

"Breakpoint display, set and clear.",
" ",
"B - displays valid breakpoints",
"B <address> [<passcount>]  - to set  software breakpoint",
"B050[P,V] <address> [<passcount>]  - to set Am29050 hardware breakpoint",
" When B050P is used, breakpoint is hit only if translation is disabled.",
" When B050V is used, breakpoint is hit only if translation is enables.",
"A breakpoint is set at the specified address.  An optional",
"<pass count> sets the pass count.  The B050 command sets",
"a breakpoint using an Am29050 breakpoint register. ",
"BC <address> - to clear the breakpoint set at <address>",
"BC - clears all breakpoints.",
" ",
"<address> format:",
" ",
"<hex>m - data memory              <hex>i - instruction memory",
"<hex>r - rom memory               <hex>u - unspecified (no addr check)",
" B Command usage: B, B050, B050V, B050P",
""
};


/*
** Configuration help
*/

char *help_c[] = {

"C - Prints target system configuration.",
" ",
"This command is used to read and display the target configuration.",
"A banner is printed displaying information about the target.",
" ",
" Other C commands: CAPS, CP, CON, CH0",
""
};

char *help_caps[] = {
" CAPS - Prints UDI capabilities of DFE and TIP",
" This prints the DFE version number, TIP version number, and UDI revision.",
""
};

char *help_cp[] = {
"CP - Create a UDI Process.",
" This sends a request to the TIP to create a new process.",
""
};

char *help_con[] = {
"CON <session_id>- Requests connection to UDI TIP running <session_id>.",
" This connects to the debug session specified by <session_id>.",
""
};

char *help_ch0[] = {
" CH0 - Transfers control of the terminal to the 29K target.",
" This is used to transfer control to the 29K target program.",
" The input characters typed are sent to the TIP without interpreting",
" for a mondfe command. Control is transferred to mondfe when a Ctrl-U",
" is typed.",
""
};
/*
** Dump
*/

char *help_d[] = {

"D[W|H|B|F|D] [<from_address> [<to_address>]]",
" ",
"Display memory or register contents.",
" ",
"DW or D - display as words.           DF - display in floating point.",
"DH      - display as half-words.      DD - display in double precision",
"DB      - display as bytes.                floating point.",
" ",
"<from_address> defaults to address last displayed.  The ",
"<to_address> is the address of the last data to display.  The default",
"is about eight lines of data.",
" ",
"Valid register names:",
"gr0-gr1, gr64-gr127 - global register names",
"sr0-sr14, sr128-sr135, sr160-sr162,sr164 - special register names",
"lr0-lr127 - local register names ",
"tr0-tr127 - TLB register names ",
"<address> format:",
" ",
"<hex>m - data memory                  <hex>i - instruction memory",
"<hex>r - rom memory                   <hex>u - unspecified (no addr check)",
" D Command usage: D, DW, DH, DB, DF, DD",
" Other D Commands: DP, DISC",
""
};


char *help_dp[] = {
" DP - Destroy process.",
" This requests the TIP to destroy a UDI process. ",
""
};

char *help_disc[] = {
" DISC - Disconnect from the debug session.",
" This disconnects the DFE from the current debug session. The TIP is",
" not destroyed and left running for later reconnections.",
""
};
/*
 * Escape command
 */

char	*help_e[] = {
"ESC",
" ",
"Temporarily exit to host operating system.",
"Use EXIT command to resume debug session.",
"Other E commands: EON, EOFF",
""
};

char	*help_ex[] = {
" EX - Exit current debug session.",
" This command can be used to exit from a debug session when done. Mondfe",
" looks for another session in progress and connects to that session. If",
" there are no more debug sessions in progress, this command causes Mondfe",
" to quit, i.e. it has the same effect as the Quit command",
""
};

char	*help_esc[] = {
"ESC",
" ",
"Temporarily exit to host operating system.",
"Use EXIT command to resume debug session.",
"Other E commands: EON, EOFF",
""
};

char *help_eon[] = {
" EON and EOFF can be used to turn echo mode ON and OFF during the",
" interactive debug session. Echo mode is specified by using the -e ",
" mondfe command line option and an file name. During echo mode, everything",
" displayed on the screen are captured in the file specified.",
""
};

/*
** Fill
*/

char *help_f[] = {

"F[W|H|B|F|D] <start address>, <end address>, <value>",
" ",
"Fill memory or register contents.",
" ",
"FW or F - fill as 32-bit integers  |    FF - fill as floating point value.",
"FH      - fill as 16-bit integers  |    FD - fill as double precision",
"FB      - fill as 8-bit integers   |    floating point value.",
"FS      - fill with the string/pattern given.",
" ",
"Valid register names:",
"gr0-gr1, gr64-gr127 - global register names",
"sr0-sr14, sr128-sr135, sr160-sr162,sr164 - special register names",
"lr0-lr127 - local register names ",
"tr0-tr127 - TLB register names ",
" ",
"<address> format:",
" ",
"<hex>m - data memory               <hex>i - instruction memory",
"<hex>r - rom memory                <hex>u - unspecified (no addr check)",
" F command usage: F, FW, FH, FB, FD, FS",
""
};


/*
** Go
*/

char *help_g[] = {

"G - Start program execution",
" ",
"This resumes program execution at the next instruction.",
" The program runs either until completion or until it hits a breakpoint",
" It is used to run the downloaded program and to resume after hitting",
" a breakpoint. The trace command can be used to execute a specified",
" number of instructions.",
""
};


/*
** I    (ix, ia, il)
*/

char *help_i[] = {
"IX, IL -  Display/Disassemble Am2903X cache registers",
" ",
"Display/Disassemble 2903x cache registers by bit field name.",
" I Commands: IX, IL ",
" Other I commands: INIT",
""
};

char *help_init[] = {
" INIT - Initialize the current process.",
" This is used to initialize the downloaded program to restart execution",
" or to reset the target. It resets the target when the current process",
" ID is set to -1. It does not clear BSS of the downloaded program for ",
" restart.",
""
};

/*
** Help
*/

char *help_h[] = {

"H <cmd>",
" ",
"Get help for a monitor command",
" ",
"This gets help for a particular monitor command.  If <cmd>.",
"is not a valid monitor command the main help screen is listed.",
" Type  <command_name>  for help on a particular command.",
""
};


/*
** Kill
*/

char *help_k[] = {

"K - Kill command.",
" When a K command is issued, the running program on the 29K target",
" is stopped.",
""
};


/*
** List (disassemble)
*/

char *help_l[] = {

"L [<first_address> [<last_address>]]",
" ",
"Disassemble instructions from memory.",
" ",
"The <first_address,> if specified, is the memory address for the first",
"instruction.  If no <first_address> is specified, disassembly will begin",
"from the address in the buffer.",
" ",
"The <last_address,> if specified, is the last address to be disassembled.",
"If no <last_address> is specified, the number of lines of data in the",
"previous disassemble command will be displayed.",
" ",
"<address> format:",
" ",
"<hex>m - data memory              <hex>i - instruction memory",
"<hex>r - rom memory               <hex>u - unspecified (no addr check)",
" Other L commands: logon, logoff",
""
};

char	*help_logon[] = {
" LOGON and LOGOFF commands can be used to turn ON or OFF the log mode",
" from the mondfe command prompt. WHen log mode is on, every command entered",
" by the user is logged into the log file specified at invocation or using",
" the ZL command. When log mode is off, the commands are not logged.",
""
};

/*
** Move
*/

char *help_m[] = {

"M <source start> <source end> <destination start>",
" ",
"Move within memory or registers.  Destination will contain exact",
"copy of original source regardless of overlap.  (The source",
"will be partially altered in the case of overlap.)",
" ",
"Valid register names:",
"gr0-gr1, gr64-gr127 - global register names",
"sr0-sr14, sr128-sr135, sr160-sr162,sr164 - special register names",
"lr0-lr127 - local register names ",
"tr0-tr127 - TLB register names ",
" ",
"<address> format :",
" ",
"<hex>m - data memory               <hex>i - instruction memory",
"<hex>r - rom memory                <hex>u - unspecified (no addr check)",
""
};

char	*help_pid[] = {
" PID <pid_number> - sets the current UDI process to the <pid_number>",
" specified.",
" A <pid_number> of -1 is used to represent the bare machine. This is",
" is used to access physical addresses, and to reset the target.",
" Use CP command to create process. Use DP command to destroy process.",
" Use INIT command to initialize process.",
""
};

/*
** Quit
*/

char *help_q[] = {

"Q",
" ",
"Quit - exit from the monitor.",
""
};

char	*help_qoff[] = {
" QON and QOFF can be used to turn ON/OFF quiet mode of Mondfe. The -q",
" command line option of mondfe can be used to invoke mondfe in quiet",
" mode. In quiet mode, the debug messages are suppressed. These messages",
" can be turned on anytime during the debug session using the QON command",
" and turned off using the QOFF command.",
""
};


/*
** Reset
*/

char *help_r[] = {

"R - Reset the target.",
" This command resets (performs a software reset) of the target. This is",
" equivalent to setting the UDI process ID to -1, and initializing the",
" process using INIT.",
""
};


/*
** Set
*/

char *help_s[] = {

"S[W|H|B|F|D] <address> <data>",
" ",
"Set memory or register contents.",
" ",
"SW or S - set as words.            SF - set in floating point.",
"SH      - set as half-words.       SD - set in double precision",
"SB      - set as bytes.                 floating point.",
" ",
"<address> indicates location to be set.  <Data> is the value",
"to be set.  The data is entered in hexadecimal.",
" ",
"Valid register names:",
"gr0-gr1, gr64-gr127 - global register names",
"sr0-sr14, sr128-sr135, sr160-sr162,sr164 - special register names",
"lr0-lr127 - local register names ",
"tr0-tr127 - TLB register names ",
" ",
"<address> format:",
" ",
"<hex>m - data memory               <hex>i - instruction memory",
"<hex>r - rom memory                <hex>u - unspecified (no addr check)",
" S command usage: S, SW, SH, SB, SF, SD",
" Other S command: SID",
""
};

char	*help_sid[] = {
" SID <sid_number> - sets the UDI session ID to <sid_number>.",
" This command can be used to set the current debug session when there",
" is multiple debug sessions going on.",
""
};

/*
** Trace
*/

char *help_t[] = {

"T <count> - Trace or Step <count> instructions.",
"Trace allows stepping through code.  The optional <count>",
"allows multiple steps to be taken.  The count is in hex.",
" The default value of <count> is 1. This may not step into",
" trap handlers based on the target/TIP capabilities.", 
" Other T commands: TIP",
""
};

char	*help_tip[] = {
" TIP <montip_command> - sends <montip_command> string to montip for execution",
"  The TIP command can be used to inform Montip to change some of its",
"  parameters. The TIP command uses the UDI Transparent mode to pass",
"  the command string. The following TIP commands are now supported:",
"    tip  lpt=0",
"       - requests Montip is stop using the parallel port for communicating",
"         to the 29K target - valid for 29K microcontroller targets.",
"    tip  lpt=1",
"       - requests Montip to use the parallel port for communicating",
"         to the 29K target - valid for 29K microcontroller targets.",
"  The TIP command can be used before issuing a Y(ank) command to download",
"  a program (COFF) file using the PC parallel port. The parallel port",
"  download capability is only applicable for a PC host. The parallel port",
"  to use MUST be specified as a Montip command line option in the UDI ",
"  configuration file - udiconfs.txt on PC, udi_soc on Unix hosts - using",
"  the -par Montip command line option.",
"  As the parallel port communication is only unidirectional, the serial",
"  communications port - com1, or com2 - must also be specified on Montip",
"  command line in the UDI configuration file.",
"  This command is valid ONLY with MiniMON29K Montip.",
""
};

/*
** X
*/

char *help_x[] = {
"XP - Display protected special purpose registers.",
" ",
"Display protected special purpose registers by bit field name.",
""
};


/*
** Yank
*/

char *help_y[] = {

"Y [-t|d|l|b] [-noi|-i] [-ms <mstk_x>] [-rs <rstk_x] [fname] [arglist]",
" ",
"This is the Yank command to download program (COFF) file to the 29K target.",
" ",
"where <fname> is name of a COFF file.",
" ",
"<arglist> is the list of command line arguments for the program.",
" ",
"-t|d|l|b| gives sections for loading. t->text, d->data, l->lit, b->bss.",
" ",
"-noi -> no process created, -i -> download for execute (default).",
" ",
"-ms <memstk_hex> -> memory stack size, -rs <regstk_hex> -> reg stack size.",
" ",
"Ex: y -db hello.pcb arg1 arg2, loads only the DATA and LIT sections.",
" ",
"Simply typing Y will use args from the previous Y issued.",
" ",
" See the TIP command for downloading using parallel port",
""
};


char	*help_zc[] = {
" ZC <cmdfile_name> - execute commands from the <cmdfile_name> command file",
" The ZC command can be used to execute a series of Mondfe commands",
" out of a command file. The <cmdfile_name> is the name of the file",
" containing the command input. This command can be executed at the",
" mondfe> prompt. When all the commands from the file are executed, the",
" mondfe> prompt appears again.",
" Nesting of command files is not allowed.",
" ",
" Other Z commands: ZE, ZL",
""
};

char	*help_ze[] = {
" ZE <echofile_name> - turns ECHO mode ON and specifies the echo file",
" When echo mode is on, everything that is displayed on the screen is ",
" also written into a file, the echo file. The <echofile_name> string ",
" specifies the file name of the echo file to use.",
""
};

char 	*help_zl[] = {
" ZL <logfile_name> - turns LOG mode ON and specifies the log file to use",
" When log mode is on, every mondfe command entered by the user is logged",
" in the log file. The log file thus created can be directly used an an",
" input command file for subsequent debug session to repeat the same sequence",
" of commands. Log mode can be turned on or off using logon or logoff command",
""
};
