DEFAULT_INCLUDES = -I$(top_builddir)/lib -I$(top_srcdir)/lib

noinst_LTLIBRARIES = libmisc.la

RegularSources = \
	VoxelChunk.cpp CvrVoxelChunk.h \
	CLUT.cpp CvrCLUT.h \
	Util.cpp CvrUtil.h \
	ResourceManager.cpp CvrResourceManager.h \
	CvrGlobalRenderLock.h GlobalRenderLock.cpp \
	GIMPGradient.cpp CvrGIMPGradient.h \
	Gradient.cpp CvrGradient.h \
	CentralDifferenceGradient.cpp CvrCentralDifferenceGradient.h

libmisc_la_SOURCES = $(RegularSources)
