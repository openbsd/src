    eval 'exec perl -x -S "$0" ${1+"$@"}'
	if 0;	# In case running under some shell

require 5;
use Getopt::Std;
use Config;

$0 =~ s|.*[/\\]||;

my $usage = <<EOT;
Usage:  $0 [-h]
   or:  $0 [-w] [-u] [-a argstring] [-s stripsuffix] [files]
   or:  $0 [-w] [-u] [-n ntargs] [-o otherargs] [-s stripsuffix] [files]
        -n ntargs       arguments to invoke perl with in generated file
                            when run from Windows NT.  Defaults to
                            '-x -S "%0" %*'.
        -o otherargs    arguments to invoke perl with in generated file
                            other than when run from Windows NT.  Defaults
                            to '-x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9'.
        -a argstring    arguments to invoke perl with in generated file
                            ignoring operating system (for compatibility
                            with previous pl2bat versions).
        -u              update files that may have already been processed
                            by (some version of) pl2bat.
        -w              include "-w" on the /^#!.*perl/ line (unless
                            a /^#!.*perl/ line was already present).
        -s stripsuffix  strip this suffix from file before appending ".bat"
                            Not case-sensitive
                            Can be a regex if it begins with `/'
                            Defaults to "/\.plx?/"
        -h              show this help
EOT

my %OPT = ();
warn($usage), exit(0) if !getopts('whun:o:a:s:',\%OPT) or $OPT{'h'};
$OPT{'n'} = '-x -S "%0" %*' unless exists $OPT{'n'};
$OPT{'o'} = '-x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9' unless exists $OPT{'o'};
$OPT{'s'} = '/\\.plx?/' unless exists $OPT{'s'};
$OPT{'s'} = ($OPT{'s'} =~ m|^/([^/]*)| ? $1 : "\Q$OPT{'s'}\E");

my $head;
if(  defined( $OPT{'a'} )  ) {
    $head = <<EOT;
	\@rem = '--*-Perl-*--
	\@echo off
	perl $OPT{'a'}
	goto endofperl
	\@rem ';
EOT
} else {
    $head = <<EOT;
	\@rem = '--*-Perl-*--
	\@echo off
	if "%OS%" == "Windows_NT" goto WinNT
	perl $OPT{'o'}
	goto endofperl
	:WinNT
	perl $OPT{'n'}
	if NOT "%COMSPEC%" == "%SystemRoot%\\system32\\cmd.exe" goto endofperl
	if %errorlevel% == 9009 echo You do not have Perl in your PATH.
	goto endofperl
	\@rem ';
EOT
}
$head =~ s/^\t//gm;
my $headlines = 2 + ($head =~ tr/\n/\n/);
my $tail = "__END__\n:endofperl\n";

@ARGV = ('-') unless @ARGV;

foreach ( @ARGV ) {
    process($_);
}

sub process {
 my( $file )= @_;
    my $myhead = $head;
    my $linedone = 0;
    my $taildone = 0;
    my $linenum = 0;
    my $skiplines = 0;
    my $line;
    my $start= $Config{startperl};
    $start= "#!perl"   unless  $start =~ /^#!.*perl/;
    open( FILE, $file ) or die "$0: Can't open $file: $!";
    @file = <FILE>;
    foreach $line ( @file ) {
	$linenum++;
	if ( $line =~ /^:endofperl\b/ ) {
	    if(  ! exists $OPT{'u'}  ) {
		warn "$0: $file has already been converted to a batch file!\n";
		return;
	    }
	    $taildone++;
	}
	if ( not $linedone and $line =~ /^#!.*perl/ ) {
	    if(  exists $OPT{'u'}  ) {
		$skiplines = $linenum - 1;
		$line .= "#line ".(1+$headlines)."\n";
	    } else {
		$line .= "#line ".($linenum+$headlines)."\n";
	    }
	    $linedone++;
	}
	if ( $line =~ /^#\s*line\b/ and $linenum == 2 + $skiplines ) {
	    $line = "";
	}
    }
    close( FILE );
    $file =~ s/$OPT{'s'}$//oi;
    $file .= '.bat' unless $file =~ /\.bat$/i or $file =~ /^-$/;
    open( FILE, ">$file" ) or die "Can't open $file: $!";
    print FILE $myhead;
    print FILE $start, ( $OPT{'w'} ? " -w" : "" ),
	       "\n#line ", ($headlines+1), "\n" unless $linedone;
    print FILE @file[$skiplines..$#file];
    print FILE $tail unless $taildone;
    close( FILE );
}
__END__

=head1 NAME

pl2bat - wrap perl code into a batch file

=head1 SYNOPSIS

B<pl2bat> B<-h>

B<pl2bat> [B<-w>] S<[B<-a> I<argstring>]> S<[B<-s> I<stripsuffix>]> [files]

B<pl2bat> [B<-w>] S<[B<-n> I<ntargs>]> S<[B<-o> I<otherargs>]> S<[B<-s> I<stripsuffix>]> [files]

=head1 DESCRIPTION

This utility converts a perl script into a batch file that can be
executed on DOS-like operating systems.

Note that by default, the ".pl" suffix will be stripped before adding
a ".bat" suffix to the supplied file names.  This can be controlled
with the C<-s> option.

The default behavior is to have the batch file compare the C<OS>
environment variable against C<"Windows_NT">.  If they match, it
uses the C<%*> construct to refer to all the command line arguments
that were given to it, so you'll need to make sure that works on your
variant of the command shell.  It is known to work in the cmd.exe shell
under WindowsNT.  4DOS/NT users will want to put a C<ParameterChar = *>
line in their initialization file, or execute C<setdos /p*> in
the shell startup file.

On Windows95 and other platforms a nine-argument limit is imposed
on command-line arguments given to the generated batch file, since
they may not support C<%*> in batch files.

These can be overridden using the C<-n> and C<-o> options or the
deprecated C<-a> option.

=head1 OPTIONS

=over 8

=item B<-n> I<ntargs>

Arguments to invoke perl with in generated batch file when run from
Windows NT (or Windows 98, probably).  Defaults to S<'-x -S "%0" %*'>.

=item B<-o> I<otherargs>

Arguments to invoke perl with in generated batch file except when
run from Windows NT (ie. when run from DOS, Windows 3.1, or Windows 95).
Defaults to S<'-x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9'>.

=item B<-a> I<argstring>

Arguments to invoke perl with in generated batch file.  Specifying
B<-a> prevents the batch file from checking the C<OS> environment
variable to determine which operating system it is being run from.

=item B<-s> I<stripsuffix>

Strip a suffix string from file name before appending a ".bat"
suffix.  The suffix is not case-sensitive.  It can be a regex if
it begins with `/' (the trailing '/' is optional and a trailing
C<$> is always assumed).  Defaults to C</.plx?/>.

