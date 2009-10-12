use 5.006;
use strict;
use warnings;
use ExtUtils::MakeMaker;

unless ($^O eq "MSWin32" || $^O eq "cygwin") {
    die "OS unsupported\n";
}

my %param = (
    NAME          => 'Win32',
    VERSION_FROM  => 'Win32.pm',
    INSTALLDIRS   => ($] >= 5.008004 ? 'perl' : 'site'),
);
$param{NO_META} = 1 if eval "$ExtUtils::MakeMaker::VERSION" >= 6.10_03;
$param{LIBS} = ['-L/lib/w32api -lole32 -lversion'] if $^O eq "cygwin";

WriteMakefile(%param);
