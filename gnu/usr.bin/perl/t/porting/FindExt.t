#!../miniperl -w

BEGIN {
    @INC = qw(../win32 ../lib);
    require './test.pl';
    skip_all('FindExt not portable')
	if $^O eq 'VMS';
}
use strict;
use Config;

# Test that Win32/FindExt.pm is consistent with Configure in determining the
# types of extensions.
# It's not possible to check the list of built dynamic extensions, as that
# varies based on which headers are present, and which options ./Configure was
# invoked with.

if ($^O eq "MSWin32" && !defined $ENV{PERL_STATIC_EXT}) {
    skip_all "PERL_STATIC_EXT must be set to the list of static extensions";
}

unless (defined $Config{usedl}) {
    skip_all "FindExt just plain broken for static perl.";
}

plan tests => 10;
use FindExt;

FindExt::apply_config(\%Config);
FindExt::scan_ext('../cpan');
FindExt::scan_ext('../dist');
FindExt::scan_ext('../ext');
FindExt::set_static_extensions(split ' ', $ENV{PERL_STATIC_EXT}) if $^O eq "MSWin32";
FindExt::set_static_extensions(split ' ', $Config{static_ext}) unless $^O eq "MSWin32";

# Config.pm and FindExt.pm make different choices about what should be built
my @config_built;
my @found_built;
{
    foreach my $type (qw(static dynamic nonxs)) {
	push @found_built, eval "FindExt::${type}_ext()";
	push @config_built, split ' ', $Config{"${type}_ext"};
    }
}
@config_built = sort @config_built;
@found_built = sort @found_built;

foreach (['static_ext',
	  [FindExt::static_ext()], $Config{static_ext}],
	 ['nonxs_ext',
	  [FindExt::nonxs_ext()], $Config{nonxs_ext}],
	 ['known_extensions',
	  [FindExt::known_extensions()], $Config{known_extensions}],
	 ['"config" dynamic + static + nonxs',
	  \@config_built, $Config{extensions}],
	 ['"found" dynamic + static + nonxs', 
	  \@found_built, join " ", FindExt::extensions()],
	) {
    my ($type, $found, $config) = @$_;
    my @config = sort split ' ', $config;
    is (scalar @$found, scalar @config,
	"We find the same number of $type");
    is ("@$found", "@config", "We find the same list of $type");
}
