## Process this file with automake to produce Makefile.in

SUBDIRS = .

extra_DIST = README

INCLUDES = -I$(top_srcdir) $(GLIB_CFLAGS)

noinst_PROGRAMS = basic-example imap-example

DEPS = $(top_builddir)/gmime/libgmime-2.0.la
LDADDS = $(top_builddir)/gmime/libgmime-2.0.la $(GLIB_LIBS)

basic_example_SOURCES = basic-example.c
basic_example_LDFLAGS = 
basic_example_DEPENDENCIES = $(DEPS)
basic_example_LDADD = $(LDADDS)

imap_example_SOURCES = imap-example.c
imap_example_LDFLAGS = 
imap_example_DEPENDENCIES = $(DEPS)
imap_example_LDADD = $(LDADDS)
