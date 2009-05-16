require Test::Simple;
# $Id: death_with_handler.plx,v 1.1 2009/05/16 21:42:58 simon Exp $

push @INC, 't/lib';
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();

Test::Simple->import(tests => 2);

# Test we still get the right exit code despite having a die
# handler.
$SIG{__DIE__} = sub {};

require Dev::Null;
tie *STDERR, 'Dev::Null';

ok(1);
ok(1);
die "This is a test";
