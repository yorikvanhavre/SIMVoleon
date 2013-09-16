DEFAULT_INCLUDES = -I$(top_builddir)/lib -I$(top_srcdir)/lib

noinst_LTLIBRARIES = lib2Drender.la

RegularSources = \
	PageHandler.cpp CvrPageHandler.h \
	2DTexSubPage.cpp Cvr2DTexSubPage.h \
	2DTexPage.cpp Cvr2DTexPage.h

EXTRA_DIST = \
	texmemfullimg.h

lib2Drender_la_SOURCES = $(RegularSources)
