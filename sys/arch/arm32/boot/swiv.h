/* SWI veneers:
 *  Written by Edward Nevill and Jonathan Roach in an idle moment between projects.
 *  Hacked by BDB to add flag returning 
 */

/* Anonymous Error type */

struct Error { int num; char msg[4]; };

typedef struct Error Error;

/* Generic SWI interface
 *  swi(swino,mask,regs...)
 *   swino = SWI number to call as defined in h.swis, X bit set if you wish the
 *           X form of the SWI to be called, clear if you want the non X form.
 *   reg_mask = mask of in / out registers
 *              bits 0-9:   Bit N set => Register N specified on input
 *                          Bit N clear => Register N unspecified on input
 *              bits 22-31: Bit N set => Register N-22 on output stored
 *                              in address specified in varargs list.
 *   ...        In order, input registers followed by output registers,
 *                      starting at r0 and going up.
 *   returns 0 or errorblock pointer if X-bit set
 *   returns r0 if X-bit clear
 *  swix(swino,mask,regs...)
 *   This behaves identically to 'swi' except that it always calls the X form.
 *
 * Eg:
 *   swi(OS_SWINumberToString, IN(R0|R1|R2), n, buf, 255);
 *   e = swi(XOS_SWINumberFromString, IN(R1)|OUT(R0), str, &n);
 *       - Error block pointer (or 0) is returned so must get returned R0
 *       - via argument list.
 *   e = swix(OS_SWINumberFromString, IN(R1)|OUT(R0), str, &n);
 *       - As above but uses the swix function rather that setting the X bit
 *         explicitly (saves one instruction on SWI call).
 *   e = swi(OS_File, IN(R0|R1|R2|R3)|OUT(R4), 255, name, buff, 0, &len);
 *       - We don't care about the load, exec or attrs so don't specify
 *         them in the output registers.
 */

extern Error *swix(int swino, int reg_mask, ...);
extern int swi(int swino, int reg_mask, ...);

/* Register mask macros
 *  The bits in the register mask are arranged as follows:
 *  31 30 29 ... 22 ...  8 ...  2  1  0
 *  O0 O1 O2 ... O9     I9 ... I2 I1 I0  I(N) = bit set if R(N) used on entry
 *                                       O(N) = bit set if R(N) written on exit
 *  The bits are arranged like this to optimise the case where a SWI is being
 *  called with a small number of input and output registers. For example, a SWI
 *  call which uses R0-R5 on entry and R0-R1 on exit will have a register mask
 *  of 0xC000003f which can be loaded into an ARM register in one instruction
 *  (the compiler performs this optimisation, even when the constant wraps
 *  around between bits 0 and 31). Using the more obvious coding of I0-I9 in bits
 *  0 - 9 and O0-O9 in bits 16-23 leads to a constant of 0x0003003f which require
 *  two instructions.
 */
#define IN(m) (m)
/* old, incorrect version
#define OUT(m) ((unsigned)(m&1)<<31|(m&2)<<29|(m&4)<<27|(m&8)<<25|(m&16)<<23|\
                (m&32)<<21|(m&64)<<19|(m&128)<<17|(m&256)<<15|(m&512)<<13)
*/
#define OUT(m) ((unsigned)((m)&1)<<31|((m)&2)<<29|((m)&4)<<27|\
                ((m)&8)<<25|((m)&16)<<23|((m)&32)<<21|((m)&64)<<19|\
                ((m)&128)<<17|((m)&256)<<15|((m)&512)<<13|((m)&1024)<<11)

/* The register names
 *  Change these to use different names if you use R0 - R9 elsewhere in your program
 */
#define PSW 0x400       /* Use only in OUT, orders BEFORE others */
#define R0 0x001
#define R1 0x002
#define R2 0x004
#define R3 0x008
#define R4 0x010
#define R5 0x020
#define R6 0x040
#define R7 0x080
#define R8 0x100
#define R9 0x200
