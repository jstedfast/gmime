## Process this file with automake to produce Makefile.in

SUBDIRS = .

gmimeincludedir = $(includedir)/gmime-$(GMIME_API_VERSION)/gmime

INCLUDES = 				\
	-I$(top_srcdir) 		\
	-I$(top_builddir) 		\
	-I$(top_srcdir)/util		\
	-I$(top_builddir)/util		\
	-DG_LOG_DOMAIN=\"gmime\"	\
	$(GMIME_CFLAGS)			\
	$(GLIB_CFLAGS)

noinst_PROGRAMS = gen-table charset-map

EXTRA_DIST = gmime-version.h.in gmime-version.h

lib_LTLIBRARIES = libgmime-2.6.la

libgmime_2_6_la_SOURCES = 		\
	gmime.c				\
	gmime-certificate.c		\
	gmime-charset.c			\
	gmime-common.c			\
	gmime-content-type.c		\
	gmime-crypto-context.c		\
	gmime-data-wrapper.c		\
	gmime-disposition.c		\
	gmime-encodings.c		\
	gmime-events.c			\
	gmime-filter.c			\
	gmime-filter-basic.c		\
	gmime-filter-best.c		\
	gmime-filter-charset.c		\
	gmime-filter-crlf.c		\
	gmime-filter-enriched.c		\
	gmime-filter-from.c		\
	gmime-filter-gzip.c		\
	gmime-filter-html.c		\
	gmime-filter-md5.c		\
	gmime-filter-strip.c		\
	gmime-filter-windows.c		\
	gmime-filter-yenc.c		\
	gmime-gpg-context.c		\
	gmime-header.c			\
	gmime-iconv.c			\
	gmime-iconv-utils.c		\
	gmime-message.c			\
	gmime-message-part.c		\
	gmime-message-partial.c		\
	gmime-multipart.c		\
	gmime-multipart-encrypted.c	\
	gmime-multipart-signed.c	\
	gmime-object.c			\
	gmime-param.c			\
	gmime-parse-utils.c		\
	gmime-parser.c			\
	gmime-part.c			\
	gmime-part-iter.c		\
	gmime-pkcs7-context.c		\
	gmime-signature.c		\
	gmime-stream.c			\
	gmime-stream-buffer.c		\
	gmime-stream-cat.c		\
	gmime-stream-file.c		\
	gmime-stream-filter.c		\
	gmime-stream-fs.c		\
	gmime-stream-gio.c		\
	gmime-stream-mem.c		\
	gmime-stream-mmap.c		\
	gmime-stream-null.c		\
	gmime-stream-pipe.c		\
	gmime-utils.c			\
	internet-address.c

gmimeinclude_HEADERS = 			\
	gmime.h				\
	gmime-certificate.h		\
	gmime-charset.h			\
	gmime-content-type.h		\
	gmime-crypto-context.h		\
	gmime-data-wrapper.h		\
	gmime-disposition.h		\
	gmime-encodings.h		\
	gmime-error.h			\
	gmime-filter.h			\
	gmime-filter-basic.h		\
	gmime-filter-best.h		\
	gmime-filter-charset.h		\
	gmime-filter-crlf.h		\
	gmime-filter-enriched.h		\
	gmime-filter-from.h		\
	gmime-filter-gzip.h		\
	gmime-filter-html.h		\
	gmime-filter-md5.h		\
	gmime-filter-strip.h		\
	gmime-filter-windows.h		\
	gmime-filter-yenc.h		\
	gmime-gpg-context.h		\
	gmime-header.h			\
	gmime-iconv.h			\
	gmime-iconv-utils.h		\
	gmime-message.h			\
	gmime-message-part.h		\
	gmime-message-partial.h		\
	gmime-multipart.h		\
	gmime-multipart-encrypted.h	\
	gmime-multipart-signed.h	\
	gmime-object.h			\
	gmime-param.h			\
	gmime-parser.h			\
	gmime-part.h			\
	gmime-part-iter.h		\
	gmime-pkcs7-context.h		\
	gmime-signature.h		\
	gmime-stream.h			\
	gmime-stream-buffer.h		\
	gmime-stream-cat.h		\
	gmime-stream-file.h		\
	gmime-stream-filter.h		\
	gmime-stream-fs.h		\
	gmime-stream-gio.h		\
	gmime-stream-mem.h		\
	gmime-stream-mmap.h		\
	gmime-stream-null.h		\
	gmime-stream-pipe.h		\
	gmime-utils.h			\
	gmime-version.h			\
	internet-address.h

noinst_HEADERS = 			\
	gmime-charset-map-private.h	\
	gmime-table-private.h		\
	gmime-parse-utils.h		\
	gmime-common.h			\
	gmime-events.h

install-data-local: install-libtool-import-lib

uninstall-local: uninstall-libtool-import-lib

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
install-libtool-import-lib:
	$(INSTALL) .libs/libgmime-$(GMIME_API_VERSION).dll.a $(DESTDIR)$(libdir)

uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libgmime-$(GMIME_API_VERSION).dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

libgmime_2_6_la_LIBADD = $(top_builddir)/util/libutil.la $(GLIB_LIBS)
libgmime_2_6_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined)

gen_table_SOURCES = gen-table.c
gen_table_LDFLAGS = 
gen_table_DEPENDENCIES = 
gen_table_LDADD = 

charset_map_SOURCES = charset-map.c
charset_map_LDFLAGS = 
charset_map_DEPENDENCIES = 
charset_map_LDADD = $(top_builddir)/util/libutil.la $(GLIB_LIBS)

CLEANFILES =

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
GMime-2.6.gir: libgmime-2.6.la
GMime_2_6_gir_INCLUDES = GObject-2.0 Gio-2.0
GMime_2_6_gir_CFLAGS = $(INCLUDES)
GMime_2_6_gir_LIBS = libgmime-2.6.la
GMime_2_6_gir_FILES = $(gmimeinclude_HEADERS) $(libgmime_2_6_la_SOURCES)
GMime_2_6_gir_EXPORT_PACKAGES = gmime-2.6
GMime_2_6_gir_SCANNERFLAGS = \
	--c-include="gmime/gmime.h" \
	--accept-unprefixed \
	--symbol-prefix "gmime_" \
	--symbol-prefix "g_mime_"
INTROSPECTION_GIRS += GMime-2.6.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

gmime-2.6.vapi: GMime-2.6.gir GMime-2.6.metadata

VAPIGEN_VAPIS = gmime-2.6.vapi
gmime_2_6_vapi_DEPS = gio-2.0
gmime_2_6_vapi_METADATADIRS = $(srcdir)
gmime_2_6_vapi_FILES = GMime-2.6.gir

gmime-2.6.deps:
	$(AM_V_GEN) for pkg in $(gmime_2_6_vapi_DEPS); do \
		echo $$pkg >> $@; \
	done

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += GMime-2.6.metadata
CLEANFILES += $(vapi_DATA)
endif
