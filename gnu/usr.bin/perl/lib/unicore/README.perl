The *.txt files were copied from

	http://www.unicode.org/Public/4.1.0/ucd

as of Unicode 4.1.0 (April 2005).

The two big files, NormalizationTest.txt (2.1 MB) and Unihan.txt
(26.7 MB) were not included due to space considerations.  Also NOT
included were any *.html files and the Derived*.txt files

    DerivedAge.txt
    DerivedCoreProperties.txt
    DerivedNormalizationProps.txt

To be 8.3-friendly, the lib/unicore/PropertyValueAliases.txt was
renamed to be lib/unicore/PropValueAliases.txt, since otherwise
it would have conflicted with lib/unicore/PropertyAliases.txt.

NOTE: If you modify the input file set you should also run
 
    mktables -makelist
    
which will recreate the mktables.lst file which is used to speed up
the build process.    

FOR PUMPKINS

The *.pl files are generated from the *.txt files by the mktables script,
more recently done during the Perl build process, but if you want to try
the old manual way:
	
	cd lib/unicore
	cp .../UnicodeOriginal/*.txt .
	rm NormalizationTest.txt Unihan.txt Derived*.txt
	p4 edit Properties *.pl */*.pl
	perl ./mktables
	p4 revert -a
	cd ../..
	perl Porting/manicheck

You need to update version by hand

	p4 edit version
	...
	
If any new (or deleted, unlikely but not impossible) *.pl files are indicated:

	cd lib/unicore
	p4 add ...
	p4 delete ...
	cd ../...
	p4 edit MANIFEST
	...

And finally:

	p4 submit

-- 
jhi@iki.fi; updated by nick@ccl4.org
