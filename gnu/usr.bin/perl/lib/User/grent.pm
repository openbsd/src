package User::grent 1.05;
use v5.38;

our ($gr_name, $gr_gid, $gr_passwd, @gr_members);

use Exporter 'import';
our @EXPORT      = qw(getgrent getgrgid getgrnam getgr);
our @EXPORT_OK   = qw($gr_name $gr_gid $gr_passwd @gr_members);
our %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );

use Class::Struct qw(struct);
struct 'User::grent' => [
    name    => '$',
    passwd  => '$',
    gid	    => '$',
    members => '@',
];

sub populate {
    return unless @_;
    my $gob = new();
    ($gr_name, $gr_passwd, $gr_gid) = @$gob[0,1,2] = @_[0,1,2];
    @gr_members = @{$gob->[3]} = split ' ', $_[3];
    return $gob;
} 

sub getgrent :prototype( ) { populate(CORE::getgrent()) }
sub getgrnam :prototype($) { populate(CORE::getgrnam(shift)) }
sub getgrgid :prototype($) { populate(CORE::getgrgid(shift)) }
sub getgr    :prototype($) { ($_[0] =~ /^\d+/) ? &getgrgid : &getgrnam }

__END__

=head1 NAME

User::grent - by-name interface to Perl's built-in getgr*() functions

=head1 SYNOPSIS

 use User::grent;
 my $gr = getgrgid(0) or die "No group zero";
 if ( $gr->name eq 'wheel' && @{$gr->members} > 1 ) {
     print "gid zero name wheel, with other members";
 } 

 use User::grent qw(:FIELDS);
 getgrgid(0) or die "No group zero";
 if ( $gr_name eq 'wheel' && @gr_members > 1 ) {
     print "gid zero name wheel, with other members";
 } 

 my $gr = getgr($whoever);

=head1 DESCRIPTION

This module's default exports override the core getgrent(), getgrgid(),
and getgrnam() functions, replacing them with versions that return
"User::grent" objects.  This object has methods that return the similarly
named structure field name from the C's passwd structure from F<grp.h>; 
namely name, passwd, gid, and members (not mem).  The first three
return scalars, the last an array reference.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your core functions.)  Access these fields as variables named
with a preceding C<gr_>.  Thus, C<$group_obj-E<gt>gid()> corresponds
to $gr_gid if you import the fields.  Array references are available as
regular array variables, so C<@{ $group_obj-E<gt>members() }> would be
simply @gr_members.

The getgr() function is a simple front-end that forwards a numeric
argument to getgrgid() and the rest to getgrnam().

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
