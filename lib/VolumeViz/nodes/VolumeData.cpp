/*!
  \class SoVolumeData VolumeViz/nodes/SoVolumeData.h
  \brief The interface for working with volume data sets.
  \ingroup volviz
*/

#include <VolumeViz/nodes/SoVolumeData.h>

#include <Inventor/SbVec3s.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/system/gl.h>
#include <VolumeViz/elements/SoVolumeDataElement.h>
#include <VolumeViz/misc/SoVolumeDataPage.h>
#include <VolumeViz/misc/SoVolumeDataSlice.h>
#include <VolumeViz/readers/SoVRMemReader.h>

#include <Inventor/errors/SoDebugError.h>



/*

DICTIONARY

  "Slice"      : One complete cut through the volume, normal to an axis.
  "Page"       : A segment of a slice.
  "In-memory"  : In this context, "in-memory" means volume data (pages) that's
                 pulled out of the reader and run through a transfer function,
                 thus ready to be rendered.
  LRU          : Least Recently Used




DATA STRUCTURES

  As several different rendering nodes may share the same volume data
  node, a sharing mechanism for in-memory data is implemented. The
  volume is partitioned into slices along each of the three axes, and
  each slice is segmented into pages. Each page is identified by it's
  slice number and it's (x,y) position in the slice. Even though
  different rendering nodes may share the same volume data, they may
  have individual transfer functions. A page shared by two rendering
  nodes with different transfer functions cannot share the same
  in-memory page. A page is therefore also identified by the nodeId of
  it's transfer functions.  All pages with same coordinates (sliceIdx,
  x, y) but different transfer functions are saved as a linked list.



LRU-system

  To support large sets of volume data, a simple memory management
  system is implemented. The scene graph's VolumeData-node contains a
  logical clock that's incremented for each run through it's
  GLRender. All in-memory pages are tagged with a timestamp at the
  time they're loaded. The VolumeData-node has a max limit for the
  amount of HW/SW memory it should occupy, and whenever it exceeds
  this limit it throws out the page with the oldest timestamp. A
  simple LRU-cache, in other words. Of course, all pages' timestamps
  are "touched" (updated) whenever they're rendered.

  The data structures does not work perfectly together with the
  LRU-cache.  Whenever a page is to be deallocated, a search through
  all slices and pages is required. The monitors for bytes allocated
  by in-memory pages are updated in a very non-elegant way (study i.e.
  SoVolumeData::renderOrthoSlice()).

  Some sort of page manager system could be implemented to solve this
  problem by storing all pages in a "flat" structure. This would look
  nicer, but would be slower as long as (sliceIdx, x, y) can't be used
  as direct indices into the tables.



USER INTERACTION

  As for now, the implementation loads only the pages that are needed
  for the current position of a SoROI/SoVolumeRender-node. Due to
  significant overhead when loading data and squeezing them through a
  transfer function, the user experiences major delays in the visual
  response on her interactions. TGS does not have functionality for
  dynamic loading of data, so the StorageHint-enum is extended with
  two elements (LOAD_MAX, LOAD_DYNAMIC). Currently, LOAD_DYNAMIC is
  the only one implemented of the StorageHints, and is set as
  default. By using LOAD_MAX, the specified available memory should be
  filled to it's maximum. Pages should be selected in an intelligent
  way, depending on the location of possible SoROI-nodes (load the
  surrounding area). This should in most cases speed up the visual
  feedback.



PALETTED TEXTURES

  See doc in SoVolumeDataPage.


MEMORY MANAGEMENT

  The TGS-API contains a function setTexMemSize. It makes the client
  application able to specify the amount of memory the volume data
  node should occupy.  In TEXELS?!? As far as my neurons can figure
  out, this doesn't make sense at all. This implementation supports
  this function, but also provides a setHWMemorySize which specifies
  the maximum number of BYTES the volume data should occupy of
  hardware memory.



RENDERING

  An SoVolumeRender is nothing but an SoROI which renders the entire
  volume. And this is how it should be implemented. But this is not
  how it is implemented now. :) The GLRender-function for both SoROI
  and SoVolumeRender is more or less identical, and they should share
  some common render function capable of rendering an entire
  volume. This should in turn use a slice rendering-function similar
  to SoVolumeDataSlice::render.



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

  It returns a subslice within a specified slice along the z-axis. This
  means that the responsibility for building slices along X and Y-axis
  lies within the reader-client.When generating textures along either
  x- or y-axis, this requires a significant number of iterations, one
  for each slice along the z-axis. This will in turn trigger plenty
  filereads at different disklocations, and your disk's heads will have
  a disco showdown the Travolta way. I've extended the interface as
  following:

  void getSubSlice(SbBox2s &subSlice,
                   int sliceNumber,
                   void * data,
                   SoOrthoSlice::Axis axis = SoOrthoSlice::Z)

  This moves the responsibility for building slices to the reader.
  It makes it possible to exploit fileformats with possible clever
  data layout, and if the fileformat/input doesn't provide intelligent
  organization, it still wouldn't be any slower. The only drawback is
  that some functionality would be duplicated among several readers
  and making them more complicated.

  The consequences is that readers developed for TGS's implementation
  would not work with ours, but the opposite should work just fine.



TODO

  No picking functionality whatsoever is implemented. Other missing
  functions are tagged with FIXMEs.

  Missing classes: SoObliqueSlice, SoOrthoSlice, all readers, all
  details.



REFACTORING

  Rumours has it that parts of this library will be refactored and
  extracted into a more or less external c-library. This is a
  very good idea, and it is already partially implemented through
  SoVolumeDataPage and SoVolumeDataSlice. This library should be as
  decoupled from Coin as possible, but it would be a lot of work to
  build a completely standalone one. An intermediate layer between
  the lib and Coin would be required, responsible for translating all
  necessary datastructures (i.e. readers and transferfunctions),
  functioncalls and opengl/coin-states.

  The interface of the library should be quite simple and would
  probably require the following:
  * A way to support the library with data and data characteristics.
    Should be done by providing the lib with pointers to
    SoVolumeReader-objects.
  * Renderingfunctionality. Volumerendering and slicerendering,
    specifying location in space, texturecoordinates in the volume and
    transferfunction.
  * Functionality to specify maximum resource usage by the lib.
  * Preferred storage- and rendering-technique.

  etc etc...

  Conclusion: This interface came out quite obvious. :) And it will
  end up a lot like the existing one, except that most of the code in
  SoVolumeData will be pushed into this lib. As mentioned, the lib
  will rely heavily on different Coin-classes, especially SoState,
  SoVolumeReader and SoTransferFunction and must be designed to fit
  with these.

  The renderingcode is totally independent of the dataformats of
  textures. (RGBA, paletted etc), and may be reused with great ease
  whereever needed in the new lib. This code is located in
  SoVolumeDataSlice::Render and i.e. SoVolumeRender::GLRender.
  I actually spent quite some time implementing the slicerendering,
  getting all the interpolation correct when switching from one page
  to another within the same arbitrary shaped quad.
  SoVolumeRender::GLRender is more straightforward, but it should be
  possible to reuse the same loop for all three axis rendering the code
  more elegant. And it's all about the looks, isn't it?

  All class declarations are copied from TGS reference manual, and
  should be consistent with the TGS VolumeViz-interface (see
  "VOLUMEREADERS").




  torbjorv 08292002
*/

