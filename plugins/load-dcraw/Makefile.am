plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4\
	-DWITH_GCONF\
	-DDCRAW_NOMAIN\
	-DDCRAW_NOLCMS\
	-DWITH_MMAP_HACK

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = dcrawloader.la

libdir = $(datadir)/rawstudio/plugins/

dcrawloader_la_LIBADD = @PACKAGE_LIBS@
dcrawloader_la_LDFLAGS = -module -avoid-version
dcrawloader_la_SOURCES =  dcrawloader.c dcraw.cc dcraw.h dcraw_api.cc dcraw_api.h mmap-hack.c mmap-hack.h
