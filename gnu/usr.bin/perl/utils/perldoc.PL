#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;

\@pagers = ();
push \@pagers, "$Config{'pager'}" if -x "$Config{'pager'}";
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

#
# Perldoc revision #1 -- look up a piece of documentation in .pod format that
# is embedded in the perl installation tree.
#
# This is not to be confused with Tom Christianson's perlman, which is a
# man replacement, written in perl. This perldoc is strictly for reading
# the perl manuals, though it too is written in perl.

if(@ARGV<1) {
	$me = $0;		# Editing $0 is unportable
	$me =~ s,.*/,,;
	die <<EOF;
Usage: $me [-h] [-v] [-t] [-u] [-m] [-l] PageName|ModuleName|ProgramName
       $me -f PerlFunc

We suggest you use "perldoc perldoc" to get aquainted 
with the system.
EOF
}

use Getopt::Std;
use Config '%Config';

@global_found = ();
$global_target = "";

$Is_VMS = $^O eq 'VMS';
$Is_MSWin32 = $^O eq 'MSWin32';

sub usage{
    warn "@_\n" if @_;
    # Erase evidence of previous errors (if any), so exit status is simple.
    $! = 0;
    die <<EOF;
perldoc [options] PageName|ModuleName|ProgramName...
perldoc [options] -f BuiltinFunction

Options:
    -h   Display this help message
    -t   Display pod using pod2text instead of pod2man and nroff
             (-t is the default on win32)
    -u	 Display unformatted pod text
    -m   Display modules file in its entirety
    -l   Display the modules file name
    -v	 Verbosely describe what's going on

PageName|ModuleName...
         is the name of a piece of documentation that you want to look at. You 
         may either give a descriptive name of the page (as in the case of
         `perlfunc') the name of a module, either like `Term::Info', 
         `Term/Info', the partial name of a module, like `info', or 
         `makemaker', or the name of a program, like `perldoc'.

BuiltinFunction
         is the name of a perl function.  Will extract documentation from
         `perlfunc'.
         
Any switches in the PERLDOC environment variable will be used before the 
command line arguments.

EOF
}

use Text::ParseWords;


unshift(@ARGV,shellwords($ENV{"PERLDOC"}));

getopts("mhtluvf:") || usage;

usage if $opt_h || $opt_h; # avoid -w warning

if ($opt_t + $opt_u + $opt_m + $opt_l > 1) {
    usage("only one of -t, -u, -m or -l")
} elsif ($Is_MSWin32) {
    $opt_t = 1 unless $opt_t + $opt_u + $opt_m + $opt_l;
}

if ($opt_t) { require Pod::Text; import Pod::Text; }

if ($opt_f) {
   @pages = ("perlfunc");
} else {
   @pages = @ARGV;
}

# Does this look like a module or extension directory?
if (-f "Makefile.PL") {
	# Add ., lib and blib/* libs to @INC (if they exist)
	unshift(@INC, '.');
	unshift(@INC, 'lib') if -d 'lib';
	require ExtUtils::testlib;
}



sub containspod {
    my($file, $readit) = @_;
    return 1 if !$readit && $file =~ /\.pod$/i;
    local($_);
    open(TEST,"<$file");
    while(<TEST>) {
	if(/^=head/) {
	    close(TEST);
	    return 1;
	}
    }
    close(TEST);
    return 0;
}

sub minus_f_nocase {
     my($file) = @_;
     # on a case-forgiving file system we can simply use -f $file
     if ($Is_VMS or $Is_MSWin32 or $^O eq 'os2') {
        return $file if -f $file and -r _;
	warn "Ignored $file: unreadable\n" unless -r _;
	return '';
     }
     local *DIR;
     local($")="/";
     my(@p,$p,$cip);
     foreach $p (split(/\//, $file)){
	my $try = "@p/$p";
	stat $try;
 	if (-d _){
 	    push @p, $p;
	    if ( $p eq $global_target) {
		$tmp_path = join ('/', @p);
		my $path_f = 0;
		for (@global_found) {
		    $path_f = 1 if $_ eq $tmp_path;
		}
		push (@global_found, $tmp_path) unless $path_f;
		print STDERR "Found as @p but directory\n" if $opt_v;
	    }
 	} elsif (-f _ && -r _) {
 	    return $try;
 	} elsif (-f _) {
	    warn "Ignored $try: unreadable\n";
 	} else {
 	    my $found=0;
 	    my $lcp = lc $p;
 	    opendir DIR, "@p";
 	    while ($cip=readdir(DIR)) {
 		if (lc $cip eq $lcp){
 		    $found++;
 		    last;
 		}
 	    }
 	    closedir DIR;
 	    return "" unless $found;
 	    push @p, $cip;
 	    return "@p" if -f "@p" and -r _;
	    warn "Ignored $file: unreadable\n" if -f _;
 	}
     }
     return; # is not a file
}
 

sub check_file {
    my($file) = @_;
    return minus_f_nocase($file) && containspod($file) ? $file : "";
}


sub searchfor {
    my($recurse,$s,@dirs) = @_;
    $s =~ s!::!/!g;
    $s = VMS::Filespec::unixify($s) if $Is_VMS;
    return $s if -f $s && containspod($s);
    printf STDERR "Looking for $s in @dirs\n" if $opt_v;
    my $ret;
    my $i;
    my $dir;
    $global_target = (split('/', $s))[-1];
    for ($i=0; $i<@dirs; $i++) {
	$dir = $dirs[$i];
	($dir = VMS::Filespec::unixpath($dir)) =~ s!/$!! if $Is_VMS;
	if (       ( $ret = check_file "$dir/$s.pod")
		or ( $ret = check_file "$dir/$s.pm")
		or ( $ret = check_file "$dir/$s")
		or ( $Is_VMS and
		     $ret = check_file "$dir/$s.com")
		or ( $^O eq 'os2' and 
		     $ret = check_file "$dir/$s.cmd")
		or ( ($Is_MSWin32 or $^O eq 'os2') and 
		     $ret = check_file "$dir/$s.bat")
		or ( $ret = check_file "$dir/pod/$s.pod")
		or ( $ret = check_file "$dir/pod/$s")
	) {
	    return $ret;
	}
	
	if ($recurse) {
	    opendir(D,$dir);
	    my @newdirs = map "$dir/$_", grep {
		not /^\.\.?$/ and
		not /^auto$/  and   # save time! don't search auto dirs
		-d  "$dir/$_"
	    } readdir D;
	    closedir(D);
	    next unless @newdirs;
	    @newdirs = map((s/.dir$//,$_)[1],@newdirs) if $Is_VMS;
	    print STDERR "Also looking in @newdirs\n" if $opt_v;
	    push(@dirs,@newdirs);
	}
    }
    return ();
}


foreach (@pages) {
	print STDERR "Searching for $_\n" if $opt_v;
	# We must look both in @INC for library modules and in PATH
	# for executables, like h2xs or perldoc itself.
	@searchdirs = @INC;
	unless ($opt_m) { 
	    if ($Is_VMS) {
		my($i,$trn);
		for ($i = 0; $trn = $ENV{'DCL$PATH'.$i}; $i++) {
		    push(@searchdirs,$trn);
		}
	    } else {
	        push(@searchdirs, grep(-d, split($Config{path_sep}, 
						 $ENV{'PATH'})));
	    }
	    @files= searchfor(0,$_,@searchdirs);
	}
	if( @files ) {
		print STDERR "Found as @files\n" if $opt_v;
	} else {
		# no match, try recursive search
		
		@searchdirs = grep(!/^\.$/,@INC);
		
		@files= searchfor(1,$_,@searchdirs);
		if( @files ) {
			print STDERR "Loosely found as @files\n" if $opt_v;
		} else {
			print STDERR "No documentation found for \"$_\".\n";
			if (@global_found) {
			    print STDERR "However, try\n";
			    my $dir = $file = "";
			    for $dir (@global_found) {
				opendir(DIR, $dir) or die "$!";
				while ($file = readdir(DIR)) {
				    next if ($file =~ /^\./);
				    $file =~ s/\.(pm|pod)$//;
				    print STDERR "\tperldoc $_\::$file\n";
				}
				closedir DIR;
			    }
			}
		}
	}
	push(@found,@files);
}

if(!@found) {
	exit ($Is_VMS ? 98962 : 1);
}

if ($opt_l) {
    print join("\n", @found), "\n";
    exit;
}

if( ! -t STDOUT ) { $no_tty = 1 }

if ($Is_MSWin32) {
	$tmp = "$ENV{TEMP}\\perldoc1.$$";
	push @pagers, qw( more< less notepad );
	unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
} elsif ($Is_VMS) {
	$tmp = 'Sys$Scratch:perldoc.tmp1_'.$$;
	push @pagers, qw( most more less type/page );
} else {
	if ($^O eq 'os2') {
	  require POSIX;
	  $tmp = POSIX::tmpnam();
	} else {
	  $tmp = "/tmp/perldoc1.$$";	  
	}
	push @pagers, qw( more less pg view cat );
	unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
}
unshift @pagers, $ENV{PERLDOC_PAGER} if $ENV{PERLDOC_PAGER};

if ($opt_m) {
	foreach $pager (@pagers) {
		system("$pager @found") or exit;
	}
	if ($Is_VMS) { eval 'use vmsish qw(status exit); exit $?' }
	exit 1;
} 

if ($opt_f) {
   my $perlfunc = shift @found;
   open(PFUNC, $perlfunc) or die "Can't open $perlfunc: $!";

   # Skip introduction
   while (<PFUNC>) {
       last if /^=head2 Alphabetical Listing of Perl Functions/;
   }

   # Look for our function
   my $found = 0;
   my @pod;
   while (<PFUNC>) {
       if (/^=item\s+\Q$opt_f\E\b/o)  {
	   $found = 1;
       } elsif (/^=item/) {
	   last if $found > 1;
       }
       next unless $found;
       push @pod, $_;
       ++$found if /^\w/;	# found descriptive text
   }
   if (@pod) {
       if ($opt_t) {
	   open(FORMATTER, "| pod2text") || die "Can't start filter";
	   print FORMATTER "=over 8\n\n";
	   print FORMATTER @pod;
	   print FORMATTER "=back\n";
	   close(FORMATTER);
       } else {
	   print @pod;
       }
   } else {
       die "No documentation for perl function `$opt_f' found\n";
   }
   exit;
}

