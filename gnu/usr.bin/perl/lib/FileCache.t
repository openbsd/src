#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..5\n";

use FileCache maxopen=>2;
my @files = qw(foo bar baz quux);

{# Test 1: that we can open files
     for my $path ( @files ){
	 cacheout $path;
	 print $path "$path 1\n";
     }
     print "not " unless scalar map({ -f } @files) == 4;
     print "ok 1\n";
}


{# Test 2: that we actually adhere to maxopen
    my @cat;
    for my $path ( @files ){
	print $path "$path 2\n";
        close($path);
	open($path, $path);
	<$path>;
	push @cat, <$path>;
        close($path);
    }
    print "not " if (grep {/foo|bar/} @cat) && ! (grep {/baz|quux/} @cat);
    print "ok 2\n" ;
}

{# Test 3: that we open for append on second viewing
     my @cat;
     for my $path ( @files ){
	 cacheout $path;
	 print $path "$path 3\n";
     }
     for my $path ( @files ){
	 open($path, $path);
	 push @cat, do{ local $/; <$path>};
         close($path);
     }
     print "not " unless scalar map({ /3$/ } @cat) == 4;
     print "ok 3\n";
}


{# Test 4: that 2 arg format works
     cacheout '+<', "foo";
     print foo "foo 2\n";
     close foo;
     cacheout '<', "foo";
     print "not " unless <foo> eq "foo 2\n";
     print "ok 4\n";
     close(foo);
}

{# Test 5: that close is overridden properly
     cacheout local $_ = "Foo_Bar";
     print $_ "Hello World\n";
     close($_);
     open($_, "+>$_");
     print $_ "$_\n";
     seek($_, 0, 0);
     print "not " unless <$_> eq "$_\n";
     print "ok 5\n";
     close($_);
}

q(
{# Test close override
     package Bob;
     use FileCache;
     cacheout local $_ = "Foo_Bar";
     print $_ "Hello World\n";
     close($_);
     open($_, "+>$_");
     print $_ "$_\n";
     seek($_, 0, 0);
     print "not " unless <$_> eq "$_\n";
     print "ok 5\n";
     close($_);
}
);

1 while unlink @files, "Foo_Bar";
