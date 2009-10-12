#!perl

use Test::More;
BEGIN {
    if ( $ENV{PERL_CORE} ) {
    require Config;
	if ( $Config::Config{extensions} !~ /(?<!\S)Win32CORE(?!\S)/ ) {
	    plan skip_all => "Win32CORE extension not built";
	    exit();
	}
    }

        plan tests => 4;
};
use_ok( "Win32CORE" );

# Make sure that Win32 is not yet loaded
ok(!defined &Win32::ExpandEnvironmentStrings);

# [perl #42925] - Loading Win32::GetLastError() via the forwarder function
# should not affect the last error being retrieved
$^E = 42;
is(Win32::GetLastError(), $^O eq 'cygwin' ? 0 : 42, 'GetLastError() works on the first call');

# Now all Win32::* functions should be loaded
ok(defined &Win32::ExpandEnvironmentStrings);
