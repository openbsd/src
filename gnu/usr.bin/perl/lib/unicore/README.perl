The *.txt files were copied from

	http://www.unicode.org/Public/UNIDATA/

as of Unicode 4.0.0 (April 2003), updated with

	http://www.unicode.org/Public/4.0-Update1/

as of Unicode 4.0.1 (March 2004)

The two big files, NormalizationTest.txt (2.0MB) and Unihan.txt
(25.7MB) were not included due to space considerations.  Also NOT
included were any *.html files and the Derived*.txt files

    DerivedAge.txt
    DerivedCoreProperties.txt
    DerivedNormalizationProps.txt

To be 8.3-friendly, the lib/unicore/PropertyValueAliases.txt was
renamed to be lib/unicore/PropValueAliases.txt, since otherwise
it would have conflicted with lib/unicore/PropertyAliases.txt.

FOR PUMPKINS

The *.pl files are generated from the *.txt files by the mktables script:
	
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
