BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = qw(../lib);
    }
}

BEGIN { print "1..9\n"; }
use NEXT;

my $count=1;

package A;
@ISA = qw/B C D/;

sub test { print "ok ", $count++, "\n"; $_[0]->NEXT::ACTUAL::test;}

package B;
@ISA = qw/C D/;
sub test { print "ok ", $count++, "\n"; $_[0]->NEXT::ACTUAL::test;}

package C;
@ISA = qw/D/;
sub test { print "ok ", $count++, "\n"; $_[0]->NEXT::ACTUAL::test;}

package D;

sub test { print "ok ", $count++, "\n"; $_[0]->NEXT::ACTUAL::test;}

package main;

my $foo = {};

bless($foo,"A");

eval { $foo->test } and print "not ";
print "ok 9\n";
