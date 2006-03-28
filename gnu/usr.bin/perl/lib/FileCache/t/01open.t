#!./perl
use FileCache;
use vars qw(@files);
BEGIN {
    @files = qw(foo bar baz quux Foo_Bar);
    chdir 't' if -d 't';

    #For tests within the perl distribution
    @INC = '../lib' if -d '../lib';
    END;
}
END{
  1 while unlink @files;
}


print "1..1\n";

{# Test 1: that we can open files
     for my $path ( @files ){
	 cacheout $path;
	 print $path "$path 1\n";
	 close $path;
     }
     print "not " unless scalar map({ -f } @files) == scalar @files;
     print "ok 1\n";
}
