
                          Installing GCC: Building

   Now that GCC is configured, you are ready to build the compiler and
   runtime libraries.

   We _highly_ recommend that GCC be built using GNU make; other versions
   may work, then again they might not.

   (For example, many broken versions of make will fail if you use the
   recommended setup where _objdir_ is different from _srcdir_.)

Building a native compiler

   For a native build issue the command `make bootstrap'. This will build
   the entire GCC system, which includes the following steps:
     * Build host tools necessary to build the compiler such as texinfo,
       bison, gperf.
     * Build target tools for use by the compiler such as binutils (bfd,
       binutils, gas, gprof, ld, and opcodes)
       if they have been individually linked or moved into the top level
       GCC source tree before configuring.
     * Perform a 3-stage bootstrap of the compiler.
     * Perform a comparison test of the stage2 and stage3 compilers.
     * Build runtime libraries using the stage3 compiler from the
       previous step.

   If you are short on disk space you might consider `make
   bootstrap-lean' instead. This is identical to `make bootstrap' except
   that object files from the stage1 and stage2 of the 3-stage bootstrap
   of the compiler are deleted as soon as they are no longer needed.

   If you want to save additional space during the bootstrap and in the
   final installation as well, you can build the compiler binaries
   without debugging information with "make CFLAGS='-O' LIBCFLAGS='-g
   -O2' LIBCXXFLAGS='-g -O2 -fno-implicit-templates' bootstrap". This
   will save roughly 40% of disk space both for the bootstrap and the
   final installation. (Libraries will still contain debugging
   information.)

   If you used the flag --enable-languages=... to restrict the compilers
   to be built, only those you've actually enabled will be built. This
   will of course only build those runtime libraries, for which the
   particular compiler has been built. Please note, that re-defining
   LANGUAGES when calling `make bootstrap' _*does not*_ work anymore!

Building a cross compiler

   We recommend reading the [1]crossgcc FAQ for information about
   building cross compilers.

   When building a cross compiler, it is not generally possible to do a
   3-stage bootstrap of the compiler. This makes for an interesting
   problem as parts of GCC can only be built with GCC.

   To build a cross compiler, we first recommend building and installing
   a native compiler. You can then use the native GCC compiler to build
   the cross compiler.

   Assuming you have already installed a native copy of GCC and
   configured your cross compiler, issue the command "make", which
   performs the following steps:
     * Build host tools necessary to build the compiler such as texinfo,
       bison, gperf.
     * Build target tools for use by the compiler such as binutils (bfd,
       binutils, gas, gprof, ld, and opcodes)
       if they have been individually linked or moved into the top level
       GCC source tree before configuring.
     * Build the compiler (single stage only).
     * Build runtime libraries using the compiler from the previous step.

   Note that if an error occurs in any step the make process will exit.

Building in parallel

   If you have a multiprocessor system you can use `make bootstrap
   MAKE="make -j 2" -j 2' or just `make -j 2 bootstrap' for GNU Make 3.79
   and above instead of just `make bootstrap' when building GCC. You can
   use a bigger number instead of two if you like. In most cases, it
   won't help to use a number bigger than the number of processors in
   your machine.
     _________________________________________________________________

   [2]Return to the GCC Installation page

References

   1. http://www.objsw.com/CrossGCC/
   2. http://gcc.gnu.org/install/index.html
