# ID 20020716.013, the exit code would become 0 if the test died
# $Id: pre_plan_death.plx,v 1.2 2009/05/16 21:42:58 simon Exp $
# before a plan.

require Test::Simple;

push @INC, 't/lib';
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();

close STDERR;
die "Knife?";

Test::Simple->import(tests => 3);

ok(1);
ok(1);
ok(1);