// *************************************************************************

SO_NODE_SOURCE(SoVolumeData);

// *************************************************************************

class SoVolumeDataP {
public:
  enum Axis { X = 0, Y = 1, Z = 2 };

  SoVolumeDataP(SoVolumeData * master)
  {
    this->master = master;

    this->slices[X] = NULL;
    this->slices[Y] = NULL;
    this->slices[Z] = NULL;

    volumeSize = SbBox3f(-1, -1, -1, 1, 1, 1);
    dimensions = SbVec3s(0, 0, 0);
    pageSize = SbVec3s(64, 64, 64);

    maxTexels = 64*1024*1024;
    maxBytesHW = 1024*1024*16;
    numTexels = 0;
    numPages = 0;
    numBytesSW = 0;
    numBytesHW = 0;
    tick = 0;

    VRMemReader = NULL;
    reader = NULL;
  }

  ~SoVolumeDataP()
  {
    delete this->VRMemReader;
    this->releaseAllSlices();
  }

  SbVec3s dimensions;
  SbBox3f volumeSize;
  SbVec3s pageSize;
  SoVolumeData::DataType dataType;

  SoVRMemReader * VRMemReader;
  SoVolumeReader * reader;

  long tick;
  int maxTexels;
  int numTexels;
  int numPages;
  int numBytesSW;
  int maxBytesHW;
  int numBytesHW;

