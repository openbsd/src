require 5.006;
use ExtUtils::MakeMaker;

my @makefileopts;
if ($] >= 5.008) {
    push @makefileopts, INSTALLDIRS => 'perl';
}

WriteMakefile(
    NAME	 => 'MIME::Base64',
    VERSION_FROM => 'Base64.pm',
    @makefileopts,

    ABSTRACT     => 'The RFC 2045 encodings; base64 and quoted-printable',
    AUTHOR       => 'Gisle Aas <gisle@activestate.com>',
    LICENSE      => 'perl',
    MIN_PERL_VERSION => 5.006,
    META_MERGE   => {
        resources  => {
            repository => 'http://github.com/gisle/mime-base64',
        }
    },
);

BEGIN {
    # compatibility with older versions of MakeMaker
    my $developer = -d ".git";
    my %mm_req = (
        LICENCE => 6.31,
        META_MERGE => 6.45,
        META_ADD => 6.45,
        MIN_PERL_VERSION => 6.48,
    );
    undef(*WriteMakefile);
    *WriteMakefile = sub {
        my %arg = @_;
        for (keys %mm_req) {
            unless (eval { ExtUtils::MakeMaker->VERSION($mm_req{$_}) }) {
                warn "$_ $@" if $developer;
                delete $arg{$_};
            }
        }
        ExtUtils::MakeMaker::WriteMakefile(%arg);
    };
}
