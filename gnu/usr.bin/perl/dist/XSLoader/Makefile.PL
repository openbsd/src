# A template for Makefile.PL.
# - Set the $PACKAGE variable to the name of your module.
# - Set $LAST_API_CHANGE to reflect the last version you changed the API 
#   of your module.
# - Fill in your dependencies in PREREQ_PM
# Alternatively, you can say the hell with this and use h2xs.

use ExtUtils::MakeMaker;
eval 'use ExtUtils::MakeMaker::Coverage';

$PACKAGE = 'XSLoader';
($PACKAGE_FILE = $PACKAGE) =~ s|::|/|g;
$LAST_API_CHANGE = 0;

eval "require $PACKAGE";

unless ($@) { # Make sure we did find the module.
    print <<"CHANGE_WARN" if ${$PACKAGE.'::VERSION'} < $LAST_API_CHANGE;

NOTE: There have been API changes between this version and any older
than version $LAST_API_CHANGE!  Please read the Changes file if you
are upgrading from a version older than $LAST_API_CHANGE.

CHANGE_WARN
}

# In case the empty lib/ directory was not created.
mkdir 'lib', 0755 unless $ENV{PERL_CORE};

# starting with Perl 5.11, "site" and "vendor" directories finally are
# before "perl" (core) in @INC, thus allowing dual-life modules to be
# updated without the need to overwrite the old version
my $installdirs = $] < 5.011 ? "perl" : "site";

WriteMakefile(
    NAME            => $PACKAGE,
    LICENSE         => 'perl',
    AUTHOR          => 'Sebastien Aperghis-Tramoni <sebastien@aperghis.net>',
    VERSION_FROM    => 'XSLoader_pm.PL',
    ABSTRACT_FROM   => 'XSLoader_pm.PL',
    INSTALLDIRS     => $installdirs,
    PL_FILES        => { 'XSLoader_pm.PL'  => 'XSLoader.pm' },
    PM              => { 'XSLoader.pm' => '$(INST_ARCHLIB)/XSLoader.pm' },
    PREREQ_PM       => {
        'Test::More' => '0.47',
    },
    META_MERGE      => {
        resources   => {
            repository  => 'git://perl5.git.perl.org/perl.git',
            license     => 'http://dev.perl.org/licenses/',
            homepage    => 'https://metacpan.org/module/Math::BigInt',
            irc         => 'irc://irc.perl.org/#p5p',
            mailinglist => 'http://lists.perl.org/list/perl5-porters.html',
            bugtracker  => "https://rt.perl.org/rt3/Search/Results.html?Query=Queue='perl5' AND Content LIKE 'module=XSLoader' AND (Status='open' OR Status='new' OR Status='stalled')",
        },
    },
    dist            => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean           => { FILES => 'XSLoader-* XSLoader.pm' },
);

# Unlink the .pm file included with the distribution
1 while unlink "XSLoader.pm";

{
    package MY;

    sub test_via_harness {
        my($self, $orig_perl, $tests) = @_;

        my @perls = ($orig_perl);
        push @perls, qw(bleadperl 
                        perl5.6.1
                        perl5.6.0
                        perl5.005_03 
                        perl5.004_05 
                        perl5.004_04
                        perl5.004)
          if $ENV{PERL_TEST_ALL};

        my $out;
        foreach my $perl (@perls) {
            $out .= $self->SUPER::test_via_harness($perl, $tests);
        }

        return $out;
    }
}
