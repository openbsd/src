#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    eval {my @n = getpwuid 0; setpwent()};
    if ($@ && $@ =~ /(The \w+ function is unimplemented)/) {
	print "1..0 # Skip: $1\n";
	exit 0;
    }
    eval { require Config; import Config; };
    my $reason;
    if ($Config{'i_pwd'} ne 'define') {
	$reason = '$Config{i_pwd} undefined';
    }
    elsif (not -f "/etc/passwd" ) { # Play safe.
	$reason = 'no /etc/passwd file';
    }

    if (not defined $where) {	# Try NIS.
	foreach my $ypcat (qw(/usr/bin/ypcat /bin/ypcat /etc/ypcat)) {
	    if (-x $ypcat &&
		open(PW, "$ypcat passwd 2>/dev/null |") &&
		defined(<PW>)) {
		$where = "NIS passwd";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where) {	# Try NetInfo.
	foreach my $nidump (qw(/usr/bin/nidump)) {
	    if (-x $nidump &&
		open(PW, "$nidump passwd . 2>/dev/null |") &&
		defined(<PW>)) {
		$where = "NetInfo passwd";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where &&		# Try dscl
	$Config{useperlio} eq 'define') {	# need perlio

	# Map dscl items to passwd fields, and provide support for
	# mucking with the dscl output if we need to (and we do).
	my %want = do {
	    my $inx = 0;
	    map {$_ => {inx => $inx++, mung => sub {$_[0]}}}
		qw{RecordName Password UniqueID PrimaryGroupID
		RealName NFSHomeDirectory UserShell};
	};

	# The RecordName for a /User record is the username. In some
	# cases there are synonyms (e.g. _www and www), in which case we
	# get a blank-delimited list. We prefer the first entry in the
	# list because getpwnam() does.
	$want{RecordName}{mung} = sub {(split '\s+', $_[0], 2)[0]};

	# The UniqueID and PrimaryGroupID for a /User record are the
	# user ID and the primary group ID respectively. In cases where
	# the high bit is set, 'dscl' returns a negative number, whereas
	# getpwnam() returns its twos complement. This mungs the dscl
	# output to agree with what getpwnam() produces. Interestingly
	# enough, getpwuid(-2) returns the right record ('nobody'), even
	# though it returns the uid as 4294967294. If you track uid_t
	# on an i386, you find it is an unsigned int, which makes the
	# unsigned version the right one; but both /etc/passwd and
	# /etc/master.passwd contain negative numbers.
	$want{UniqueID}{mung} = $want{PrimaryGroupID}{mung} = sub {
	    unpack 'L', pack 'l', $_[0]};

	foreach my $dscl (qw(/usr/bin/dscl)) {
	    -x $dscl or next;
	    open (my $fh, '-|', join (' ', $dscl, qw{. -readall /Users},
		    keys %want, '2>/dev/null')) or next;
	    my $data;
	    my @rec;
	    while (<$fh>) {
		chomp;
		if ($_ eq '-') {
		    @rec and $data .= join (':', @rec) . "\n";
		    @rec = ();
		    next;
		}
		my ($name, $value) = split ':\s+', $_, 2;
		unless (defined $value) {
		    s/:$//;
		    $name = $_;
		    $value = <$fh>;
		    chomp $value;
		    $value =~ s/^\s+//;
		}
		if (defined (my $info = $want{$name})) {
		    $rec[$info->{inx}] = $info->{mung}->($value);
		}
	    }
	    @rec and $data .= join (':', @rec) . "\n";
	    if (open (PW, '<', \$data)) {
		$where = "dscl . -readall /Users";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where) {	# Try local.
	my $PW = "/etc/passwd";
	if (-f $PW && open(PW, $PW) && defined(<PW>)) {
	    $where = $PW;
	    undef $reason;
	}
    }

    if (not defined $where) {      # Try NIS+
     foreach my $niscat (qw(/bin/niscat)) {
         if (-x $niscat &&
           open(PW, "$niscat passwd.org_dir 2>/dev/null |") &&
           defined(<PW>)) {
           $where = "NIS+ $niscat passwd.org_dir";
           undef $reason;
           last;
         }
     }
    }

    if ($reason) {	# Give up.
	print "1..0 # Skip: $reason\n";
	exit 0;
    }
}

# By now the PW filehandle should be open and full of juicy password entries.

print "1..2\n";

# Go through at most this many users.
# (note that the first entry has been read away by now)
my $max = 25;

my $n = 0;
my $tst = 1;
my %perfect;
my %seen;

print "# where $where\n";

setpwent();

while (<PW>) {
    chomp;
    # LIMIT -1 so that users with empty shells don't fall off
    my @s = split /:/, $_, -1;
    my ($name_s, $passwd_s, $uid_s, $gid_s, $gcos_s, $home_s, $shell_s);
    (my $v) = $Config{osvers} =~ /^(\d+)/;
    if ($^O eq 'darwin' && $v < 9) {
       ($name_s, $passwd_s, $uid_s, $gid_s, $gcos_s, $home_s, $shell_s) = @s[0,1,2,3,7,8,9];
    } else {
       ($name_s, $passwd_s, $uid_s, $gid_s, $gcos_s, $home_s, $shell_s) = @s;
    }
    next if /^\+/; # ignore NIS includes
    if (@s) {
	push @{ $seen{$name_s} }, $.;
    } else {
	warn "# Your $where line $. is empty.\n";
	next;
    }
    if ($n == $max) {
	local $/;
	my $junk = <PW>;
	last;
    }
    # In principle we could whine if @s != 7 but do we know enough
    # of passwd file formats everywhere?
    if (@s == 7 || ($^O eq 'darwin' && @s == 10)) {
	@n = getpwuid($uid_s);
	# 'nobody' et al.
	next unless @n;
	my ($name,$passwd,$uid,$gid,$quota,$comment,$gcos,$home,$shell) = @n;
	# Protect against one-to-many and many-to-one mappings.
	if ($name_s ne $name) {
	    @n = getpwnam($name_s);
	    ($name,$passwd,$uid,$gid,$quota,$comment,$gcos,$home,$shell) = @n;
	    next if $name_s ne $name;
	}
	$perfect{$name_s}++
	    if $name    eq $name_s    and
               $uid     eq $uid_s     and
# Do not compare passwords: think shadow passwords.
               $gid     eq $gid_s     and
               $gcos    eq $gcos_s    and
               $home    eq $home_s    and
               $shell   eq $shell_s;
    }
    $n++;
}

endpwent();

print "# max = $max, n = $n, perfect = ", scalar keys %perfect, "\n";

if (keys %perfect == 0 && $n) {
    $max++;
    print <<EOEX;
#
# The failure of op/pwent test is not necessarily serious.
# It may fail due to local password administration conventions.
# If you are for example using both NIS and local passwords,
# test failure is possible.  Any distributed password scheme
# can cause such failures.
#
# What the pwent test is doing is that it compares the $max first
# entries of $where
# with the results of getpwuid() and getpwnam() call.  If it finds no
# matches at all, it suspects something is wrong.
# 
EOEX
    print "not ";
    $not = 1;
} else {
    $not = 0;
}
print "ok ", $tst++;
print "\t# (not necessarily serious: run t/op/pwent.t by itself)" if $not;
print "\n";

# Test both the scalar and list contexts.

my @pw1;

setpwent();
for (1..$max) {
    my $pw = scalar getpwent();
    last unless defined $pw;
    push @pw1, $pw;
}
endpwent();

my @pw2;

setpwent();
for (1..$max) {
    my ($pw) = (getpwent());
    last unless defined $pw;
    push @pw2, $pw;
}
endpwent();

print "not " unless "@pw1" eq "@pw2";
print "ok ", $tst++, "\n";

close(PW);
