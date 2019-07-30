/*===------- llvm/Config/llvm-config.h - llvm configuration -------*- C -*-===*/
/*                                                                            */
/*                     The LLVM Compiler Infrastructure                       */
/*                                                                            */
/* This file is distributed under the University of Illinois Open Source      */
/* License. See LICENSE.TXT for details.                                      */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

/* This file enumerates variables from the LLVM configuration so that they
   can be in exported headers and won't override package specific directives.
   This is a C header that can be included in the llvm-c headers. */

#ifndef LLVM_CONFIG_H
#define LLVM_CONFIG_H


/* Define if we link Polly to the tools */
/* #undef LINK_POLLY_INTO_TOOLS */

/* Target triple LLVM will generate code for by default */
/* #define LLVM_DEFAULT_TARGET_TRIPLE "amd64-unknown-openbsd6.0" */

/* Define if threads enabled */
/* #define LLVM_ENABLE_THREADS 1 */

/* Has gcc/MSVC atomic intrinsics */
/* #define LLVM_HAS_ATOMICS 1 */

/* Host triple LLVM will be executed on */
/* #define LLVM_HOST_TRIPLE "amd64-unknown-openbsd6.0" */

/* LLVM architecture name for the native architecture, if available */
/* #define LLVM_NATIVE_ARCH X86 */

/* LLVM name for the native AsmParser init function, if available */
/* #define LLVM_NATIVE_ASMPARSER LLVMInitializeX86AsmParser */

/* LLVM name for the native AsmPrinter init function, if available */
/* #define LLVM_NATIVE_ASMPRINTER LLVMInitializeX86AsmPrinter */

/* LLVM name for the native Disassembler init function, if available */
/* #define LLVM_NATIVE_DISASSEMBLER LLVMInitializeX86Disassembler */

/* LLVM name for the native Target init function, if available */
/* #define LLVM_NATIVE_TARGET LLVMInitializeX86Target */

/* LLVM name for the native TargetInfo init function, if available */
/* #define LLVM_NATIVE_TARGETINFO LLVMInitializeX86TargetInfo */

/* LLVM name for the native target MC init function, if available */
/* #define LLVM_NATIVE_TARGETMC LLVMInitializeX86TargetMC */

/* Define if this is Unixish platform */
#define LLVM_ON_UNIX 1

/* Define if this is Win32ish platform */
/* #undef LLVM_ON_WIN32 */

/* Define if we have the Intel JIT API runtime support library */
#define LLVM_USE_INTEL_JITEVENTS 0

/* Define if we have the oprofile JIT-support library */
#define LLVM_USE_OPROFILE 0

/* Major version of the LLVM API */
#define LLVM_VERSION_MAJOR 5

/* Minor version of the LLVM API */
#define LLVM_VERSION_MINOR 0

/* Patch version of the LLVM API */
#define LLVM_VERSION_PATCH 1

/* LLVM version string */
#define LLVM_VERSION_STRING "5.0.1"

/* Define if we link Polly to the tools */
/* #undef LINK_POLLY_INTO_TOOLS */

#endif
