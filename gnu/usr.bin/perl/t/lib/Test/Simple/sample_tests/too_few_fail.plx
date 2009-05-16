require Test::Simple;
# $Id: too_few_fail.plx,v 1.2 2009/05/16 21:42:58 simon Exp $

push @INC, 't/lib';
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();

Test::Simple->import(tests => 5);


ok(0);
ok(1);
ok(0);
