#! /usr/local/perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test::More qw/no_plan/;

BEGIN {
    (my $coretests = $0) =~ s'[^/]+\.t'coretests.pm';
    require $coretests;
    use_ok('version', 0.9902);
}

diag "Tests with base class" unless $ENV{PERL_CORE};

BaseTests("version","new","qv");
BaseTests("version","new","declare");
BaseTests("version","parse", "qv");
BaseTests("version","parse", "declare");

# dummy up a redundant call to satify David Wheeler
local $SIG{__WARN__} = sub { die $_[0] };
eval 'use version;';
unlike ($@, qr/^Subroutine main::declare redefined/,
    "Only export declare once per package (to prevent redefined warnings).");

# https://rt.cpan.org/Ticket/Display.html?id=47980
my $v = eval {
    require IO::Handle;
    $@ = qq(Can't locate some/completely/fictitious/module.pm);
    return IO::Handle->VERSION;
};
ok defined($v), 'Fix for RT #47980';

{ # https://rt.cpan.org/Ticket/Display.html?id=81085
    eval { version::new() };
    like $@, qr'Usage: version::new\(class, version\)',
	'No bus err when called as function';
    eval { $x = 1; print version::new };
    like $@, qr'Usage: version::new\(class, version\)',
	'No implicit object creation when called as function';
    eval { $x = "version"; print version::new };
    like $@, qr'Usage: version::new\(class, version\)',
	'No implicit object creation when called as function';
}
