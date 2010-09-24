;# This legacy library is deprecated and will be removed in a future
;# release of perl.
;#
;# shellwords.pl
;#
;# Usage:
;#	require 'shellwords.pl';
;#	@words = shellwords($line);
;#	or
;#	@words = shellwords(@lines);
;#	or
;#	@words = shellwords();		# defaults to $_ (and clobbers it)

require Text::ParseWords;
*shellwords = \&Text::ParseWords::old_shellwords;

1;
