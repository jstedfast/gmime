API = gmime-api.xml
RAW_API = gmime-api.raw
METADATA = GMime.metadata
ASSEMBLY_NAME = gmime-sharp
references = $(GTK_SHARP_LIBS)
SOURCES_XML = sources.xml

PACKAGE_SHARP = $(PACKAGE)-sharp
TARGET = $(ASSEMBLY)
ASSEMBLY = $(ASSEMBLY_NAME).dll
gapidir = $(datadir)/gapi-2.0
noinst_DATA = $(ASSEMBLY) $(ASSEMBLY).config
gapi_DATA = $(API)
CLEANFILES = $(ASSEMBLY) generated-stamp generated/*.cs $(API)
DISTCLEANFILES = $(ASSEMBLY).config AssemblyInfo.cs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-sharp.pc

sources = 				\
	InternetAddressList.cs		\
	StreamWrapper.cs

build_sources =  			\
	$(srcdir)/InternetAddressList.cs\
	$(srcdir)/StreamWrapper.cs	\
	AssemblyInfo.cs

customs = 				\
	DataWrapper.custom		\
	Global.custom			\
	Header.custom			\
	Message.custom			\
	Object.custom			\
	Stream.custom

build_customs = 			\
	$(srcdir)/DataWrapper.custom	\
	$(srcdir)/Global.custom		\
	$(srcdir)/Header.custom		\
	$(srcdir)/Message.custom	\
	$(srcdir)/Object.custom		\
	$(srcdir)/Stream.custom

EXTRA_DIST =			\
	$(ASSEMBLY).config.in	\
	$(ASSEMBLY_NAME).pub	\
	AssemblyInfo.cs.in	\
	$(RAW_API)		\
	$(customs)		\
	$(sources)		\
	$(SOURCES_XML)		\
	gmime-sharp.pc.in

$(API): $(srcdir)/$(RAW_API) $(METADATA)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	$(GAPI_FIXUP) --api=$(API) --metadata=$(METADATA)

generated-stamp: $(API) $(build_customs)
	rm -f generated/* &&	\
	$(GAPI_CODEGEN) --generate $(API) --outdir=generated --customdir=$(srcdir) --assembly-name=$(ASSEMBLY_NAME) \
	&& touch generated-stamp

$(ASSEMBLY): $(build_sources) generated-stamp $(ASSEMBLY_NAME).snk
	$(CSC) -unsafe /out:$(ASSEMBLY) /target:library $(references) \
	-keyfile:$(ASSEMBLY_NAME).snk $(build_sources) generated/*.cs
	sn -q -R $(ASSEMBLY) $(ASSEMBLY_NAME).snk

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
	gapi-parser $(SOURCES_XML)
