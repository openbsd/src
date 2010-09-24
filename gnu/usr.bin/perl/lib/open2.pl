# This legacy library is deprecated and will be removed in a future
# release of perl.
#
# This is a compatibility interface to IPC::Open2.  New programs should
# do
#
#     use IPC::Open2;
#
# instead of
#
#     require 'open2.pl';

package main;
use IPC::Open2 'open2';
1
