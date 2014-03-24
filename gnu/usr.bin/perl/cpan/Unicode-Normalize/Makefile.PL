require 5.006001;
use ExtUtils::MakeMaker;

my $clean = {};

if (-f "Normalize.xs") {
    print STDERR "Making header files for XS...\n";

    do 'mkheader' or die $@ || "mkheader: $!";

    $clean = { FILES => 'unfcan.h unfcmb.h unfcmp.h unfcpt.h unfexc.h' };
}

WriteMakefile(
    'AUTHOR'            => 'SADAHIRO Tomoyuki <SADAHIRO@cpan.org>',
    'ABSTRACT'          => 'Unicode Normalization Forms',
    'INSTALLDIRS'       => ($] >= 5.007002 && $] < 5.011) ? 'perl' : 'site',
                            # see perl5110delta, @INC reorganization
    'LICENSE'           => 'perl',
    'NAME'              => 'Unicode::Normalize',
    'VERSION_FROM'      => 'Normalize.pm', # finds $VERSION
    'clean'             => $clean,
    'PREREQ_PM'         => {
        bytes           => 0,
        Carp            => 0,
        constant        => 0,
        DynaLoader      => 0,
        Exporter        => 0,
        File::Copy      => 0,
        File::Spec      => 0,
        strict          => 0,
        warnings        => 0,
    },
);
