.PHONY: html pdf

EXTRA_DIST = gmime-tut.sgml

all: html

if ENABLE_GTK_DOC
html: 
	(cd $(srcdir); \
	 db2html gmime-tut.sgml; \
	 test -d html && rm -r html; \
	 mv gmime-tut html)

pdf:
	(cd $(srcdir); db2pdf gmime-tut.sgml)

dist-hook: html
	cp -Rp $(srcdir)/html $(distdir)
else
html:
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "***"

pdf:
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "***"

dist-hook:
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif

