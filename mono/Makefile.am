API=gmime-api.xml
RAW_API = gmime-api.raw
METADATA = GMime.metadata
ASSEMBLY_NAME = gmime-sharp
references = $(GTK_SHARP_LIBS)
XML_SOURCES = sources.xml

PACKAGE_SHARP = $(PACKAGE)-sharp
TARGET = $(ASSEMBLY)
ASSEMBLY = $(ASSEMBLY_NAME).dll
gapidir = $(datadir)/gapi
noinst_DATA = $(ASSEMBLY) $(ASSEMBLY).config
gapi_DATA = $(API)
CLEANFILES = $(ASSEMBLY) generated-stamp generated/*.cs $(API)
DISTCLEANFILES = $(ASSEMBLY).config AssemblyInfo.cs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-sharp.pc

sources = 				\
	InternetAddressList.cs		\
	ObjectStream.cs			\
	#

build_sources = $(addprefix $(srcdir)/, $(sources)) AssemblyInfo.cs

customs = 				\
	DataWrapper.custom		\
	Global.custom			\
	Header.custom			\
	Message.custom			\
	Object.custom			\
	Stream.custom			\
	#

build_customs = $(addprefix $(srcdir)/, $(customs))

EXTRA_DIST =			\
	$(ASSEMBLY).config.in	\
	$(ASSEMBLY_NAME).pub	\
	AssemblyInfo.cs.in	\
	$(RAW_API)		\
	$(METADATA)		\
	$(customs)		\
	$(sources)		\
	$(XML_SOURCES)		\
	gmime-sharp.pc.in

$(API): $(srcdir)/$(RAW_API) $(srcdir)/$(METADATA)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA)

generated-stamp: $(API) $(build_customs)
	rm -f generated/* &&	\
	$(GAPI_CODEGEN) --generate $(API) --outdir=generated --customdir=$(srcdir) --assembly-name=$(ASSEMBLY_NAME) 	\
	&& touch generated-stamp

$(ASSEMBLY): $(build_sources) generated-stamp $(ASSEMBLY_NAME).pub
	$(CSC) --unsafe /out:$(ASSEMBLY) /target:library $(references) $(build_sources) generated/*.cs

install-data-local:
	@if test -n '$(TARGET)'; then									\
	  if test -n '$(DESTDIR)'; then								\
	    echo "$(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /root $(DESTDIR)$(prefix)/lib";		\
	    $(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /root $(DESTDIR)$(prefix)/lib || exit 1;	\
	  else												\
	    echo "$(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /gacdir $(prefix)/lib";			\
	    $(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /gacdir $(prefix)/lib || exit 1;		\
	  fi;												\
	fi

uninstall-local:
	@if test -n '$(TARGET)'; then									\
	  if test -n '$(DESTDIR)'; then								\
	    echo "$(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /root $(DESTDIR)$(prefix)/lib";	\
	    $(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /root $(DESTDIR)$(prefix)/lib || exit 1;  \
	  else												\
	    echo "$(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /gacdir $(prefix)/lib";	\
	    $(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /gacdir $(prefix)/lib || exit 1;	\
	  fi;												\
	fi

parse:
	gapi-parser $(XML_SOURCES)
