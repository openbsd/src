@echo off

chdir libiberty
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\bfd
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\opcodes
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\gprof
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\binutils
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\gas
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..\ld
call configure %1 %2 %3 %4 %5 %6 %7 %8 %9
chdir ..
