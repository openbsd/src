use ExtUtils::MakeMaker;

my $clean = {};

if (-f "Normalize.xs") {
    print STDERR "Making header files for XS...\n";

    do "mkheader";
    $clean = { FILES => 'unfcan.h unfcmb.h unfcmp.h unfcpt.h unfexc.h' };
}

WriteMakefile(
    'INSTALLDIRS'	=> $] >= 5.007 ? 'perl' : 'site',
    'NAME'		=> 'Unicode::Normalize',
    'VERSION_FROM'	=> 'Normalize.pm', # finds $VERSION
    'clean'		=> $clean,
);
