#!./perl -w

BEGIN { print "1..7\n"; }
BEGIN {
    print "not " if exists $^H{foo};
    print "ok 1 - \$^H{foo} doesn't exist initially\n";
}
{
    # simulate a pragma -- don't forget HINT_LOCALIZE_HH
    BEGIN { $^H |= 0x00020000; $^H{foo} = "a"; }
    BEGIN {
	print "not " if $^H{foo} ne "a";
	print "ok 2 - \$^H{foo} is now 'a'\n";
    }
    {
	BEGIN { $^H |= 0x00020000; $^H{foo} = "b"; }
	BEGIN {
	    print "not " if $^H{foo} ne "b";
	    print "ok 3 - \$^H{foo} is now 'b'\n";
	}
    }
    BEGIN {
	print "not " if $^H{foo} ne "a";
	print "ok 4 - \$H^{foo} restored to 'a'\n";
    }
    CHECK {
	print "not " if exists $^H{foo};
	print "ok 6 - \$^H{foo} doesn't exist when compilation complete\n";
    }
    print "not " if exists $^H{foo};
    print "ok 7 - \$^H{foo} doesn't exist at runtime\n";
}
BEGIN {
    print "not " if exists $^H{foo};
    print "ok 5 - \$^H{foo} doesn't exist while finishing compilation\n";
}
