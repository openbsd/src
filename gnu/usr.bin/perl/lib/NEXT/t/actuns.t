use Test::More tests => 6;

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = qw(../lib);
    }
}

BEGIN { use_ok('NEXT') };
my $order = 0;

package A;
@ISA = qw/B C D/;

sub test { ::ok(++$order==1,"test A"); $_[0]->NEXT::UNSEEN::ACTUAL::test;}

package B;
@ISA = qw/D C/;
sub test { ::ok(++$order==2,"test B"); $_[0]->NEXT::ACTUAL::UNSEEN::test;}

package C;
@ISA = qw/D/;
sub test { ::ok(++$order==4,"test C"); $_[0]->NEXT::UNSEEN::ACTUAL::test;}

package D;

sub test { ::ok(++$order==3,"test D"); $_[0]->NEXT::ACTUAL::UNSEEN::test;}

package main;

my $foo = {};

bless($foo,"A");

eval{ $foo->test }
	? fail("Didn't die on missing ancestor") 
	: pass("Correctly dies after C");
