# A template for Makefile.PL.
# - Set the $PACKAGE variable to the name of your module.
# - Set $LAST_API_CHANGE to reflect the last version you changed the API 
#   of your module.
# - Fill in your dependencies in PREREQ_PM
# Alternatively, you can say the hell with this and use h2xs.

use ExtUtils::MakeMaker;
use ExtUtils::MM_Unix;
eval 'use ExtUtils::MakeMaker::Coverage';

$PACKAGE = 'XSLoader';
($PACKAGE_FILE = $PACKAGE) =~ s|::|/|g;
$LAST_API_CHANGE = 0;

$CURRENT_VERSION = ${$PACKAGE.'::VERSION'};
$NEW_VERSION     = ExtUtils::MM_Unix->parse_version("XSLoader_pm.PL");

eval "require $PACKAGE";

unless ($@) { # Make sure we did find the module.
    print <<"CHANGE_WARN" if $CURRENT_VERSION < $LAST_API_CHANGE;

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
        # NOTE: If we should require a Test::More version higher than 0.98
        # (that included with perl 5.14), we need to remove the meta-spec
        # entry below for EUMM 6.57_02 to 6.57_06 (the buggy versions
        # included with perl 5.14).  Otherwise installation will break.
        # See https://github.com/Perl-Toolchain-Gang/CPAN-Meta/issues/118
        # for details.
        'Test::More' => '0.47',
    },
    META_MERGE      => {
        'meta-spec'     => { version => 2 },
        dynamic_config  => 0,
        resources       => {
            repository  => {
                type        => 'git',
                url         => 'git://perl5.git.perl.org/perl.git',
            },
            homepage    => 'https://metacpan.org/module/XSLoader',
            x_IRC       => 'irc://irc.perl.org/#p5p',
            x_MailingList => 'http://lists.perl.org/list/perl5-porters.html',
            bugtracker => {
                mailto      => 'perlbug@perl.org',
                web         => "https://rt.perl.org/rt3/Search/Results.html?Query=Queue='perl5' AND Content LIKE 'module=XSLoader' AND (Status='open' OR Status='new' OR Status='stalled')",
            },
        },
        provides    => {
            'XSLoader'  => {
                file        => 'XSLoader_pm.PL',
                version     => $NEW_VERSION,
            },
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
