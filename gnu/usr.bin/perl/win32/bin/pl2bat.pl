#!perl -w
require 5;
use Getopt::Std;

$0 =~ s|.*[/\\]||;

my $usage = <<EOT;
Usage:  $0 [-h] [-a argstring] [-s stripsuffix] [files]
        -a argstring    arguments to invoke perl with in generated file
                            Defaults to "-x -S %0 %*" on WindowsNT,
			    "-x -S %0 %1 %2 %3 %4 %5 %6 %7 %8 %9" otherwise
        -s stripsuffix  strip this suffix from file before appending ".bat"
	                    Not case-sensitive
                            Can be a regex if it begins with `/'
	                    Defaults to "/\.pl/"
        -h              show this help
EOT

my %OPT = ();
warn($usage), exit(0) if !getopts('ha:s:',\%OPT) or $OPT{'h'};
$OPT{'a'} = ($^O eq 'MSWin32' and &Win32::IsWinNT
             ? '-x -S %0 %*'
	     : '-x -S %0 %1 %2 %3 %4 %5 %6 %7 %8 %9')
	  unless exists $OPT{'a'};
$OPT{'s'} = '.pl' unless exists $OPT{'s'};
$OPT{'s'} = ($OPT{'s'} =~ m|^/([^/]*)| ? $1 : "\Q$OPT{'s'}\E");

(my $head = <<EOT) =~ s/^\t//gm;
	\@rem = '--*-Perl-*--
	\@echo off
	perl $OPT{'a'}
	goto endofperl
	\@rem ';
EOT
my $headlines = 2 + ($head =~ tr/\n/\n/);
my $tail = "__END__\n:endofperl\n";

@ARGV = ('-') unless @ARGV;

process(@ARGV);

sub process {
   LOOP:
    foreach ( @_ ) {
    	my $myhead = $head;
    	my $linedone = 0;
	my $linenum = $headlines;
	my $line;
        open( FILE, $_ ) or die "$0: Can't open $_: $!";
        @file = <FILE>;
        foreach $line ( @file ) {
	    $linenum++;
            if ( $line =~ /^:endofperl/) {
                warn "$0: $_ has already been converted to a batch file!\n";
                next LOOP;
	    }
	    if ( not $linedone and $line =~ /^#!.*perl/ ) {
		$line .= "#line $linenum\n";
		$linedone++;
	    }
        }
        close( FILE );
        s/$OPT{'s'}$//oi;
        $_ .= '.bat' unless /\.bat$/i or /^-$/;
        open( FILE, ">$_" ) or die "Can't open $_: $!";
	print FILE $myhead;
	print FILE "#!perl\n#line " . ($headlines+1) . "\n" unless $linedone;
        print FILE @file, $tail;
        close( FILE );
    }
}
__END__

=head1 NAME

pl2bat - wrap perl code into a batch file

=head1 SYNOPSIS

B<pl2bat> [B<-h>] S<[B<-a> I<argstring>]> S<[B<-s> I<stripsuffix>]> [files]

=head1 DESCRIPTION

This utility converts a perl script into a batch file that can be
executed on DOS-like operating systems.

Note that by default, the ".pl" suffix will be stripped before adding
a ".bat" suffix to the supplied file names.  This can be controlled
with the C<-s> option.

The default behavior on WindowsNT is to generate a batch file that
uses the C<%*> construct to refer to all the command line arguments
that were given to it, so you'll need to make sure that works on your
variant of the command shell.  It is known to work in the cmd.exe shell
under WindowsNT.  4DOS/NT users will want to put a C<ParameterChar = *>
line in their initialization file, or execute C<setdos /p*> in
the shell startup file.  On Windows95 and other platforms a nine
argument limit is imposed on command-line arguments given to the
generated batch file, since they may not support C<%*> in batch files.
This can be overridden using the C<-a> option.

=head1 OPTIONS

=over 8

=item B<-a> I<argstring>

Arguments to invoke perl with in generated batch file.  Defaults to
S<"-x -S %0 %*"> on WindowsNT, S<"-x -S %0 %1 %2 %3 %4 %5 %6 %7 %8 %9">
on other platforms.

=item B<-s> I<stripsuffix>

Strip a suffix string from file name before appending a ".bat"
suffix.  The suffix is not case-sensitive.  It can be a regex if it
begins with `/' (the trailing '/' being optional.  Defaults to ".pl".

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

=head1 BUGS

C<$0> will contain the full name, including the ".bat" suffix
when the generated batch file runs.  If you don't like this,
see runperl.bat for an alternative way to invoke perl scripts.

Default behavior is to invoke Perl with the -S flag, so Perl will
search the PATH to find the script.  This may have undesirable
effects.

=head1 SEE ALSO

perl, perlwin32, runperl.bat

=cut

