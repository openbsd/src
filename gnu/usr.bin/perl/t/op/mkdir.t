#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 13;

use File::Path;
rmtree('blurfl');

# tests 3 and 7 rather naughtily expect English error messages
$ENV{'LC_ALL'} = 'C';
$ENV{LANGUAGE} = 'C'; # GNU locale extension

ok(mkdir('blurfl',0777));
ok(!mkdir('blurfl',0777));
like($!, qr/cannot move|exist|denied|unknown/i);
ok(-d 'blurfl');
ok(rmdir('blurfl'));
ok(!rmdir('blurfl'));
like($!, qr/cannot find|such|exist|not found|not a directory|unknown/i);
ok(mkdir('blurfl'));
ok(rmdir('blurfl'));

SKIP: {
    # trailing slashes will be removed before the system call to mkdir
    # but we don't care for MacOS ...
    skip("MacOS", 4) if $^O eq 'MacOS';
    ok(mkdir('blurfl///'));
    ok(-d 'blurfl');
    ok(rmdir('blurfl///'));
    ok(!-d 'blurfl');
}
