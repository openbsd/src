# Time-stamp: <01/08/01 21:03:25 keuchel@w2k>
#
# rebuild all perl targets

machines="
wince-arm-hpc-wce300
wince-arm-hpc-wce211
wince-sh3-hpc-wce211
wince-mips-hpc-wce211
wince-mips-hpc-wce200
wince-sh3-hpc-wce200
wince-arm-pocket-wce300
wince-mips-pocket-wce300
wince-sh3-pocket-wce300
wince-x86em-pocket-wce300
wince-sh3-palm-wce211
wince-mips-palm-wce211
wince-x86em-palm-wce211
"

for mach in $machines; do
  mflags="-f makefile.ce MACHINE=$mach"
  echo "Calling nmake for $mach..."
  nmake $mflags clean
  nmake $mflags all
  nmake $mflags all dlls || exit 1
  nmake $mflags makedist
done

