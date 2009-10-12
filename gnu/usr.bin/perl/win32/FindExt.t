#!../miniperl -w

BEGIN {
    @INC = qw(../win32 ../lib);
}
use strict;

use Test::More tests => 10;
use FindExt;
use Config;

FindExt::scan_ext('../ext');

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
    is_deeply($found, \@config, "We find the same");
}