  SoVolumeDataSlice ** slices[3];

  SoVolumeDataSlice * getSlice(const Axis AXISIDX, int sliceidx);

  void releaseAllSlices();
  void releaseSlices(const Axis AXISIDX);

  void freeTexels(int desired);
  void freeHWBytes(int desired);
  void managePages();
  void releaseLRUPage();

  SbBool check2n(int n);

private:
  SoVolumeData * master;
};

#define PRIVATE(p) (p->pimpl)
#define PUBLIC(p) (p->master)

// *************************************************************************

SoVolumeData::SoVolumeData(void)
{
  SO_NODE_CONSTRUCTOR(SoVolumeData);

  PRIVATE(this) = new SoVolumeDataP(this);

  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, AUTO);
  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, TEX2D_MULTI);
  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, TEX2D);
  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, TEX3D);
  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, MEMORY);
  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, VOLUMEPRO);
  SO_NODE_DEFINE_ENUM_VALUE(StorageHint, TEX2D_SINGLE);
  SO_NODE_SET_SF_ENUM_TYPE(storageHint, StorageHint);

  SO_NODE_ADD_FIELD(fileName, (""));
  SO_NODE_ADD_FIELD(storageHint, (SoVolumeData::AUTO));
  SO_NODE_ADD_FIELD(usePalettedTexture, (TRUE));
  // FIXME: is this field an extension specific to us? (Can't find it
  // in TGS doc.) If so, make sure it is documented as such. 20021106 mortene.
  SO_NODE_ADD_FIELD(useCompressedTexture, (TRUE));
}


SoVolumeData::~SoVolumeData()
{
  delete PRIVATE(this);
}


// Doc from parent class.
void
SoVolumeData::initClass(void)
{
  SO_NODE_INIT_CLASS(SoVolumeData, SoVolumeRendering, "SoVolumeRendering");

  SO_ENABLE(SoGLRenderAction, SoVolumeDataElement);
}

void
SoVolumeData::setVolumeSize(const SbBox3f & size)
{
  PRIVATE(this)->volumeSize = size;
  if (PRIVATE(this)->VRMemReader)
    PRIVATE(this)->VRMemReader->setVolumeSize(size);
}

SbBox3f &
SoVolumeData::getVolumeSize(void)
{
  return PRIVATE(this)->volumeSize;
}


SbVec3s &
SoVolumeData::getDimensions(void)
{
  return PRIVATE(this)->dimensions;
}


// FIXME: If size != 2^n these functions should extend to the nearest
// accepted size.  torbjorv 07292002
void
SoVolumeData::setVolumeData(const SbVec3s &dimensions,
                            void * data,
                            SoVolumeData::DataType type)
{

  PRIVATE(this)->VRMemReader = new SoVRMemReader;
  PRIVATE(this)->VRMemReader->setData(dimensions,
                                      data,
                                      PRIVATE(this)->volumeSize,
                                      type);
  this->setReader(PRIVATE(this)->VRMemReader);

  if (PRIVATE(this)->pageSize[0] > dimensions[0])
    PRIVATE(this)->pageSize[0] = dimensions[0];
  if (PRIVATE(this)->pageSize[1] > dimensions[1])
    PRIVATE(this)->pageSize[1] = dimensions[1];
  if (PRIVATE(this)->pageSize[2] > dimensions[2])
    PRIVATE(this)->pageSize[2] = dimensions[2];
}

void
SoVolumeData::setPageSize(int size)
{
  this->setPageSize(SbVec3s(size, size, size));
}

