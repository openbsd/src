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
chdir(dirname($0));
($file = basename($0)) =~ s/\.PL$//;
$file =~ s/\.pl$//
	if ($^O eq 'VMS' or $^O eq 'os2');  # "case-forgiving"

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{'startperl'}
    eval 'exec perl -S \$0 "\$@"'
	if 0;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';
    eval 'exec perl -S $0 "$@"'
	if 0;

#
# Perldoc revision #1 -- look up a piece of documentation in .pod format that
# is embedded in the perl installation tree.
#
# This is not to be confused with Tom Christianson's perlman, which is a
# man replacement, written in perl. This perldoc is strictly for reading
# the perl manuals, though it too is written in perl.

if(@ARGV<1) {
	die <<EOF;
Usage: $0 [-h] [-v] [-t] [-u] [-m] PageName|ModuleName|ProgramName

We suggest you use "perldoc perldoc" to get aquainted 
with the system.
EOF
}

use Getopt::Std;
$Is_VMS = $^O eq 'VMS';

sub usage{
        warn "@_\n" if @_;
    die <<EOF;
perldoc [-h] [-v] [-u] PageName|ModuleName|ProgramName...
    -h   Display this help message.
    -t   Display pod using pod2text instead of pod2man and nroff.
    -u	 Display unformatted pod text
    -m   Display modules file in its entirety
    -v	 Verbosely describe what's going on.
PageName|ModuleName...
         is the name of a piece of documentation that you want to look at. You 
         may either give a descriptive name of the page (as in the case of
         `perlfunc') the name of a module, either like `Term::Info', 
         `Term/Info', the partial name of a module, like `info', or 
         `makemaker', or the name of a program, like `perldoc'.
         
Any switches in the PERLDOC environment variable will be used before the 
command line arguments.

EOF
}

use Text::ParseWords;


unshift(@ARGV,shellwords($ENV{"PERLDOC"}));

getopts("mhtuv") || usage;

usage if $opt_h || $opt_h; # avoid -w warning

usage("only one of -t, -u, or -m") if $opt_t + $opt_u + $opt_m > 1;

if ($opt_t) { require Pod::Text; import Pod::Text; }

@pages = @ARGV;

sub containspod {
	my($file) = @_;
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
     local *DIR;
     local($")="/";
     my(@p,$p,$cip);
     foreach $p (split(/\//, $file)){
	if ($Is_VMS and not scalar @p) {
	    # VMS filesystems don't begin at '/'
	    push(@p,$p);
	    next;
	}
 	if (-d ("@p/$p")){
 	    push @p, $p;
 	} elsif (-f ("@p/$p")) {
 	    return "@p/$p";
 	} else {
 	    my $found=0;
 	    my $lcp = lc $p;
 	    opendir DIR, "@p";
 	    while ($cip=readdir(DIR)) {
		$cip =~ s/\.dir$// if $Is_VMS;
 		if (lc $cip eq $lcp){
 		    $found++;
 		    last;
 		}
 	    }
 	    closedir DIR;
 	    return "" unless $found;
 	    push @p, $cip;
 	    return "@p" if -f "@p";
 	}
     }
     return; # is not a file
 }
 
  sub searchfor {
  	my($recurse,$s,@dirs) = @_;
  	$s =~ s!::!/!g;
  	$s = VMS::Filespec::unixify($s) if $Is_VMS;
  	printf STDERR "looking for $s in @dirs\n" if $opt_v;
 	my $ret;
 	my $i;
 	my $dir;
  	for ($i=0;$i<@dirs;$i++) {
  		$dir = $dirs[$i];
  		($dir = VMS::Filespec::unixpath($dir)) =~ s!/$!! if $Is_VMS;
 	    if ((    $ret = minus_f_nocase "$dir/$s.pod")
 		or ( $ret = minus_f_nocase "$dir/$s.pm"  and containspod($ret))
 		or ( $ret = minus_f_nocase "$dir/$s"     and containspod($ret))
 		or ( $Is_VMS and 
 		     $ret = minus_f_nocase "$dir/$s.com" and containspod($ret))
 		or ( $ret = minus_f_nocase "$dir/pod/$s.pod")
 		or ( $ret = minus_f_nocase "$dir/pod/$s" and containspod($ret)))
 		{ return $ret; }
 		
 		if($recurse) {
			opendir(D,$dir);
			my(@newdirs) = grep(-d,map("$dir/$_",grep(!/^\.\.?$/,readdir(D))));
			closedir(D);
			@newdirs = map((s/.dir$//,$_)[1],@newdirs) if $Is_VMS;
			next unless @newdirs;
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
		    push(@searchdirs, grep(-d, split(':', $ENV{'PATH'})));
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
			print STDERR "No documentation found for '$_'\n";
		}
	}
	push(@found,@files);
}

if(!@found) {
	exit ($Is_VMS ? 98962 : 1);
}

if( ! -t STDOUT ) { $opt_f = 1 }

unless($Is_VMS) {
	$tmp = "/tmp/perldoc1.$$";
	$goodresult = 0;
	@pagers = qw( more less pg view cat );
	unshift(@pagers,$ENV{PAGER}) if $ENV{PAGER};
} else {
	$tmp = 'Sys$Scratch:perldoc.tmp1_'.$$;
	@pagers = qw( most more less type/page );
	unshift(@pagers,$ENV{PERLDOC_PAGER}) if $ENV{PERLDOC_PAGER};
	$goodresult = 1;
}

if ($opt_m) {
    foreach $pager (@pagers) {
	my($sts) = system("$pager @found");
	exit 0 if ($Is_VMS ? ($sts & 1) : !$sts);
    }
    exit $Is_VMS ? $sts : 1;
} 

foreach (@found) {

	if($opt_t) {
		open(TMP,">>$tmp");
		Pod::Text::pod2text($_,*TMP);
		close(TMP);
	} elsif(not $opt_u) {
		open(TMP,">>$tmp");
		$rslt = `pod2man $_ | nroff -man`;
		if ($Is_VMS) { $err = !($? % 2) || $rslt =~ /IVVERB/; }
		else      { $err = $?; }
		print TMP $rslt unless $err;
		close TMP;
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

if( $opt_f ) {
	open(TMP,"<$tmp");
	print while <TMP>;
	close(TMP);
} else {
	foreach $pager (@pagers) {
		$sts = system("$pager $tmp");
		last if $Is_VMS && ($sts & 1);
		last unless $sts;
	}
}

1 while unlink($tmp); #Possibly pointless VMSism

exit 0;

__END__

=head1 NAME

perldoc - Look up Perl documentation in pod format.

=head1 SYNOPSIS

B<perldoc> [B<-h>] [B<-v>] [B<-t>] [B<-u>] PageName|ModuleName|ProgramName

=head1 DESCRIPTION

I<perldoc> looks up a piece of documentation in .pod format that is
embedded in the perl installation tree or in a perl script, and displays
it via pod2man | nroff -man | $PAGER.  This is primarily used for the
documentation for the perl library modules. 

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

=head1 SEE ALSO

=head1 DIAGNOSTICS

=cut

#
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
