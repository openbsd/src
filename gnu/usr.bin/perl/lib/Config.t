#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require "./test.pl";
}

plan tests => 36;

use_ok('Config');

# Some (safe?) bets.

ok(keys %Config > 500, "Config has more than 500 entries");

ok(each %Config);

is($Config{PERL_REVISION}, 5, "PERL_REVISION is 5");

# Check that old config variable names are aliased to their new ones.
my %grandfathers = ( PERL_VERSION       => 'PATCHLEVEL',
                     PERL_SUBVERSION    => 'SUBVERSION',
                     PERL_CONFIG_SH     => 'CONFIG'
                   );
while( my($new, $old) = each %grandfathers ) {
    isnt($Config{$new}, undef,       "$new is defined");
    is($Config{$new}, $Config{$old}, "$new is aliased to $old");
}

ok( exists $Config{cc},      "has cc");

ok( exists $Config{ccflags}, "has ccflags");

ok(!exists $Config{python},  "has no python");

ok( exists $Config{d_fork},  "has d_fork");

ok(!exists $Config{d_bork},  "has no d_bork");

like($Config{ivsize},     qr/^(4|8)$/, "ivsize is 4 or 8 (it is $Config{ivsize})");

# byteorder is virtual, but it has rules. 

like($Config{byteorder}, qr/^(1234|4321|12345678|87654321)$/, "byteorder is 1234 or 4321 or 12345678 or 87654321 (it is $Config{byteorder})");

is(length $Config{byteorder}, $Config{ivsize}, "byteorder is as long as ivsize (which is $Config{ivsize})");

# ccflags_nolargefiles is virtual, too.

ok(exists $Config{ccflags_nolargefiles}, "has ccflags_nolargefiles");

# Utility functions.

{
    # make sure we can export what we say we can export.
    package Foo;
    my @exports = qw(myconfig config_sh config_vars config_re);
    Config->import(@exports);
    foreach my $func (@exports) {
	::ok( __PACKAGE__->can($func), "$func exported" );
    }
}

like(Config::myconfig(),       qr/osname=\Q$Config{osname}\E/,   "myconfig");
like(Config::config_sh(),      qr/osname='\Q$Config{osname}\E'/, "config_sh");
like(join("\n", Config::config_re('c.*')),
			       qr/^c.*?=/,                   'config_re' );

my $out = tie *STDOUT, 'FakeOut';

Config::config_vars('cc');
my $out1 = $$out;
$out->clear;

Config::config_vars('d_bork');
my $out2 = $$out;
$out->clear;

untie *STDOUT;

like($out1, qr/^cc='\Q$Config{cc}\E';/, "config_vars cc");
like($out2, qr/^d_bork='UNKNOWN';/, "config_vars d_bork is UNKNOWN");

# Read-only.

undef $@;
eval { $Config{d_bork} = 'borkbork' };
like($@, qr/Config is read-only/, "no STORE");

ok(!exists $Config{d_bork}, "still no d_bork");

undef $@;
eval { delete $Config{d_fork} };
like($@, qr/Config is read-only/, "no DELETE");

ok( exists $Config{d_fork}, "still d_fork");

undef $@;
eval { %Config = () };
like($@, qr/Config is read-only/, "no CLEAR");

ok( exists $Config{d_fork}, "still d_fork");

{
    package FakeOut;

    sub TIEHANDLE {
	bless(\(my $text), $_[0]);
    }

    sub clear {
	${ $_[0] } = '';
    }

    sub PRINT {
	my $self = shift;
	$$self .= join('', @_);
    }
}

# Signal-related variables
# (this is actually a regression test for Configure.)

is($Config{sig_num_init}  =~ tr/,/,/, $Config{sig_size}, "sig_num_init size");
is($Config{sig_name_init} =~ tr/,/,/, $Config{sig_size}, "sig_name_init size");
