#!./perl

# Tests the scoping of $^H and %^H

BEGIN { print "1..14\n"; }
BEGIN {
    print "not " if exists $^H{foo};
    print "ok 1 - \$^H{foo} doesn't exist initially\n";
    print "not " if $^H & 0x00020000;
    print "ok 2 - \$^H doesn't contain HINT_LOCALIZE_HH initially\n";
}
{
    # simulate a pragma -- don't forget HINT_LOCALIZE_HH
    BEGIN { $^H |= 0x00020000; $^H{foo} = "a"; }
    BEGIN {
	print "not " if $^H{foo} ne "a";
	print "ok 3 - \$^H{foo} is now 'a'\n";
	print "not " unless $^H & 0x00020000;
	print "ok 4 - \$^H contains HINT_LOCALIZE_HH while compiling\n";
    }
    {
	BEGIN { $^H |= 0x00020000; $^H{foo} = "b"; }
	BEGIN {
	    print "not " if $^H{foo} ne "b";
	    print "ok 5 - \$^H{foo} is now 'b'\n";
	}
    }
    BEGIN {
	print "not " if $^H{foo} ne "a";
	print "ok 6 - \$H^{foo} restored to 'a'\n";
    }
    # The pragma settings disappear after compilation
    # (test at CHECK-time and at run-time)
    CHECK {
	print "not " if exists $^H{foo};
	print "ok 9 - \$^H{foo} doesn't exist when compilation complete\n";
	print "not " if $^H & 0x00020000;
	print "ok 10 - \$^H doesn't contain HINT_LOCALIZE_HH when compilation complete\n";
    }
    print "not " if exists $^H{foo};
    print "ok 11 - \$^H{foo} doesn't exist at runtime\n";
    print "not " if $^H & 0x00020000;
    print "ok 12 - \$^H doesn't contain HINT_LOCALIZE_HH at run-time\n";
    # op_entereval should keep the pragmas it was compiled with
    eval q*
	print "not " if $^H{foo} ne "a";
	print "ok 13 - \$^H{foo} is 'a' at eval-\"\" time # TODO\n";
	print "not " unless $^H & 0x00020000;
	print "ok 14 - \$^H contains HINT_LOCALIZE_HH at eval\"\"-time\n";
    *;
}
BEGIN {
    print "not " if exists $^H{foo};
    print "ok 7 - \$^H{foo} doesn't exist while finishing compilation\n";
    print "not " if $^H & 0x00020000;
    print "ok 8 - \$^H doesn't contain HINT_LOCALIZE_HH while finishing compilation\n";
}
