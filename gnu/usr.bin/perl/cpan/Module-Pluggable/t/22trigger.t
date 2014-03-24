#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 7;

my $foo;
my @plugins;
my @errors;
ok($foo = TriggerTest->new(), "Created new TriggerTest");
ok(@plugins = $foo->plugins,  "Ran plugins");
ok(@errors  = $foo->errors,   "Got errors");
is_deeply([sort @plugins], ['TriggerTest::Plugin::After', 'TriggerTest::Plugin::CallbackAllow'], "Got the correct plugins");
is_deeply([@errors], ['TriggerTest::Plugin::Error'], "Got the correct errors");
ok(_is_loaded('TriggerTest::Plugin::CallbackDeny'), "CallbackDeny has been required");
ok(!_is_loaded('TriggerTest::Plugin::Deny'), "Deny has not been required");


# Stolen from Module::Loaded by Chris Williams (bingOs)
sub _is_loaded {
    my $pm      = shift;
    my $file    = __PACKAGE__->_pm_to_file( $pm ) or return;
    return $INC{$file} if exists $INC{$file};
    return;
}

sub _pm_to_file {
    my $pkg = shift;
    my $pm  = shift or return;
    my $file = join '/', split '::', $pm;
    $file .= '.pm';
    return $file;
}

package TriggerTest;

our @ERRORS;
use strict;
use Module::Pluggable require          => 1,
                      on_require_error => sub { my $p = shift; push @ERRORS, $p; return 0 },
                      before_require   => sub { my $p = shift; return !($p eq "TriggerTest::Plugin::Deny") },
                      after_require    => sub { my $p = shift; return !($p->can('exclude') && $p->exclude) };

sub new {
    my $class = shift;
    return bless {}, $class;
}

sub errors {
    @ERRORS;
}
1;

