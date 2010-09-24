# Test the user_agent method. 
use Test::More 'no_plan';
use CGI;

my $q = CGI->new; 

is($q->user_agent, undef, 'user_agent: undef test'); 

$ENV{HTTP_USER_AGENT} = 'mark';
is($q->user_agent, 'mark', 'user_agent: basic test'); 
ok($q->user_agent('ma.*'), 'user_agent: positive regex test'); 
ok(!$q->user_agent('BOOM.*'), 'user_agent: negative regex test'); 


