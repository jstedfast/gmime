## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE = gmime
DOC_MODULE_VERSIONED = $(DOC_MODULE)-$(GMIME_MAJOR_VERSION).$(GMIME_MINOR_VERSION)

# The top-level SGML file.
DOC_MAIN_SGML_FILE = gmime-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR = $(top_srcdir)/gmime

EXTRA_DIST = gmime-docs.sgml gmime-sections.txt

HTML_DIR = $(datadir)/gtk-doc/html

TARGET_DIR = $(HTML_DIR)/$(DOC_MODULE_VERSIONED)

scan:
	gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="gmime-type-utils.h gmime-charset-map-private.h gmime-table-private.h memchunk.h"

templates: scan
	gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	if ! test -d html ; then mkdir html ; fi
	-cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *.args *-unused.txt *-undocumented.txt

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt 

install-data-local:
	$(INSTALL) -d $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) $(srcdir)/html/*.html $(DESTDIR)$(TARGET_DIR)
	$(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR)
	if test "x$(enable_gtk_doc)" = "xyes"; then \
		gtkdoc-fixxref --module=$(DOC_MODULE_VERSIONED) --html-dir=$(DESTDIR)$(HTML_DIR); \
	fi

uninstall:
	rm -rf $(DESTDIR)$(TARGET_DIR)

dist-hook:
	(cd $(srcdir) ; tar cvf - html) | (cd $(distdir); tar xf -)

.PHONY : html sgml templates scan
