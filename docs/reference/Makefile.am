## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE = gmime
DOC_MODULE_VERSIONED = $(DOC_MODULE)-$(GMIME_MAJOR_VERSION).$(GMIME_MINOR_VERSION)

# The top-level SGML file.
DOC_MAIN_SGML_FILE = gmime-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR = $(top_srcdir)/gmime

HTML_DIR = $(datadir)/gtk-doc/html

TARGET_DIR = $(HTML_DIR)/$(DOC_MODULE_VERSIONED)

content_files = 			\
	building.sgml			\
	changes-2.0.sgml		\
	compiling.sgml			\
	data-wrappers.sgml		\
	filters.sgml			\
	question_index.sgml		\
	resources.sgml			\
	streams.sgml

ignore_headers = 			\
	gmime-charset-map-private.h	\
	gmime-table-private.h		\
	gmime-type-utils.h		\
	gtrie.h				\
	md5-utils.h			\
	memchunk.h			\
	url-scanner.h

EXTRA_DIST = 				\
	$(content_files)		\
	$(DOC_MODULE)-docs.sgml		\
	$(DOC_MODULE)-sections.txt	\
	gmime.hierarchy

SCANOBJ_FILES = $(DOC_MODULE).hierarchy

if ENABLE_GTK_DOC
all-local: html-build.stamp

#### scan ####

scan-build.stamp:
	@echo '*** Scanning header files ***'
	cd $(srcdir) && \
	gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(ignore_headers)"
	touch scan-build.stamp

#### templates ####

tmpl-build.stamp: scan-build.stamp gmime-docs.sgml $(content_files)
	@echo '*** Rebuilding template files ***'
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### sgml ####

sgml-build.stamp: tmpl.stamp
	@echo '*** Building SGML ***'
	cd $(srcdir) && gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp
	@echo '*** Building HTML ***'
	rm -rf $(srcdir)/html
	mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR)
	touch html-build.stamp
endif

##############

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *.args *-unused.txt *-undocumented.txt *.stamp

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt 

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	fi)

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/tmpl
	mkdir $(distdir)/sgml
	mkdir $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/html/* $(distdir)/html

uninstall:
	rm -rf $(DESTDIR)$(TARGET_DIR)

.PHONY : dist-hook-local
