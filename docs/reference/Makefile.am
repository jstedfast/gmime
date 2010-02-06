## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE_VERSION = $(GMIME_MAJOR_VERSION).$(GMIME_MINOR_VERSION)
DOC_MODULE = gmime

# The top-level SGML file.
DOC_MAIN_SGML_FILE = gmime-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR = $(top_srcdir)/gmime

HTML_DIR = $(datadir)/gtk-doc/html

# Images to copy into HTML directory
HTML_IMAGES = 

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = --deprecated-guards="GMIME_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--type-init-func="g_mime_init (0)"

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml --ignore-files=trio

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_builddir)/gmime \
	$(GLIB_CFLAGS)

GTKDOC_LIBS = 					\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-$(GMIME_API_VERSION).la 	\
	$(GLIB_LIBS)

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = 			\
	building.sgml			\
	changes-2.0.sgml		\
	changes-2.2.sgml		\
	changes-2.4.sgml		\
	changes-2.6.sgml		\
	compiling.sgml			\
	data-wrappers.sgml		\
	filters.sgml			\
	question_index.sgml		\
	resources.sgml			\
	streams.sgml			\
	tree_index.sgml

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gmime/*.h
CFILE_GLOB=$(top_srcdir)/gmime/*.c

# Header files to ignore when scanning
IGNORE_HFILES = 			\
	gmime-charset-map-private.h	\
	gmime-table-private.h		\
	gmime-parse-utils.h		\
	gmime-common.h			\
	gmime-events.h

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS = 

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += 				\
	$(DOC_MODULE)-docs.sgml		\
	gmime.hierarchy

BUILT_EXTRA_DIST = 

dist-hook-local: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)';				\
	for f in $$files; do					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi;	\
	  cp $$d/$$f $(distdir) || exit 1; done
