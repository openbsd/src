
package Text::Wrap;

#
# This is a very simple paragraph formatter.  It formats one 
# paragraph at a time by wrapping and indenting text.
#
# Usage:
#
#	use Text::Wrap;
#
#	print wrap($initial_tab,$subsequent_tab,@text);
#
# You can also set the number of columns to wrap before:
#
#	$Text::Wrap::columns = 135; # <= width of screen
#
#	use Text::Wrap qw(wrap $columns); 
#	$columns = 70;
#	
#
# The first line will be printed with $initial_tab prepended.  All
# following lines will have $subsequent_tab prepended.
#
# Example:
#
#	print wrap("\t","","This is a bit of text that ...");
#
# David Muir Sharnoff <muir@idiom.com>
# Version: 9/21/95
#

=head1 NAME

Text::Wrap -- wrap text into a paragraph

=head1 SYNOPSIS

	use Text::Wrap;
	
	$Text::Wrap::columns = 20; # Default
	print wrap("\t","",Hello, world, it's a nice day, isn't it?");

=head1 DESCRIPTION

This module is a simple paragraph formatter that wraps text into a paragraph
and indents each line. The single exported function, wrap(), takes three
arguments. The first is included before the first output line, and the
second argument is included before each subsequest output line. The third
argument is the text to be wrapped.

=head1 AUTHOR

David Muir Sharnoff <muir@idiom.com>

=cut

require Exporter;

@ISA = (Exporter);
@EXPORT = qw(wrap);
@EXPORT_OK = qw($columns);

BEGIN	{
	$Text::Wrap::columns = 76;  # <= screen width
}

use Text::Tabs;
use strict;

sub wrap
{
	my ($ip, $xp, @t) = @_;

	my $r;
	my $t = expand(join(" ",@t));
	my $lead = $ip;
	my $ll = $Text::Wrap::columns - length(expand($lead)) - 1;
	if ($t =~ s/^([^\n]{0,$ll})\s//) {
		$r .= unexpand($lead . $1 . "\n");
		$lead = $xp;
		my $ll = $Text::Wrap::columns - length(expand($lead)) - 1;
		while ($t =~ s/^([^\n]{0,$ll})\s//) {
			$r .= unexpand($lead . $1 . "\n");
		}
	} 
	die "couldn't wrap '$t'" 
		if length($t) > $ll;
	$r .= $t;
	return $r;
}

1;
