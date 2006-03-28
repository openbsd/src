require 5.006001;
use ExtUtils::MakeMaker;

my $clean = {};

if (-f "Normalize.xs") {
    print STDERR "Making header files for XS...\n";

    do 'mkheader' or die $@ || "mkheader: $!";

    $clean = { FILES => 'unfcan.h unfcmb.h unfcmp.h unfcpt.h unfexc.h' };
}

WriteMakefile(
    'INSTALLDIRS'	=> $] >= 5.007002 ? 'perl' : 'site',
    'NAME'		=> 'Unicode::Normalize',
    'VERSION_FROM'	=> 'Normalize.pm', # finds $VERSION
    'clean'		=> $clean,
    'PREREQ_PM'	  	=> {
	Carp		=> 0,
	constant	=> 0,
	DynaLoader	=> 0,
	Exporter	=> 0,
	File::Copy	=> 0,
	File::Spec	=> 0,
	strict		=> 0,
	Test		=> 0,
	warnings	=> 0,
    },
);
