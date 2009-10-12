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
);
