#!/usr/local/bin/perl
#	$OpenBSD$
#
# This is slightly hacked up in order to support DESTDIR.
#

use Config;
use File::Basename qw(&basename &dirname);

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.
# Wanted:  $archlibexp

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

'di ';
'ds 00 \"';
'ig 00 ';

\$perlincl = "$Config{archlibexp}";

!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

$destdir = $ENV{"DESTDIR"};
if ($destdir ne '')  {
    chdir "$destdir/usr/include" || die "Can't cd $destdir/usr/include";
    # whack $perlincl
    $perlincl = "$destdir/$perlincl";
} else {
    chdir '/usr/include' || die "Can't cd /usr/include";
}

@isatype = split(' ',<<END);
	char	uchar	u_char
	short	ushort	u_short
	int	uint	u_int
	long	ulong	u_long
	FILE
END

@isatype{@isatype} = (1) x @isatype;
$inif = 0;

@ARGV = ('-') unless @ARGV;

foreach $file (@ARGV) {
    if ($file eq '-') {
	open(IN, "-");
	open(OUT, ">-");
    }
    else {
	($outfile = $file) =~ s/\.h$/.ph/ || next;
	print "$file -> $outfile\n";
	if ($file =~ m|^(.*)/|) {
	    $dir = $1;
	    if (!-d "$perlincl/$dir") {
		mkdir("$perlincl/$dir",0777);
	    }
	}
	open(IN,"$file") || ((warn "Can't open $file: $!\n"),next);
	open(OUT,">$perlincl/$outfile") || die "Can't create $outfile: $!\n";
    }
    while (<IN>) {
	chop;
	while (/\\$/) {
	    chop;
	    $_ .= <IN>;
	    chop;
	}
	if (s:/\*:\200:g) {
	    s:\*/:\201:g;
	    s/\200[^\201]*\201//g;	# delete single line comments
	    if (s/\200.*//) {		# begin multi-line comment?
		$_ .= '/*';
		$_ .= <IN>;
		redo;
	    }
	}
	if (s/^#\s*//) {
	    if (s/^define\s+(\w+)//) {
		$name = $1;
		$new = '';
		s/\s+$//;
		if (s/^\(([\w,\s]*)\)//) {
		    $args = $1;
		    if ($args ne '') {
			foreach $arg (split(/,\s*/,$args)) {
			    $arg =~ s/^\s*([^\s].*[^\s])\s*$/$1/;
			    $curargs{$arg} = 1;
			}
			$args =~ s/\b(\w)/\$$1/g;
			$args = "local($args) = \@_;\n$t    ";
		    }
		    s/^\s+//;
		    do expr();
		    $new =~ s/(["\\])/\\$1/g;
		    if ($t ne '') {
			$new =~ s/(['\\])/\\$1/g;
			print OUT $t,
			  "eval 'sub $name {\n$t    ${args}eval \"$new\";\n$t}';\n";
		    }
		    else {
			print OUT "sub $name {\n    ${args}eval \"$new\";\n}\n";
		    }
		    %curargs = ();
		}
		else {
		    s/^\s+//;
		    do expr();
		    $new = 1 if $new eq '';
		    if ($t ne '') {
			$new =~ s/(['\\])/\\$1/g;
			print OUT $t,"eval 'sub $name {",$new,";}';\n";
		    }
		    else {
			print OUT $t,"sub $name {",$new,";}\n";
		    }
		}
	    }
	    elsif (/^include\s*<(.*)>/) {
		($incl = $1) =~ s/\.h$/.ph/;
		print OUT $t,"require '$incl';\n";
	    }
	    elsif (/^ifdef\s+(\w+)/) {
		print OUT $t,"if (defined &$1) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    }
	    elsif (/^ifndef\s+(\w+)/) {
		print OUT $t,"if (!defined &$1) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    }
	    elsif (s/^if\s+//) {
		$new = '';
		$inif = 1;
		do expr();
		$inif = 0;
		print OUT $t,"if ($new) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    }
	    elsif (s/^elif\s+//) {
		$new = '';
		$inif = 1;
		do expr();
		$inif = 0;
		$tab -= 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT $t,"}\n${t}elsif ($new) {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    }
	    elsif (/^else/) {
		$tab -= 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT $t,"}\n${t}else {\n";
		$tab += 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
	    }
	    elsif (/^endif/) {
		$tab -= 4;
		$t = "\t" x ($tab / 8) . ' ' x ($tab % 8);
		print OUT $t,"}\n";
	    }
	}
    }
    print OUT "1;\n";
}

sub expr {
    while ($_ ne '') {
	s/^(\s+)//		&& do {$new .= ' '; next;};
	s/^(0x[0-9a-fA-F]+)//	&& do {$new .= $1; next;};
	s/^(\d+)[LlUu]*//	&& do {$new .= $1; next;};
	s/^("(\\"|[^"])*")//	&& do {$new .= $1; next;};
	s/^'((\\"|[^"])*)'//	&& do {
	    if ($curargs{$1}) {
		$new .= "ord('\$$1')";
	    }
	    else {
		$new .= "ord('$1')";
	    }
	    next;
	};
	s/^sizeof\s*\(([^)]+)\)/{$1}/ && do {
	    $new .= '$sizeof';
	    next;
	};
	s/^([_a-zA-Z]\w*)//	&& do {
	    $id = $1;
	    if ($id eq 'struct') {
		s/^\s+(\w+)//;
		$id .= ' ' . $1;
		$isatype{$id} = 1;
	    }
	    elsif ($id eq 'unsigned') {
		s/^\s+(\w+)//;
		$id .= ' ' . $1;
		$isatype{$id} = 1;
	    }
	    if ($curargs{$id}) {
		$new .= '$' . $id;
	    }
	    elsif ($id eq 'defined') {
		$new .= 'defined';
	    }
	    elsif (/^\(/) {
		s/^\((\w),/("$1",/ if $id =~ /^_IO[WR]*$/i;	# cheat
		$new .= " &$id";
	    }
	    elsif ($isatype{$id}) {
		if ($new =~ /{\s*$/) {
		    $new .= "'$id'";
		}
		elsif ($new =~ /\(\s*$/ && /^[\s*]*\)/) {
		    $new =~ s/\(\s*$//;
		    s/^[\s*]*\)//;
		}
		else {
		    $new .= q(').$id.q(');
		}
	    }
	    else {
		if ($inif && $new !~ /defined\s*\($/) {
		    $new .= '(defined(&' . $id . ') ? &' . $id . ' : 0)';
		} 
		elsif (/^\[/) { 
		    $new .= ' $' . $id;
		}
		else {
		    $new .= ' &' . $id;
		}
	    }
	    next;
	};
	s/^(.)// && do { if ($1 ne '#') { $new .= $1; } next;};
    }
}
##############################################################################

	# These next few lines are legal in both Perl and nroff.

.00 ;			# finish .ig

'di			\" finish diversion--previous line must be blank
.nr nl 0-1		\" fake up transition to first page again
.nr % 0			\" start at page 1
'; __END__ ############# From here on it's a standard manual page ############
.TH H2PH 1 "August 8, 1990"
.AT 3
.SH NAME
h2ph \- convert .h C header files to .ph Perl header files
.SH SYNOPSIS
.B h2ph [headerfiles]
.SH DESCRIPTION
.I h2ph
converts any C header files specified to the corresponding Perl header file
format.
It is most easily run while in /usr/include:
.nf

	cd /usr/include; h2ph * sys/*

.fi
If run with no arguments, filters standard input to standard output.
.SH ENVIRONMENT
No environment variables are used.
.SH FILES
/usr/include/*.h
.br
/usr/include/sys/*.h
.br
etc.
.SH AUTHOR
Larry Wall
.SH "SEE ALSO"
perl(1)
.SH DIAGNOSTICS
The usual warnings if it can't read or write the files involved.
.SH BUGS
Doesn't construct the %sizeof array for you.
.PP
It doesn't handle all C constructs, but it does attempt to isolate
definitions inside evals so that you can get at the definitions
that it can translate.
.PP
It's only intended as a rough tool.
You may need to dicker with the files produced.
.ex
!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
