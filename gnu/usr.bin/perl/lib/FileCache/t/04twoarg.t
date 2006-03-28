#!./perl
BEGIN {
    use FileCache;
    chdir 't' if -d 't';

    #For tests within the perl distribution
    @INC = '../lib' if -d '../lib';
    END;
}
END{
  unlink('foo');
}

print "1..1\n";

{# Test 4: that 2 arg format works, and that we cycle on mode change
     cacheout '>', "foo";
     print foo "foo 4\n";
     cacheout '+>', "foo";
     print foo "foo 44\n";
     seek(foo, 0, 0);
     print 'not ' unless <foo> eq "foo 44\n";
     print "ok 1\n";
     close foo;
}
