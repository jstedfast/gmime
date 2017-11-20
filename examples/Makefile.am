## Process this file with automake to produce Makefile.in

SUBDIRS = .

extra_DIST = README

AM_CPPFLAGS = -I$(top_srcdir) $(GMIME_CFLAGS) $(GLIB_CFLAGS)

noinst_PROGRAMS = basic-example imap-example uuencode uudecode msgcheck

DEPS = 						\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-$(GMIME_API_VERSION).la

LDADDS = 					\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-$(GMIME_API_VERSION).la 	\
	$(GLIB_LIBS)

basic_example_SOURCES = basic-example.c
basic_example_LDFLAGS = 
basic_example_DEPENDENCIES = $(DEPS)
basic_example_LDADD = $(LDADDS)

imap_example_SOURCES = imap-example.c
imap_example_LDFLAGS = 
imap_example_DEPENDENCIES = $(DEPS)
imap_example_LDADD = $(LDADDS)

uuencode_SOURCES = uuencode.c
uuencode_LDFLAGS = 
uuencode_DEPENDENCIES = $(DEPS)
uuencode_LDADD = $(LDADDS)

uudecode_SOURCES = uudecode.c
uudecode_LDFLAGS = 
uudecode_DEPENDENCIES = $(DEPS)
uudecode_LDADD = $(LDADDS)

msgcheck_SOURCES = msgcheck.c
msgcheck_LDFLAGS = 
msgcheck_DEPENDENCIES = $(DEPS)
msgcheck_LDADD = $(LDADDS)