void
SoVolumeData::setPageSize(const SbVec3s & insize)
{
  SbVec3s size = insize;

  // Checking if the sizes are 2^n.
  // FIXME: Should there have been an assertion here? This baby doesn't
  // return anything... 08312002 torbjorv
  if (!PRIVATE(this)->check2n(size[0]) ||
      !PRIVATE(this)->check2n(size[1]) ||
      !PRIVATE(this)->check2n(size[2])) {
#if 1 // debug
    SoDebugError::postInfo("SoVolumeData::setPageSize",
                           "not in 2^n format");
#endif // debug
    return;
  }

  if (size[0] < 4) size[0] = 4;
  if (size[1] < 4) size[1] = 4;
  if (size[2] < 4) size[2] = 4;


  SbBool rebuildX = FALSE;
  SbBool rebuildY = FALSE;
  SbBool rebuildZ = FALSE;

  // The X-size has changed. Rebuild Y- and Z-axis maps.
  if (size[0] != PRIVATE(this)->pageSize[0]) {
    rebuildY = TRUE;
    rebuildZ = TRUE;
  }

  // The Y-size has changed. Rebuild X- and Z-axis maps.
  if (size[1] != PRIVATE(this)->pageSize[1]) {
    rebuildX = TRUE;
    rebuildZ = TRUE;
  }

  // The Z-size has changed. Rebuild X- and Y-axis maps.
  if (size[2] != PRIVATE(this)->pageSize[2]) {
    rebuildX = TRUE;
    rebuildY = TRUE;
  }

  PRIVATE(this)->pageSize = size;

  if (rebuildX) PRIVATE(this)->releaseSlices(SoVolumeDataP::X);
  if (rebuildY) PRIVATE(this)->releaseSlices(SoVolumeDataP::Y);
  if (rebuildZ) PRIVATE(this)->releaseSlices(SoVolumeDataP::Z);
}

void
SoVolumeData::GLRender(SoGLRenderAction * action)
{
  SoVolumeDataElement::setVolumeData(action->getState(), this, this);
  PRIVATE(this)->tick++;
}

void
SoVolumeData::renderOrthoSlice(SoState * state,
                               const SbBox2f & quad,
                               float depth,
                               int sliceIdx,
                               const SbBox2f & textureCoords,
                               SoTransferFunction * transferFunction,
                               // axis: 0, 1, 2 for X, Y or Z axis.
                               int axis)
{
  SbVec2f max, min;
  quad.getBounds(min, max);

  SoVolumeDataSlice * slice =
    PRIVATE(this)->getSlice((SoVolumeDataP::Axis)axis, sliceIdx);

  // FIXME: huh? What's up with this? 20021111 mortene.
  PRIVATE(this)->numTexels -= slice->numTexels;
  PRIVATE(this)->numPages -= slice->numPages;
  PRIVATE(this)->numBytesSW -= slice->numBytesSW;
  PRIVATE(this)->numBytesHW -= slice->numBytesHW;

  SbVec3f v0, v1, v2, v3;
  if (axis == SoVolumeDataP::X) {
    v0 = SbVec3f(depth, min[1], min[0]);
    v1 = SbVec3f(depth, min[1], max[0]);
    v2 = SbVec3f(depth, max[1], max[0]);
    v3 = SbVec3f(depth, max[1], min[0]);
  }
  else if (axis == SoVolumeDataP::Y) {
    v0 = SbVec3f(min[0], depth, min[1]);
    v1 = SbVec3f(max[0], depth, min[1]);
    v2 = SbVec3f(max[0], depth, max[1]);
    v3 = SbVec3f(min[0], depth, max[1]);
  }
  else if (axis == SoVolumeDataP::Z) {
    v0 = SbVec3f(min[0], min[1], depth);
    v1 = SbVec3f(max[0], min[1], depth);
    v2 = SbVec3f(max[0], max[1], depth);
    v3 = SbVec3f(min[0], max[1], depth);
  }
  else assert(FALSE);

  slice->render(state, v0, v1, v2, v3, textureCoords, transferFunction,
                PRIVATE(this)->tick);

  // FIXME: huh? What's up with this? 20021111 mortene.
  PRIVATE(this)->numTexels += slice->numTexels;
  PRIVATE(this)->numPages += slice->numPages;
  PRIVATE(this)->numBytesSW += slice->numBytesSW;
  PRIVATE(this)->numBytesHW += slice->numBytesHW;

  PRIVATE(this)->managePages();
}

