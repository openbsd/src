#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '.';
    push @INC, '../lib';
}

print "1..17\n";

#
# This file tries to test builtin override using CORE::GLOBAL
#
my $dirsep = "/";

BEGIN { package Foo; *main::getlogin = sub { "kilroy"; } }

print "not " unless getlogin eq "kilroy";
print "ok 1\n";

my $t = 42;
BEGIN { *CORE::GLOBAL::time = sub () { $t; } }

print "not " unless 45 == time + 3;
print "ok 2\n";

#
# require has special behaviour
#
my $r;
BEGIN { *CORE::GLOBAL::require = sub { $r = shift; 1; } }

require Foo;
print "not " unless $r eq "Foo.pm";
print "ok 3\n";

require Foo::Bar;
print "not " unless $r eq join($dirsep, "Foo", "Bar.pm");
print "ok 4\n";

require 'Foo';
print "not " unless $r eq "Foo";
print "ok 5\n";

require 5.6;
print "not " unless $r eq "5.6";
print "ok 6\n";

require v5.6;
print "not " unless abs($r - 5.006) < 0.001 && $r eq "\x05\x06";
print "ok 7\n";

eval "use Foo";
print "not " unless $r eq "Foo.pm";
print "ok 8\n";

eval "use Foo::Bar";
print "not " unless $r eq join($dirsep, "Foo", "Bar.pm");
print "ok 9\n";

eval "use 5.6";
print "not " unless $r eq "5.6";
print "ok 10\n";

# localizing *CORE::GLOBAL::foo should revert to finding CORE::foo
{
    local(*CORE::GLOBAL::require);
    $r = '';
    eval "require NoNeXiSt;";
    print "not " if $r or $@ !~ /^Can't locate NoNeXiSt/i;
    print "ok 11\n";
}

#
# readline() has special behaviour too
#

$r = 11;
BEGIN { *CORE::GLOBAL::readline = sub (;*) { ++$r }; }
print <FH>	== 12 ? "ok 12\n" : "not ok 12\n";
print <$fh>	== 13 ? "ok 13\n" : "not ok 13\n";
my $pad_fh;
print <$pad_fh>	== 14 ? "ok 14\n" : "not ok 14\n";

# Non-global readline() override
BEGIN { *Rgs::readline = sub (;*) { --$r }; }
package Rgs;
print <FH>	== 13 ? "ok 15\n" : "not ok 15\n";
print <$fh>	== 12 ? "ok 16\n" : "not ok 16\n";
print <$pad_fh>	== 11 ? "ok 17\n" : "not ok 17\n";
