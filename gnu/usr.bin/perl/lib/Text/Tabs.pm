#
# expand and unexpand tabs as per the unix expand and 
# unexpand programs.
#
# expand and unexpand operate on arrays of lines.  Do not
# feed strings that contain newlines to them.
#
# David Muir Sharnoff <muir@idiom.com>
# 
# Version: 9/21/95
#

=head1 NAME

Text::Tabs -- expand and unexpand tabs

=head1 SYNOPSIS

	use Text::Tabs;
	
	#$tabstop = 8; # Defaults
	print expand("Hello\tworld");
	print unexpand("Hello,        world");
	$tabstop = 4;
	print join("\n",expand(split(/\n/,
		"Hello\tworld,\nit's a nice day.\n"
		)));

=head1 DESCRIPTION

This module expands and unexpands tabs into spaces, as per the unix expand
and unexpand programs. Either function should be passed an array of strings
(newlines may I<not> be included, and should be used to split an incoming
string into separate elements.) which will be processed and returned.

=head1 AUTHOR

David Muir Sharnoff <muir@idiom.com>

=cut

package Text::Tabs;

require Exporter;

@ISA = (Exporter);
@EXPORT = qw(expand unexpand $tabstop);

$tabstop = 8;

sub expand
{
	my @l = @_;
	for $_ (@l) {
		1 while s/^([^\t]*)(\t+)/
			$1 . (" " x 
				($tabstop * length($2)
				- (length($1) % $tabstop)))
			/e;
	}
	return @l if wantarray;
	return @l[0];
}

sub unexpand
{
	my @l = &expand(@_);
	my @e;
	for $x (@l) {
		@e = split(/(.{$tabstop})/,$x);
		for $_ (@e) {
			s/  +$/\t/;
		}
		$x = join('',@e);
	}
	return @l if wantarray;
	return @l[0];
}

1;
