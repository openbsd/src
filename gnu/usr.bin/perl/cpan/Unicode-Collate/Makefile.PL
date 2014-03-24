require 5.006001;
use ExtUtils::MakeMaker;

my $clean = {};

if (-f "Collate.xs") {
    print STDERR "Making header files for XS...\n";

    do 'mkheader' or die $@ || "mkheader: $!";

    $clean = { FILES => 'ucatbl.h' };
}

WriteMakefile(
    'AUTHOR'            => 'SADAHIRO Tomoyuki <SADAHIRO@cpan.org>',
    'ABSTRACT'          => 'Unicode Collation Algorithm',
    'INSTALLDIRS'       => ($] >= 5.007002 && $] < 5.011) ? 'perl' : 'site',
                            # see perl5110delta, @INC reorganization
    'LICENSE'           => 'perl',
    'NAME'              => 'Unicode::Collate',
    'VERSION_FROM'      => 'Collate.pm', # finds $VERSION
    'clean'             => $clean,
    'PREREQ_PM'         => {
        Carp            => 0,
        constant        => 0,
        DynaLoader      => 0,
        File::Spec      => 0,
        strict          => 0,
        warnings        => 0,
    },
);