SbVec3s &
SoVolumeData::getPageSize()
{
  return PRIVATE(this)->pageSize;
}

void
SoVolumeData::setTexMemorySize(int size)
{
  PRIVATE(this)->maxTexels = size;
}

void
SoVolumeData::setReader(SoVolumeReader * reader)
{
  PRIVATE(this)->reader = reader;

  reader->getDataChar(PRIVATE(this)->volumeSize,
                      PRIVATE(this)->dataType,
                      PRIVATE(this)->dimensions);
}

void
SoVolumeData::setHWMemorySize(int size)
{
  PRIVATE(this)->maxBytesHW = size;
}

/*************************** PIMPL-FUNCTIONS ********************************/


SoVolumeDataSlice *
SoVolumeDataP::getSlice(const SoVolumeDataP::Axis AXISIDX, int sliceidx)
{
  assert((AXISIDX >= X) && (AXISIDX <= Z));
  assert((sliceidx >= 0) && (sliceidx < this->dimensions[AXISIDX]));

#if 0 // debug
  SoDebugError::postInfo("SoVolumeDataP::getSlice", "axis==%c sliceidx==%d",
                         AXISIDX == X ? 'X' : (AXISIDX == Y ? 'Y' : 'Z'),
                         sliceidx);
#endif // debug

  // First SoVolumeDataPage ever for this axis?
  if (this->slices[AXISIDX] == NULL) {
    this->slices[AXISIDX] = new SoVolumeDataSlice*[this->dimensions[AXISIDX]];
    for (int i=0; i < this->dimensions[AXISIDX]; i++) {
      this->slices[AXISIDX][i] = NULL;
    }
  }

  if (this->slices[AXISIDX][sliceidx] == NULL) {
    SoVolumeDataSlice * newslice = new SoVolumeDataSlice;

    SoOrthoSlice::Axis axis =
      AXISIDX == X ? SoOrthoSlice::X :
      (AXISIDX == Y ? SoOrthoSlice::Y : SoOrthoSlice::Z );

    SbVec2s pagesize =
      AXISIDX == X ?
      SbVec2s(this->pageSize[2], this->pageSize[1]) :
      (AXISIDX == Y ?
       SbVec2s(this->pageSize[0], this->pageSize[2]) :
       SbVec2s(this->pageSize[0], this->pageSize[1]));
    
    assert(pagesize[0] > 0 && pagesize[1] > 0);
    newslice->init(this->reader, sliceidx, axis, pagesize);

    this->slices[AXISIDX][sliceidx] = newslice;
  }

  return this->slices[AXISIDX][sliceidx];
}

// FIXME: Perhaps there already is a function somewhere in C or Coin
// that can test this easily?  31082002 torbjorv
SbBool
SoVolumeDataP::check2n(int n)
{
  for (int i = 0; i < (int) (sizeof(int)*8); i++) {

    if (n & 1) {
      if (n != 1)
        return FALSE;
      else
        return TRUE;
    }

    n >>= 1;
  }
  return TRUE;
}

void
SoVolumeDataP::releaseAllSlices(void)
{
  for (int i = 0; i < 3; i++) { this->releaseSlices((Axis)i); }
}

void
SoVolumeDataP::freeTexels(int desired)
{
  if (desired > maxTexels) return;

  while ((maxTexels - numTexels) < desired)
    this->releaseLRUPage();
}


