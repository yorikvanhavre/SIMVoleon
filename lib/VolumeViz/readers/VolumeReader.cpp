/*!
  \class SoVolumeReader VolumeViz/readers/SoVolumeReader.h
  \brief Abstract superclass for all volume data reader classes.
  \ingroup volviz
*/

#include <VolumeViz/readers/SoVolumeReader.h>
#include <Inventor/errors/SoDebugError.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif // HAVE_SYS_TYPES_H

#include <sys/stat.h>
#include <errno.h>
#include <string.h>

// FIXME: the following doc might be outdated. Audit and
// edit. 20030324 mortene.

/*
VOLUMEREADERS

  Currently, only a reader of memory provided data is implemented
  (SoVRMemReader). SoVolumeData uses the interface specified with
  SoVolumeReader, and extensions with other readers should be straight
  forward. When running setReader or setVolumeData, only a pointer to
  the reader is stored. In other words, things could go bananas if the
  client application start mocking around with the reader's settings
  after a call to setReader. If the reader is changed, setReader must
  be run over again.  This requirement differs from TGS as their
  implementation loads all data once specified a reader (I guess).

  The TGS interface for SoVolumeReader contains a function getSubSlice
  with the following definition:

  void getSubSlice(SbBox2s &subSlice, int sliceNumber, void * data)

  It returns a subpage within a specified page along the z-axis. This
  means that the responsibility for building pages along X and Y-axis
  lies within the reader-client.When generating textures along either
  x- or y-axis, this requires a significant number of iterations, one
  for each page along the z-axis. This will in turn trigger plenty
  filereads at different disklocations, and your disk's heads will have
  a disco showdown the Travolta way. I've extended the interface as
  following:

  void getSubSlice(SbBox2s &subSlice,
                   int sliceNumber,
                   void * data,
                   SoOrthoSlice::Axis axis = SoOrthoSlice::Z)

  This moves the responsibility for building pages to the reader.
  It makes it possible to exploit fileformats with possible clever
  data layout, and if the fileformat/input doesn't provide intelligent
  organization, it still wouldn't be any slower. The only drawback is
  that some functionality would be duplicated among several readers
  and making them more complicated.

  The consequences is that readers developed for TGS's implementation
  would not work with ours, but the opposite should work just fine.

  torbjorv 08292002
*/

// *************************************************************************

class SoVolumeReaderP{
public:
  SoVolumeReaderP(SoVolumeReader * master) {
    this->master = master;
  }

  SbString filename;

private:
  SoVolumeReader * master;
};


#define PRIVATE(p) (p->pimpl)
#define PUBLIC(p) (p->master)

// *************************************************************************

SoVolumeReader::SoVolumeReader(void)
{
  PRIVATE(this) = new SoVolumeReaderP(this);
  this->m_data = NULL;
}

SoVolumeReader::~SoVolumeReader()
{
  // FIXME: dealloc this->m_data when we allocated it
  // ourselves. 20021109 mortene.

  delete PRIVATE(this);
}

void
SoVolumeReader::setUserData(void * data)
{
  // FIXME: unimplemented. 20021108 mortene.
}

// *************************************************************************

int
SoVolumeReader::setFilename(const char * filename)
{
  PRIVATE(this)->filename = filename;

  // TGS doc doesn't say what this is supposed to return.
  return 0;
}

int64_t
SoVolumeReader::fileSize(void)
{
  struct stat buf;
  const char * filename = PRIVATE(this)->filename.getString();

  if (stat(filename, &buf) == 0) {
    return buf.st_size;
  }
  else {
    SoDebugError::post("SoVolumeReader::fileSize", "couldn't stat() '%s': %s",
                       filename, strerror(errno));
    return -1;
  }
}

// FIXME: unimplemented methods follows. 20021108 mortene.

void *
SoVolumeReader::getBuffer(int64_t offset, unsigned int size)
{
  return NULL;
}

int SoVolumeReader::bytesToInt(unsigned char * ptr, int sizebytes) { return 0x0000; }
void SoVolumeReader::swapBytes(int * intptr, int sizebytes) { return; }

// *************************************************************************
