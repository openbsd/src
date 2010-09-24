#!./perl

use FileCache;

END { unlink('foo') }

use Test::More tests => 1;

{# Test 4: that 2 arg format works, and that we cycle on mode change
     cacheout '>', "foo";
     print foo "foo 4\n";
     cacheout '+>', "foo";
     print foo "foo 44\n";
     seek(foo, 0, 0);
     ok(<foo> eq "foo 44\n");
     close foo;
}
