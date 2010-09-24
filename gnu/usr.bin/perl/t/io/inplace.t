#!./perl
use strict;
require './test.pl';

$^I = $^O eq 'VMS' ? '_bak' : '.bak';

plan( tests => 6 );

my @tfiles     = (tempfile(), tempfile(), tempfile());
my @tfiles_bak = map "$_$^I", @tfiles;

END { unlink_all(@tfiles_bak); }

for my $file (@tfiles) {
    runperl( prog => 'print qq(foo\n);', 
             args => ['>', $file] );
}

@ARGV = @tfiles;

while (<>) {
    s/foo/bar/;
}
continue {
    print;
}

is ( runperl( prog => 'print<>;', args => \@tfiles ), 
     "bar\nbar\nbar\n", 
     "file contents properly replaced" );

is ( runperl( prog => 'print<>;', args => \@tfiles_bak ), 
     "foo\nfoo\nfoo\n", 
     "backup file contents stay the same" );

SKIP:
{
    # based on code, dosish and epoc systems can't do no-backup inplace
    # edits
    $^O =~ /^(MSWin32|cygwin|uwin|dos|epoc|os2)$/
	and skip("Can't inplace edit without backups on $^O", 4);
    
    our @ifiles = ( tempfile(), tempfile(), tempfile() );
    
    {
	for my $file (@ifiles) {
	    runperl( prog => 'print qq(bar\n);',
		     args => [ '>', $file ] );
	}
	
	local $^I = '';
    local @ARGV = @ifiles;
	
	while (<>) {
	    print "foo$_";
	}
	
	is(scalar(@ARGV), 0, "consumed ARGV");
	
#	runperl may quote its arguments, so don't expect to be able
#	to reuse things you send it.

	my @my_ifiles = @ifiles;
	is( runperl( prog => 'print<>;', args => \@my_ifiles ),
	    "foobar\nfoobar\nfoobar\n",
	    "normal inplace edit");
    }
    
    # test * equivalency RT #70802
    {
	for my $file (@ifiles) {
	    runperl( prog => 'print qq(bar\n);',
		     args => [ '>', $file ] );
	}
	
	local $^I = '*';
	local @ARGV = @ifiles;
	
	while (<>) {
	    print "foo$_";
	}
	
	is(scalar(@ARGV), 0, "consumed ARGV");
	
	my @my_ifiles = @ifiles;
	is( runperl( prog => 'print<>;', args => \@my_ifiles ),
	    "foobar\nfoobar\nfoobar\n",
	    "normal inplace edit");
    }
    
    END { unlink_all(@ifiles); }
}
