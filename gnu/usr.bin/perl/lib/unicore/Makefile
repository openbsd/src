all:
	../../miniperl -I../../lib ./mktables

TestProp.pl: mktables UnicodeData.txt Scripts.txt Blocks.txt PropList.txt
	../../miniperl -I../../lib ./mktables -maketest

test:   TestProp.pl
	../../miniperl -I../../lib TestProp.pl

clean:
	rm -f *.pl */*.pl
	rm -f Properties
