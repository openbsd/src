@echo off
chdir libiberty
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\bfd
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\opcodes
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\gprof
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\binutils
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\gas
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\ld
make %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..