foreach (@found) {

	if($opt_t) {
		open(TMP,">>$tmp");
		Pod::Text::pod2text($_,*TMP);
		close(TMP);
	} elsif(not $opt_u) {
		my $cmd = "pod2man --lax $_ | nroff -man";
		$cmd .= " | col -x" if $^O =~ /hpux/;
		$rslt = `$cmd`;
		unless(($err = $?)) {
			open(TMP,">>$tmp");
			print TMP $rslt;
			close TMP;
		}
	}
	                                                
	if( $opt_u or $err or -z $tmp) {
		open(OUT,">>$tmp");
		open(IN,"<$_");
		$cut = 1;
		while (<IN>) {
			$cut = $1 eq 'cut' if /^=(\w+)/;
			next if $cut;
			print OUT;
		}
		close(IN);
		close(OUT);
	}
}

if( $no_tty ) {
	open(TMP,"<$tmp");
	print while <TMP>;
	close(TMP);
} else {
	foreach $pager (@pagers) {
		system("$pager $tmp") or last;
	}
}

1 while unlink($tmp); #Possibly pointless VMSism

exit 0;

__END__

=head1 NAME

perldoc - Look up Perl documentation in pod format.

=head1 SYNOPSIS

B<perldoc> [B<-h>] [B<-v>] [B<-t>] [B<-u>] [B<-m>] [B<-l>] PageName|ModuleName|ProgramName

