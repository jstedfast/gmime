.PHONY: html

EXTRA_DIST = gmime-tut.sgml

html:
if HAVE_DOCBOOK
	if test -w $(srcdir); then \
	  (cd $(srcdir); \
	   $(DB2HTML) -o html gmime-tut.sgml); \
	fi
else
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "***"
endif

dist-hook: html
if HAVE_DOCBOOK
	cp -Rp $(srcdir)/html $(distdir)
else
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif

