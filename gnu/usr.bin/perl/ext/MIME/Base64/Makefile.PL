require 5.006;
use ExtUtils::MakeMaker;

my @makefileopts;
if (grep { $_ eq 'PERL_CORE=1' } @ARGV) {
    push @makefileopts, MAN3PODS => {};
}
if ($] >= 5.008) {
    push @makefileopts, INSTALLDIRS => 'perl';
}

WriteMakefile(
    NAME	 => 'MIME::Base64',
    VERSION_FROM => 'Base64.pm',
    dist         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    @makefileopts,
);
