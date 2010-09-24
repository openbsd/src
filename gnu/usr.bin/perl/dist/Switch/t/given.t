use Carp;
use Switch qw(Perl6 __ fallthrough);

my($C,$M);sub ok{$C++;$M.=$_[0]?"ok $C\n":"not ok $C (line ".(caller)[2].")\n"}
END{print"1..$C\n$M"}

# NON-when THINGS;

$when->{when} = { when => "when" };

*when = \&when;

# PREMATURE when

eval { when 1 { ok(0) }; ok(0) } || ok(1);

# H.O. FUNCS

given __ > 2 {

	when 1	{ ok(0) } else { ok(1) }
	when 2	{ ok(0) } else { ok(1) }
	when 3	{ ok(1) } else { ok(0) }
}

given (3) {

	eval { when __ <= 1 || __ > 2	{ ok(0) } } || ok(1);
	when __ <= 2 		{ ok(0) };
	when __ <= 3		{ ok(1) };
}

# POSSIBLE ARGS: NUMERIC, STRING, ARRAY, HASH, REGEX, CODE

# 1. NUMERIC SWITCH

for (1..3)
{
	given ($_) {
		# SELF
		when ($_) { ok(1) } else { ok(0) }

		# NUMERIC
		when 1 { ok ($_==1) } else { ok($_!=1) }
		when (1)  { ok ($_==1) } else { ok($_!=1) }
		when 3 { ok ($_==3) } else { ok($_!=3) }
		when (4) { ok (0) } else { ok(1) }
		when (2) { ok ($_==2) } else { ok($_!=2) }

		# STRING
		when ('a') { ok (0) } else { ok(1) }
		when  'a'  { ok (0) } else { ok(1) }
		when ('3') { ok ($_ == 3) } else { ok($_ != 3) }
		when ('3.0') { ok (0) } else { ok(1) }

		# ARRAY
		when ([10,5,1]) { ok ($_==1) } else { ok($_!=1) }
		when  [10,5,1]  { ok ($_==1) } else { ok($_!=1) }
		when (['a','b']) { ok (0) } else { ok(1) }
		when (['a','b',3]) { ok ($_==3) } else { ok ($_!=3) }
		when (['a','b',2.0])  { ok ($_==2) } else { ok ($_!=2) }
		when ([])  { ok (0) } else { ok(1) }

		# HASH
		when ({})  { ok (0) } else { ok (1) }
		when {}  { ok (0) } else { ok (1) }
		when {1,1}  { ok ($_==1) } else { ok($_!=1) }
		when ({1=>1, 2=>0})  { ok ($_==1) } else { ok($_!=1) }

		# SUB/BLOCK
		when (sub {$_[0]==2})  { ok ($_==2) } else { ok($_!=2) }
		when {$_[0]==2}  { ok ($_==2) } else { ok($_!=2) }
		when {0}  { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		when {1}  { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 2. STRING SWITCH

for ('a'..'c','1')
{
	given ($_) {
		# SELF
		when ($_)  { ok(1) } else { ok(0) }

		# NUMERIC
		when (1)   { ok ($_ !~ /[a-c]/) } else { ok ($_ =~ /[a-c]/) }
		when (1.0)  { ok ($_ !~ /[a-c]/) } else { ok ($_ =~ /[a-c]/) }

		# STRING
		when ('a')  { ok ($_ eq 'a') } else { ok($_ ne 'a') }
		when ('b')  { ok ($_ eq 'b') } else { ok($_ ne 'b') }
		when ('c')  { ok ($_ eq 'c') } else { ok($_ ne 'c') }
		when ('1')  { ok ($_ eq '1') } else { ok($_ ne '1') }
		when ('d')  { ok (0) } else { ok (1) }

		# ARRAY
		when (['a','1'])  { ok ($_ eq 'a' || $_ eq '1') }
			else { ok ($_ ne 'a' && $_ ne '1') }
		when (['z','2'])  { ok (0) } else { ok(1) }
		when ([])  { ok (0) } else { ok(1) }

		# HASH
		when ({})  { ok (0) } else { ok (1) }
		when ({a=>'a', 1=>1, 2=>0})  { ok ($_ eq 'a' || $_ eq '1') }
			else { ok ($_ ne 'a' && $_ ne '1') }

		# SUB/BLOCK
		when (sub{$_[0] eq 'a' })  { ok ($_ eq 'a') }
			else { ok($_ ne 'a') }
		when {$_[0] eq 'a'}  { ok ($_ eq 'a') } else { ok($_ ne 'a') }
		when {0}  { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		when {1}  { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 3. ARRAY SWITCH

my $iteration = 0;
for ([],[1,'a'],[2,'b'])
{
	given ($_) {
	$iteration++;
		# SELF
		when ($_)  { ok(1) }

		# NUMERIC
		when (1)  { ok ($iteration==2) } else { ok ($iteration!=2) }
		when (1.0)  { ok ($iteration==2) } else { ok ($iteration!=2) }

		# STRING
		when ('a')  { ok ($iteration==2) } else { ok ($iteration!=2) }
		when ('b')  { ok ($iteration==3) } else { ok ($iteration!=3) }
		when ('1')  { ok ($iteration==2) } else { ok ($iteration!=2) }

		# ARRAY
		when (['a',2])  { ok ($iteration>=2) } else { ok ($iteration<2) }
		when ([1,'a'])  { ok ($iteration==2) } else { ok($iteration!=2) }
		when ([])  { ok (0) } else { ok(1) }
		when ([7..100])  { ok (0) } else { ok(1) }

		# HASH
		when ({})  { ok (0) } else { ok (1) }
		when ({a=>'a', 1=>1, 2=>0})  { ok ($iteration==2) }
			else { ok ($iteration!=2) }

		# SUB/BLOCK
		when {scalar grep /a/, @_}  { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		when (sub {scalar grep /a/, @_ })  { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		when {0}  { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		when {1}  { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
	}
}


# 4. HASH SWITCH

$iteration = 0;
for ({},{a=>1,b=>0})
{
	given ($_) {
	$iteration++;

		# SELF
		when ($_)  { ok(1) } else { ok(0) }

		# NUMERIC
		when (1)  { ok (0) } else { ok (1) }
		when (1.0)  { ok (0) } else { ok (1) }

		# STRING
		when ('a')  { ok ($iteration==2) } else { ok ($iteration!=2) }
		when ('b')  { ok (0) } else { ok (1) }
		when ('c')  { ok (0) } else { ok (1) }

		# ARRAY
		when (['a',2])  { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		when (['b','a'])  { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		when (['b','c'])  { ok (0) } else { ok (1) }
		when ([])  { ok (0) } else { ok(1) }
		when ([7..100])  { ok (0) } else { ok(1) }

		# HASH
		when ({})  { ok (0) } else { ok (1) }
		when ({a=>'a', 1=>1, 2=>0})  { ok (0) } else { ok (1) }

		# SUB/BLOCK
		when {$_[0]{a}}  { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		when (sub {$_[0]{a}})  { ok ($iteration==2) }
			else { ok ($iteration!=2) }
		when {0}  { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		when {1}  { ok (1) } else { ok (0) }	# ; -> SUB, NOT HASH
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
	given ($_) {
	$iteration++;
		# SELF
		when ($_)  { ok(1) } else { ok(0) }

		# NUMERIC
		when (1)  { ok ($iteration<=2) } else { ok ($iteration>2) }
		when (1.0)  { ok ($iteration<=2) } else { ok ($iteration>2) }
		when (1.1)  { ok ($iteration==1) } else { ok ($iteration!=1) }

		# STRING
		when ('a')  { ok ($iteration==1) } else { ok ($iteration!=1) }
		when ('b')  { ok ($iteration==1) } else { ok ($iteration!=1) }
		when ('c')  { ok ($iteration==1) } else { ok ($iteration!=1) }
		when ('1')  { ok ($iteration<=2) } else { ok ($iteration>2) }

		# ARRAY
		when ([1, 'a'])  { ok ($iteration<=2) }
			else { ok ($iteration>2) }
		when (['b','a'])  { ok ($iteration==1) }
			else { ok ($iteration!=1) }
		when (['b','c'])  { ok ($iteration==1) }
			else { ok ($iteration!=1) }
		when ([])  { ok ($iteration==1) } else { ok($iteration!=1) }
		when ([7..100])  { ok ($iteration==1) }
			else { ok($iteration!=1) }

		# HASH
		when ({})  { ok ($iteration==1) } else { ok ($iteration!=1) }
		when ({a=>'a', 1=>1, 2=>0})  { ok ($iteration<=2) }
			else { ok ($iteration>2) }

		# SUB/BLOCK
		when {$_[0]->{a}}  { ok (0) } else { ok (1) }
		when (sub {$_[0]{a}})  { ok (0) } else { ok (1) }
		when {0}  { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
		when {1}  { ok (0) } else { ok (1) }	# ; -> SUB, NOT HASH
	}
}


# NESTED SWITCHES

for my $count (1..3)
{
	given ([9,"a",11]) {
		when (qr/\d/)  {
				given ($count) {
					when (1)      { ok($count==1) }
						else { ok($count!=1) }
					when ([5,6])  { ok(0) } else { ok(1) }
				}
			    }
		ok(1) when 11;
	}
}
