EXTRA_DIST = gmime-tut.sgml

all: html

html: 
if ENABLE_GTK_DOC
	(cd $(srcdir); \
	 db2html gmime-tut.sgml; \
	 test -d html && rm -r html; \
	 mv gmime-tut html)
else
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "***"
endif

pdf:
if ENABLE_GTK_DOC
	(cd $(srcdir); db2pdf gmime-tut.sgml)
else
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "***"
endif

dist-hook: html
if ENABLE_GTK_DOC
	cp -Rp $(srcdir)/html $(distdir)
else
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif
