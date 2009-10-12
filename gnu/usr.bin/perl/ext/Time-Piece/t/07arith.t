use Test::More tests => 13;

BEGIN { use_ok('Time::Piece'); }

ok(1);

my $t = gmtime(951827696); # 2000-02-29T12:34:56

is($t->mon, 2);
is($t->mday, 29);

my $t2 = $t->add_months(1);
is($t2->year, 2000);
is($t2->mon,  3);
is($t2->mday, 29);

my $t3 = $t->add_months(-1);
is($t3->year, 2000);
is($t3->mon,  1);
is($t3->mday, 29);

# this one wraps around to March because of the leap year
my $t4 = $t->add_years(1);
is($t4->year, 2001);
is($t4->mon, 3);
is($t4->mday, 1);
