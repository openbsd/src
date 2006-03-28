#!./perl
BEGIN {
    chdir 't' if -d 't';

    #For tests within the perl distribution
    @INC = '../lib' if -d '../lib';
    END;

    # Functions exported by FileCache;
    @funcs  = qw[cacheout cacheout_close];
    $i      = 0;
    
    # number of tests
    print "1..8\n";
}

# Test 6: Test that exporting both works to package main and
# other packages. Now using Exporter.

# First, we shouldn't be able to have these in our namespace
# Add them to BEGIN so the later 'use' doesn't influence this
# test
BEGIN {   
    for my $f (@funcs) {
        ++$i;
        print 'not ' if __PACKAGE__->can($f);
        print "ok $i\n"; 
    }
}

# With an empty import list, we also shouldn't have them in
# our namespace.
# Add them to BEGIN so the later 'use' doesn't influence this
# test
BEGIN {   
    use FileCache ();
    for my $f (@funcs) {
        ++$i;
        print 'not ' if __PACKAGE__->can($f);
        print "ok $i\n"; 
    }
}


# Now, we use FileCache in 'main'
{   use FileCache;
    for my $f (@funcs) {
        ++$i;
        print 'not ' if !__PACKAGE__->can($f);
        print "ok $i\n"; 
    }
}

# Now we use them in another package
{   package X;
    use FileCache;
    for my $f (@main::funcs) {
        ++$main::i;
        print 'not ' if !__PACKAGE__->can($f);
        print "ok $main::i\n"; 
    }
}