B<perldoc> B<-f> BuiltinFunction

=head1 DESCRIPTION

I<perldoc> looks up a piece of documentation in .pod format that is embedded
in the perl installation tree or in a perl script, and displays it via
C<pod2man | nroff -man | $PAGER>. (In addition, if running under HP-UX,
C<col -x> will be used.) This is primarily used for the documentation for
the perl library modules.

Your system may also have man pages installed for those modules, in
which case you can probably just use the man(1) command.

=head1 OPTIONS

=over 5

=item B<-h> help

Prints out a brief help message.

=item B<-v> verbose

Describes search for the item in detail.

=item B<-t> text output

Display docs using plain text converter, instead of nroff. This may be faster,
but it won't look as nice.

=item B<-u> unformatted

Find docs only; skip reformatting by pod2*

=item B<-m> module

Display the entire module: both code and unformatted pod documentation.
This may be useful if the docs don't explain a function in the detail
you need, and you'd like to inspect the code directly; perldoc will find
the file for you and simply hand it off for display.

=item B<-l> file name only

Display the file name of the module found.

=item B<-f> perlfunc

The B<-f> option followed by the name of a perl built in function will
extract the documentation of this function from L<perlfunc>.

=item B<PageName|ModuleName|ProgramName>

The item you want to look up.  Nested modules (such as C<File::Basename>)
are specified either as C<File::Basename> or C<File/Basename>.  You may also
give a descriptive name of a page, such as C<perlfunc>. You make also give a
partial or wrong-case name, such as "basename" for "File::Basename", but
this will be slower, if there is more then one page with the same partial
name, you will only get the first one.

=back

=head1 ENVIRONMENT

Any switches in the C<PERLDOC> environment variable will be used before the 
command line arguments.  C<perldoc> also searches directories
specified by the C<PERL5LIB> (or C<PERLLIB> if C<PERL5LIB> is not
defined) and C<PATH> environment variables.
(The latter is so that embedded pods for executables, such as
C<perldoc> itself, are available.)

=head1 AUTHOR

Kenneth Albanowski <kjahds@kjahds.com>

Minor updates by Andy Dougherty <doughera@lafcol.lafayette.edu>

=cut

#
# Version 1.12: Sat Apr 12 22:41:09 EST 1997
#       Gurusamy Sarathy <gsar@umich.edu>
#	-various fixes for win32
# Version 1.11: Tue Dec 26 09:54:33 EST 1995
#       Kenneth Albanowski <kjahds@kjahds.com>
#   -added Charles Bailey's further VMS patches, and -u switch
#   -added -t switch, with pod2text support
# 
# Version 1.10: Thu Nov  9 07:23:47 EST 1995
#		Kenneth Albanowski <kjahds@kjahds.com>
#	-added VMS support
#	-added better error recognition (on no found pages, just exit. On
#	 missing nroff/pod2man, just display raw pod.)
#	-added recursive/case-insensitive matching (thanks, Andreas). This
#	 slows things down a bit, unfortunately. Give a precise name, and
#	 it'll run faster.
#
# Version 1.01:	Tue May 30 14:47:34 EDT 1995
#		Andy Dougherty  <doughera@lafcol.lafayette.edu>
#   -added pod documentation.
#   -added PATH searching.
#   -added searching pod/ subdirectory (mainly to pick up perlfunc.pod
#    and friends.
#
#
# TODO:
#
#	Cache directories read during sloppy match
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
