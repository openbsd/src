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
*       NAME	@(#)tip_test.c	1.1 91/07/24  Daniel Mann
* 
*	This module is used for testing of TIP services.
********************************************************************** HISTORY
*/
#include <stdio.h>
#include "udiproc.h"

UDIError UDIConnect() {return;}
UDIError UDIDisconnect() {return;}
UDIError UDISetCurrentConnection() {return;}
UDIError UDICapabilities() {return;}
UDIError UDIEnumerateTIPs() {return;}
UDIError UDIGetErrorMsg() {return;}
UDIError UDIGetTargetConfig() {return;}
UDIError UDICreateProcess() {return;}
UDIError UDISetCurrentProcess() {return;}
UDIError UDIDestroyProcess() {return;}
UDIError UDIInitializeProcess() {return;}
UDIError UDIRead() {return;}
UDIError UDIWrite() {return;}
UDIError UDICopy() {return;}
UDIError UDIExecute() {return;}
UDIError UDIStep() {return;}
UDIError UDIStop() {return;}
UDIError UDIWait() {return;}
UDIError UDISetBreakpoint() {return;}
UDIError UDIQueryBreakpoint() {return;}
UDIError UDIClearBreakpoint() {return;}
UDIError UDIGetStdout() {return;}
UDIError UDIGetStderr() {return;}
UDIError UDIPutStdin() {return;}
UDIError UDIStdinMode() {return;}
UDIError UDIPutTrans() {return;}
UDIError UDIGetTrans() {return;}
UDIError UDITransMode() {return;}