void
SoVolumeDataP::releaseLRUPage(void)
{
  SoVolumeDataPage * LRUPage = NULL;
  SoVolumeDataPage * tmpPage = NULL;
  SoVolumeDataSlice * LRUSlice = NULL;

  // Searching for LRU page among X-slices
  if (this->slices[X]) {
    for (int i = 0; i < this->dimensions[0]; i++) {
      tmpPage = NULL;
      if (this->slices[X][i]) {
        tmpPage = this->slices[X][i]->getLRUPage();
        if (LRUPage == NULL) {
          LRUSlice = this->slices[X][i];
          LRUPage = tmpPage;
        }
        else if (tmpPage && (tmpPage->lastuse < LRUPage->lastuse)) {
          LRUSlice = this->slices[X][i];
          LRUPage = tmpPage;
        }
      }
    }
  }

  // Searching for LRU page among Y-slices
  if (this->slices[Y]) {
    for (int i = 0; i < this->dimensions[1]; i++) {
      tmpPage = NULL;
      if (this->slices[Y][i]) {
        tmpPage = this->slices[Y][i]->getLRUPage();
        if (LRUPage == NULL) {
          LRUSlice = this->slices[Y][i];
          LRUPage = tmpPage;
        }
        else if (tmpPage && (tmpPage->lastuse < LRUPage->lastuse)) {
          LRUSlice = this->slices[Y][i];
          LRUPage = tmpPage;
        }
      }
    }
  }

  // Searching for LRU page among X-slices
  if (this->slices[Z]) {
    for (int i = 0; i < this->dimensions[2]; i++) {
      tmpPage = NULL;
      if (this->slices[Z][i]) {
        tmpPage = this->slices[Z][i]->getLRUPage();
        if (LRUPage == NULL) {
          LRUSlice = this->slices[Z][i];
          LRUPage = tmpPage;
        }
        else if (tmpPage && (tmpPage->lastuse < LRUPage->lastuse)) {
          LRUSlice = this->slices[Z][i];
          LRUPage = tmpPage;
        }
      }
    }
  }

  this->numTexels -= LRUSlice->numTexels;
  this->numPages -= LRUSlice->numPages;
  this->numBytesSW -= LRUSlice->numBytesSW;
  this->numBytesHW -= LRUSlice->numBytesHW;

  LRUSlice->releasePage(LRUPage);
#if 1 // debug
  SoDebugError::postInfo("SoVolumeDataP::releaseLRUPage",
                         "released page");
#endif // debug

  this->numTexels += LRUSlice->numTexels;
  this->numPages += LRUSlice->numPages;
  this->numBytesSW += LRUSlice->numBytesSW;
  this->numBytesHW += LRUSlice->numBytesHW;
}

void
SoVolumeDataP::releaseSlices(const SoVolumeDataP::Axis AXISIDX)
{
  if (this->slices[AXISIDX] == NULL) return;

  for (int i = 0; i < this->dimensions[AXISIDX]; i++) {
    delete this->slices[AXISIDX][i];
    this->slices[AXISIDX][i] = NULL;
  }

  delete[] this->slices[AXISIDX];
}

void
SoVolumeDataP::freeHWBytes(int desired)
{
  if (desired > this->maxBytesHW) return;

  while ((this->maxBytesHW - this->numBytesHW) < desired) {
    this->releaseLRUPage();
  }
}

void
SoVolumeDataP::managePages(void)
{
  // Keep both measures within maxlimits
  this->freeHWBytes(0);
  this->freeTexels(0);
}


/****************** UNIMPLEMENTED FUNCTIONS ******************************/
// FIXME: Implement these functions. torbjorv 08282002


SbBool
SoVolumeData::getVolumeData(SbVec3s & dimensions, void *& data,
                            SoVolumeData::DataType & type)
{ return FALSE; }

SoVolumeReader *
SoVolumeData::getReader()
{ return NULL; }

SbBool
SoVolumeData::getMinMax(int &min, int &max)
{ return FALSE; }

SbBool
SoVolumeData::getHistogram(int &length, int *&histogram)
{ return FALSE; }

SoVolumeData *
SoVolumeData::subSetting(const SbBox3s &region)
{ return NULL; }

void
SoVolumeData::updateRegions(const SbBox3s *region, int num)
{}

SoVolumeData *
SoVolumeData::reSampling(const SbVec3s &dimensions,
                         SoVolumeData::SubMethod subMethod,
                         SoVolumeData::OverMethod)
{ return NULL; }

void
SoVolumeData::enableSubSampling(SbBool enable)
{}

void
SoVolumeData::enableAutoSubSampling(SbBool enable)
{}

void
SoVolumeData::enableAutoUnSampling(SbBool enable)
{}

void
SoVolumeData::unSample()
{}

void
SoVolumeData::setSubSamplingMethod(SubMethod method)
{}

void
SoVolumeData::setSubSamplingLevel(const SbVec3s &ROISampling,
                    const SbVec3s &secondarySampling)
{}
