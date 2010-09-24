use Carp;
use Switch qw(__ fallthrough);

my($C,$M);sub ok{$C++;$M.=$_[0]?"ok $C\n":"not ok $C (line ".(caller)[2].")\n"}
END{print"1..$C\n$M"}

# NON-case THINGS;

$case->{case} = { case => "case" };

*case = \&case;

# PREMATURE case

eval { case 1 { ok(0) }; ok(0) } || ok(1);

# H.O. FUNCS

switch (__ > 2) {

	case 1	{ ok(0) } else { ok(1) }
	case 2	{ ok(0) } else { ok(1) }
	case 3	{ ok(1) } else { ok(0) }
}

switch (3) {

	eval { case __ <= 1 || __ > 2	{ ok(0) } } || ok(1);
	case __ <= 2 		{ ok(0) };
	case __ <= 3		{ ok(1) };
}

# POSSIBLE ARGS: NUMERIC, STRING, ARRAY, HASH, REGEX, CODE

# 1. NUMERIC SWITCH

for (1..3)
{
	switch ($_) {
		# SELF
		case ($_) { ok(1) } else { ok(0) }

		# NUMERIC
		case (1) { ok ($_==1) } else { ok($_!=1) }
		case  1  { ok ($_==1) } else { ok($_!=1) }
		case (3) { ok ($_==3) } else { ok($_!=3) }
		case (4) { ok (0) } else { ok(1) }
		case (2) { ok ($_==2) } else { ok($_!=2) }

		# STRING
		case ('a') { ok (0) } else { ok(1) }
		case  'a'  { ok (0) } else { ok(1) }
		case ('3') { ok ($_ == 3) } else { ok($_ != 3) }
		case ('3.0') { ok (0) } else { ok(1) }

		# ARRAY
		case ([10,5,1]) { ok ($_==1) } else { ok($_!=1) }
		case  [10,5,1]  { ok ($_==1) } else { ok($_!=1) }
		case (['a','b']) { ok (0) } else { ok(1) }
		case (['a','b',3]) { ok ($_==3) } else { ok ($_!=3) }
		case (['a','b',2.0]) { ok ($_==2) } else { ok ($_!=2) }
		case ([]) { ok (0) } else { ok(1) }

		# HASH
		case ({}) { ok (0) } else { ok (1) }
		case {} { ok (0) } else { ok (1) }
		case {1,1} { ok ($_==1) } else { ok($_!=1) }
		case ({1=>1, 2=>0}) { ok ($_==1) } else { ok($_!=1) }

		# SUB/BLOCK
		case (sub {$_[0]==2}) { ok ($_==2) } else { ok($_!=2) }
		case {$_[0]==2} { ok ($_==2) } else { ok($_!=2) }
		case {0} { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		case {1} { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 2. STRING SWITCH

for ('a'..'c','1')
{
	switch ($_) {
		# SELF
		case ($_) { ok(1) } else { ok(0) }

		# NUMERIC
		case (1)  { ok ($_ !~ /[a-c]/) } else { ok ($_ =~ /[a-c]/) }
		case (1.0) { ok ($_ !~ /[a-c]/) } else { ok ($_ =~ /[a-c]/) }

		# STRING
		case ('a') { ok ($_ eq 'a') } else { ok($_ ne 'a') }
		case ('b') { ok ($_ eq 'b') } else { ok($_ ne 'b') }
		case ('c') { ok ($_ eq 'c') } else { ok($_ ne 'c') }
		case ('1') { ok ($_ eq '1') } else { ok($_ ne '1') }
		case ('d') { ok (0) } else { ok (1) }

		# ARRAY
		case (['a','1']) { ok ($_ eq 'a' || $_ eq '1') }
			else { ok ($_ ne 'a' && $_ ne '1') }
		case (['z','2']) { ok (0) } else { ok(1) }
		case ([]) { ok (0) } else { ok(1) }

		# HASH
		case ({}) { ok (0) } else { ok (1) }
		case ({a=>'a', 1=>1, 2=>0}) { ok ($_ eq 'a' || $_ eq '1') }
			else { ok ($_ ne 'a' && $_ ne '1') }

		# SUB/BLOCK
		case (sub{$_[0] eq 'a' }) { ok ($_ eq 'a') }
			else { ok($_ ne 'a') }
		case {$_[0] eq 'a'} { ok ($_ eq 'a') } else { ok($_ ne 'a') }
		case {0} { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		case {1} { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 3. ARRAY SWITCH

my $iteration = 0;
for ([],[1,'a'],[2,'b'])
{
	switch ($_) {
	$iteration++;
		# SELF
		case ($_) { ok(1) }

		# NUMERIC
		case (1) { ok ($iteration==2) } else { ok ($iteration!=2) }
		case (1.0) { ok ($iteration==2) } else { ok ($iteration!=2) }

		# STRING
		case ('a') { ok ($iteration==2) } else { ok ($iteration!=2) }
		case ('b') { ok ($iteration==3) } else { ok ($iteration!=3) }
		case ('1') { ok ($iteration==2) } else { ok ($iteration!=2) }

		# ARRAY
		case (['a',2]) { ok ($iteration>=2) } else { ok ($iteration<2) }
		case ([1,'a']) { ok ($iteration==2) } else { ok($iteration!=2) }
		case ([]) { ok (0) } else { ok(1) }
		case ([7..100]) { ok (0) } else { ok(1) }

		# HASH
		case ({}) { ok (0) } else { ok (1) }
		case ({a=>'a', 1=>1, 2=>0}) { ok ($iteration==2) }
			else { ok ($iteration!=2) }

		# SUB/BLOCK
		case {scalar grep /a/, @_} { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		case (sub {scalar grep /a/, @_ }) { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		case {0} { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		case {1} { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 4. HASH SWITCH

$iteration = 0;
for ({},{a=>1,b=>0})
{
	switch ($_) {
	$iteration++;

		# SELF
		case ($_) { ok(1) } else { ok(0) }

		# NUMERIC
		case (1) { ok (0) } else { ok (1) }
		case (1.0) { ok (0) } else { ok (1) }

		# STRING
		case ('a') { ok ($iteration==2) } else { ok ($iteration!=2) }
		case ('b') { ok (0) } else { ok (1) }
		case ('c') { ok (0) } else { ok (1) }

		# ARRAY
		case (['a',2]) { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		case (['b','a']) { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		case (['b','c']) { ok (0) } else { ok (1) }
		case ([]) { ok (0) } else { ok(1) }
		case ([7..100]) { ok (0) } else { ok(1) }

		# HASH
		case ({}) { ok (0) } else { ok (1) }
		case ({a=>'a', 1=>1, 2=>0}) { ok (0) } else { ok (1) }

		# SUB/BLOCK
		case {$_[0]{a}} { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		case (sub {$_[0]{a}}) { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		case {0} { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		case {1} { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 5. CODE SWITCH

$iteration = 0;
for ( sub {1},
      sub { return 0 unless @_;
	    my ($data) = @_;
	    my $type = ref $data;
	    return $type eq 'HASH'   && $data->{a}
		|| $type eq 'Regexp' && 'a' =~ /$data/
		|| $type eq ""       && $data eq '1';
	  },
      sub {0} )
{
	switch ($_) {
	$iteration++;
		# SELF
		case ($_) { ok(1) } else { ok(0) }

		# NUMERIC
		case (1) { ok ($iteration<=2) } else { ok ($iteration>2) }
		case (1.0) { ok ($iteration<=2) } else { ok ($iteration>2) }
		case (1.1) { ok ($iteration==1) } else { ok ($iteration!=1) }

		# STRING
		case ('a') { ok ($iteration==1) } else { ok ($iteration!=1) }
		case ('b') { ok ($iteration==1) } else { ok ($iteration!=1) }
		case ('c') { ok ($iteration==1) } else { ok ($iteration!=1) }
		case ('1') { ok ($iteration<=2) } else { ok ($iteration>2) }

		# ARRAY
		case ([1, 'a']) { ok ($iteration<=2) }
			else { ok ($iteration>2) }
		case (['b','a']) { ok ($iteration==1) }
			else { ok ($iteration!=1) }
		case (['b','c']) { ok ($iteration==1) }
			else { ok ($iteration!=1) }
		case ([]) { ok ($iteration==1) } else { ok($iteration!=1) }
		case ([7..100]) { ok ($iteration==1) }
			else { ok($iteration!=1) }

		# HASH
		case ({}) { ok ($iteration==1) } else { ok ($iteration!=1) }
		case ({a=>'a', 1=>1, 2=>0}) { ok ($iteration<=2) }
			else { ok ($iteration>2) }

		# SUB/BLOCK
		case {$_[0]->{a}} { ok (0) } else { ok (1) }
		case (sub {$_[0]{a}}) { ok (0) } else { ok (1) }
		case {0} { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		case {1} { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
	}
}


# NESTED SWITCHES

for my $count (1..3)
{
	switch ([9,"a",11]) {
		case (qr/\d/) {
				switch ($count) {
					case (1)     { ok($count==1) }
						else { ok($count!=1) }
					case ([5,6]) { ok(0) } else { ok(1) }
				}
			    }
		ok(1) case (11);
	}
}
