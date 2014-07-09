#!/usr/bin/perl -w
# $LynxId: tbl2html.pl,v 1.5 2011/05/21 15:18:16 tom Exp $
#
# Translate one or more ".tbl" files into ".html" files which can be used to
# test the charset support in lynx.  Each of the ".html" files will use the
# charset that corresponds to the input ".tbl" file.

use strict;

use Getopt::Std;
use File::Basename;
use POSIX qw(strtod);

sub field($$) {
	my $value = $_[0];
	my $count = $_[1];

	while ( $count > 0 ) {
		$count -= 1;
		$value =~ s/^\S*\s*//;
	}
	$value =~ s/\s.*//;
	return $value;
}

sub notes($) {
	my $value = $_[0];

	$value =~ s/^[^#]*//;
	$value =~ s/^#//;
	$value =~ s/^\s+//;

	return $value;
}

sub make_header($$$) {
	my $source   = $_[0];
	my $charset  = $_[1];
	my $official = $_[2];

	printf FP "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n";
	printf FP "<HTML>\n";
	printf FP "<HEAD>\n";
	printf FP "<!-- $source -->\n";
	printf FP "<TITLE>%s table</TITLE>\n", &escaped($official);
	printf FP "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=%s\">\n", &escaped($charset);
	printf FP "</HEAD>\n";
	printf FP "\n";
	printf FP "<BODY> \n";
	printf FP "\n";
	printf FP "<H1 ALIGN=center>%s table</H1> \n", &escaped($charset);
	printf FP "\n";
	printf FP "<PRE>\n";
	printf FP "Code  Char  Entity   Render          Description\n";
}

sub make_mark() {
	printf FP "----  ----  ------   ------          -----------------------------------\n";
}

sub escaped($) {
	my $result = $_[0];
	$result =~ s/&/&amp;/g;
	$result =~ s/</&lt;/g;
	$result =~ s/>/&gt;/g;
	return $result;
}

sub make_row($$$) {
	my $old_code = $_[0];
	my $new_code = $_[1];
	my $comments = $_[2];

	# printf "# make_row %d %d %s\n", $old_code, $new_code, $comments;
	my $visible = sprintf("&amp;#%d;      ", $new_code);
	if ($old_code < 256) {
		printf FP "%4x    %c   %.13s  &#%d;             %s\n",
			$old_code, $old_code,
			$visible, $new_code,
			&escaped($comments);
	} else {
		printf FP "%4x    .   %.13s  &#%d;             %s\n",
			$old_code,
			$visible, $new_code,
			&escaped($comments);
	}
}

sub null_row($$) {
	my $old_code = $_[0];
	my $comments = $_[1];

	if ($old_code < 256) {
		printf FP "%4x    %c                     %s\n",
			$old_code, $old_code,
			&escaped($comments);
	} else {
		printf FP "%4x    .                     %s\n",
			$old_code,
			&escaped($comments);
	}
}

sub make_footer() {
	printf FP "</PRE>\n";
	printf FP "</BODY>\n";
	printf FP "</HTML>\n";
}

# return true if the string describes a range
sub is_range($) {
	return ($_[0] =~ /.*-.*/);
}

# convert the U+'s to 0x's so strtod() can convert them.
sub zeroxes($) {
	my $result = $_[0];
	$result =~ s/^U\+/0x/;
	$result =~ s/-U\+/-0x/;
	return $result;
}

# convert a string to a number (-1's are outside the range of Unicode).
sub value_of($) {
	my ($result, $oops) = strtod($_[0]);
	$result = -1 if ($oops ne 0);
	return $result;
}

# return the first number in a range
sub first_of($) {
	my $range = &zeroxes($_[0]);
	$range =~ s/-.*//;
	return &value_of($range);
}

# return the last number in a range
sub last_of($) {
	my $range = &zeroxes($_[0]);
	$range =~ s/^.*-//;
	return &value_of($range);
}

sub one_many($$$) {
	my $oldcode = $_[0];
	my $newcode = &zeroxes($_[1]);
	my $comment = $_[2];

	my $old_code = &value_of($oldcode);
	if ( $old_code lt 0 ) {
		printf "? Problem with number \"%s\"\n", $oldcode;
	} else {
		&make_mark if (( $old_code % 8 ) == 0 );

		if ( $newcode =~ /^#.*/ ) {
			&null_row($old_code, $comment);
		} elsif ( &is_range($newcode) ) {
			my $first_item = &first_of($newcode);
			my $last_item  = &last_of($newcode);
			my $item;

			if ( $first_item lt 0 or $last_item lt 0 ) {
				printf "? Problem with one:many numbers \"%s\"\n", $newcode;
			} else {
				if ( $comment =~ /^$/ ) {
					$comment = sprintf("mapped: %#x to %#x..%#x", $old_code, $first_item, $last_item);
				} else {
					$comment = $comment . " (range)";
				}
				for $item ( $first_item..$last_item) {
					&make_row($old_code, $item, $comment);
				}
			}
		} else {
			my $new_code = &value_of($newcode);
			if ( $new_code lt 0 ) {
				printf "? Problem with number \"%s\"\n", $newcode;
			} else {
				if ( $comment =~ /^$/ ) {
					$comment = sprintf("mapped: %#x to %#x", $old_code, $new_code);
				}
				&make_row($old_code, $new_code, $comment);
			}
		}
	}
}

sub many_many($$$) {
	my $oldcode = $_[0];
	my $newcode = $_[1];
	my $comment = $_[2];

	my $first_old = &first_of($oldcode);
	my $last_old  = &last_of($oldcode);
	my $item;

	if (&is_range($newcode)) {
		my $first_new = &first_of($newcode);
		my $last_new  = &last_of($newcode);
		for $item ( $first_old..$last_old) {
			&one_many($item, $first_new, $comment);
			$first_new += 1;
		}
	} else {
		for $item ( $first_old..$last_old) {
			&one_many($item, $newcode, $comment);
		}
	}
}

sub approximate($$$) {
	my $values = $_[0];
	my $expect = sprintf("%-8s", $_[1]);
	my $comment = $_[2];
	my $escaped = &escaped($expect);
	my $left;
	my $this;
	my $next;

	$escaped =~ s/\\134/\\/g;
	$escaped =~ s/\\015/\&#13\;/g;
	$escaped =~ s/\\012/\&#10\;/g;

	while ( $escaped =~ /^.*\\[0-7]{3}.*$/ ) {
		$left = $escaped;
		$left =~ s/\\[0-7]{3}.*//;
		$this = substr $escaped,length($left)+1,3;
		$next = substr $escaped,length($left)+4;
		$escaped = sprintf("%s&#%d;%s", $left, oct $this, $next);
	}

	my $visible = sprintf("&amp;#%d;      ", $values);
	if ($values < 256) {
		printf FP "%4x    %c   %.13s  &#%d;             approx: %s\n",
			$values, $values,
			$visible,
			$values,
			$escaped;
	} else {
		printf FP "%4x    .   %.13s  &#%d;             approx: %s\n",
			$values,
			$visible,
			$values,
			$escaped;
	}
}

sub doit($) {
	my $source = $_[0];

	printf "** %s\n", $source;

	my $target = basename($source, ".tbl");

	# Read the file into an array in memory.
	open(FP,$source) || do {
		print STDERR "Can't open input $source: $!\n";
		return;
	};
	my (@input) = <FP>;
	chomp @input;
	close(FP);

	my $n;
	my $charset = "";
	my $official = "";
	my $empty = 1;

	for $n (0..$#input) {
		$input[$n] =~ s/\s*$//; # trim trailing blanks
		$input[$n] =~ s/^\s*//; # trim leading blanks
		$input[$n] =~ s/^#0x/0x/; # uncomment redundant stuff

		next if $input[$n] =~ /^$/;
		next if $input[$n] =~ /^#.*$/;

		if ( $empty 
		  and ( $input[$n] =~ /^\d/
		     or $input[$n] =~ /^U\+/ ) ) {
			$target = $charset . ".html";
			printf "=> %s\n", $target;
			open(FP,">$target") || do {
				print STDERR "Can't open output $target: $!\n";
				return;
			};
			&make_header($source, $charset, $official);
			$empty = 0;
		}

		if ( $input[$n] =~ /^M.*/ ) {
			$charset = $input[$n];
			$charset =~ s/^.//;
		} elsif ( $input[$n] =~ /^O.*/ ) {
			$official = $input[$n];
			$official =~ s/^.//;
		} elsif ( $input[$n] =~ /^\d/ ) {

			my $newcode = &field($input[$n], 1);

			next if ( $newcode eq "idem" );
			next if ( $newcode eq "" );

			my $oldcode = &field($input[$n], 0);
			if ( &is_range($oldcode) ) {
				&many_many($oldcode, $newcode, &notes($input[$n]));
			} else {
				&one_many($oldcode, $newcode, &notes($input[$n]));
			}
		} elsif ( $input[$n] =~ /^U\+/ ) {
			if ( $input[$n] =~ /^U\+\w+:/ ) {
				my $values = $input[$n];
				my $expect = $input[$n];

				$values =~ s/:.*//;
				$values = &zeroxes($values);
				$expect =~ s/^[^:]+://;

				if ( &is_range($values) ) {
					printf "fixme:%s(%s)(%s)\n", $input[$n], $values, $expect;
				} else {
					&approximate(&value_of($values), $expect, &notes($input[$n]));
				}
			} else {
				my $value = $input[$n];
				$value =~ s/\s*".*//;
				$value = &value_of(&zeroxes($value));
				if ($value gt 0) {
					my $quote = $input[$n];
					my $comment = &notes($input[$n]);
					$quote =~ s/^[^"]*"//;
					$quote =~ s/".*//;
					&approximate($value, $quote, $comment);
				} else {
					printf "fixme:%d(%s)\n", $n, $input[$n];
				}
			}
		} else {
			# printf "skipping line %d:%s\n", $n + 1, $input[$n];
		}
	}
	if ( ! $empty ) {
		&make_footer();
	}
	close FP;
}

sub usage() {
	print <<USAGE;
Usage: $0 [tbl-files]

The script writes a new ".html" file for each input, using
the same name as the input, stripping the ".tbl" suffix.
USAGE
	exit(1);
}

if ( $#ARGV < 0 ) {
	usage();
} else {
	while ( $#ARGV >= 0 ) {
		&doit ( shift @ARGV );
	}
}
exit (0);