=item B<-w>

If no line matching C</^#!.*perl/> is found in the script, then such
a line is inserted just after the new preamble.  The exact line
depends on C<$Config{startperl}> [see L<Config>].  With the B<-w>
option, C<" -w"> is added after the value of C<$Config{startperl}>.
If a line matching C</^#!.*perl/> already exists in the script,
then it is not changed and the B<-w> option is ignored.

=item B<-u>

If the script appears to have already been processed by B<pl2bat>,
then the script is skipped and not processed unless B<-u> was
specified.  If B<-u> is specified, the existing preamble is replaced.

=item B<-h>

Show command line usage.

=back

=head1 EXAMPLES

	C:\> pl2bat foo.pl bar.PM 
	[..creates foo.bat, bar.PM.bat..]
	
	C:\> pl2bat -s "/\.pl|\.pm/" foo.pl bar.PM
	[..creates foo.bat, bar.bat..]
	
	C:\> pl2bat < somefile > another.bat
	
	C:\> pl2bat > another.bat
	print scalar reverse "rekcah lrep rehtona tsuj\n";
	^Z
	[..another.bat is now a certified japh application..]
	
	C:\> ren *.bat *.pl
	C:\> pl2bat -u *.pl
	[..updates the wrapping of some previously wrapped scripts..]
	
	C:\> pl2bat -u -s .bat *.bat
	[..same as previous example except more dangerous..]

=head1 BUGS

C<$0> will contain the full name, including the ".bat" suffix
when the generated batch file runs.  If you don't like this,
see runperl.bat for an alternative way to invoke perl scripts.

Default behavior is to invoke Perl with the B<-S> flag, so Perl will
search the PATH to find the script.   This may have undesirable
effects.

=head1 SEE ALSO

perl, perlwin32, runperl.bat

=cut

