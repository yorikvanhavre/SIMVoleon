// From torbjorv's dictionary: a "slice" is "one complete cut through
// the volume, normal to an axis".

#include <VolumeViz/misc/SoVolumeDataSlice.h>

#include <Inventor/errors/SoDebugError.h>
#include <Inventor/system/gl.h>


// *************************************************************************

// There's a linked list of pages on each [row, col] spot in a slice,
// because we need to make different pages for different
// SoTransferFunction instances.

class SoVolumeDataPageItem {
public:
  SoVolumeDataPageItem(SoVolumeDataPage * p)
  {
    this->page = p;
    this->next = NULL;
    this->prev = NULL;
  }

  SoVolumeDataPage * page;
  SoVolumeDataPageItem * next, * prev;
};

// *************************************************************************


SoVolumeDataSlice::SoVolumeDataSlice(void)
{
  this->pageSize = SbVec2s(32, 32);
  this->pages = NULL;
  this->axis = SoOrthoSlice::Z;
  this->numPages = 0;
  this->numTexels = 0;
  this->sliceIdx = 0;
  this->reader = NULL;
  this->dataType = SoVolumeData::RGBA;
}


SoVolumeDataSlice::~SoVolumeDataSlice()
{
  this->reader = NULL;
  this->releaseAllPages();
}


void SoVolumeDataSlice::init(SoVolumeReader * reader, int sliceIdx,
                             SoOrthoSlice::Axis axis,
                             const SbVec2s & pageSize)
{
  this->releaseAllPages();

  this->reader = reader;
  this->sliceIdx = sliceIdx;
  this->axis = axis;
  this->pageSize = pageSize;

  SbVec3s dim;
  SbBox3f size;
  reader->getDataChar(size, this->dataType, dim);

  switch (axis) {
    case SoOrthoSlice::X:
      this->dimensions[0] = dim[2];
      this->dimensions[1] = dim[1];
      break;

    case SoOrthoSlice::Y:
      this->dimensions[0] = dim[0];
      this->dimensions[1] = dim[2];
      break;

    case SoOrthoSlice::Z:
      this->dimensions[0] = dim[0];
      this->dimensions[1] = dim[1];
      break;
  }

  this->numCols = this->dimensions[0] / this->pageSize[0];
  this->numRows = this->dimensions[1] / this->pageSize[1];
}



/*!
  Release resources used by a page in the slice.
*/
void
SoVolumeDataSlice::releasePage(SoVolumeDataPage * page)
{
  assert(page != NULL);
  assert(this->pages != NULL);

  const int NRPAGES = this->numCols * this->numRows;
  for (int i = 0; i < NRPAGES; i++) {
    SoVolumeDataPageItem * p = this->pages[i];
    if (p == NULL) continue; // skip this, continue with for-loop

    while ((p != NULL) && (p->page != page)) { p = p->next; }
    if (p == NULL) continue; // not the right list, continue with for-loop

    this->numPages--;
    this->numTexels -= this->pageSize[0] * this->pageSize[1];
    this->numBytesSW -= p->page->numBytesSW;
    this->numBytesHW -= p->page->numBytesHW;

    if (p->next) { p->next->prev = p->prev; }
    if (p->prev) { p->prev->next = p->next; }
    else { this->pages[i] = p->next; }

    delete p->page;
    delete p;
    return;
  }
  assert(FALSE && "couldn't find page");
}



void SoVolumeDataSlice::releaseLRUPage(void)
{
  assert(this->pages != NULL);

  SoVolumeDataPage * LRUPage = this->getLRUPage();
  assert(LRUPage != NULL);
  this->releasePage(LRUPage);
}



SoVolumeDataPage *
SoVolumeDataSlice::getLRUPage(void)
{
  assert(this->pages != NULL);

  SoVolumeDataPage * LRUPage = NULL;

  const int NRPAGES = this->numCols * this->numRows;
  for (int i = 0; i < NRPAGES; i++) {
    SoVolumeDataPageItem * pitem = this->pages[i];
    while (pitem != NULL) {
      if (LRUPage == NULL) { LRUPage = pitem->page; }
      else if (pitem->page->lastuse < LRUPage->lastuse) { LRUPage = pitem->page; }
      pitem = pitem->next;
    }
  }
  return LRUPage;
}


void
SoVolumeDataSlice::releaseAllPages(void)
{
  if (this->pages == NULL) return;

  for (int i = 0; i < this->numCols * this->numRows; i++) {
    SoVolumeDataPageItem * pitem = this->pages[i];
    if (pitem == NULL) continue;

    do {
      SoVolumeDataPageItem * prev = pitem;
      pitem = pitem->next;
      delete prev->page;
      delete prev;
    } while (pitem != NULL);

    this->pages[i] = NULL;
  }

  delete[] this->pages;
  this->pages = NULL;
}




/*!
  Renders arbitrary shaped quad. Automatically loads all pages needed
  for the given texturecoords. Texturecoords are in normalized
  coordinates [0, 1].

  Vertices are specified in counterclockwise order: v0 maps to lower
  left of slice, v1 maps to lower right of slice, v2 maps to upper
  right of slice, and v3 maps to upper left of slice.
*/
void SoVolumeDataSlice::render(SoState * state,
                               const SbVec3f & v0, const SbVec3f & v1,
                               const SbVec3f & v2, const SbVec3f & v3,
                               const SbBox2f & textureCoords,
                               SoTransferFunction * transferFunction,
                               long tick)
{
  assert(this->reader);
  assert(transferFunction);

  SbVec2f minUV, maxUV;
  textureCoords.getBounds(minUV, maxUV);

  SbVec2f pageSizef =
    SbVec2f(float(this->pageSize[0])/float(dimensions[0]),
            float(this->pageSize[1])/float(dimensions[1]));

  // Local page-UV-coordinates for the current quad to be rendered.
  SbVec2f localMinUV, localMaxUV;

  // Global slice-UV-coordinates for the current quad to be rendered.
  SbVec2f globalMinUV, globalMaxUV;

  // Vertices for left and right edge of current row
  SbVec3f endLowerLeft, endLowerRight;
  SbVec3f endUpperLeft, endUpperRight;

  // Vertices for current quad to be rendered
  SbVec3f upperLeft, upperRight, lowerLeft, lowerRight;

  globalMinUV = minUV;
  endLowerLeft = v0;
  endLowerRight = v1;
  int row = (int) (minUV[1]*this->numRows);
  while (globalMinUV[1] != maxUV[1]) {

    if ((row + 1)*pageSizef[1] < maxUV[1]) {
      // This is not the last row to be rendered
      globalMaxUV[1] = (row + 1)*pageSizef[1];
      localMaxUV[1] = 1.0;

      // Interpolating the row's endvertices
      float k =
        float(globalMaxUV[1] - minUV[1])/float(maxUV[1] - minUV[1]);
      endUpperLeft[0] = (1 - k)*v0[0] + k*v3[0];
      endUpperLeft[1] = (1 - k)*v0[1] + k*v3[1];
      endUpperLeft[2] = (1 - k)*v0[2] + k*v3[2];

      endUpperRight[0] = (1 - k)*v1[0] + k*v2[0];
      endUpperRight[1] = (1 - k)*v1[1] + k*v2[1];
      endUpperRight[2] = (1 - k)*v1[2] + k*v2[2];
    }
    else {

      // This is the last row to be rendered
      globalMaxUV[1] = maxUV[1];
      localMaxUV[1] = (globalMaxUV[1] - row*pageSizef[1])/pageSizef[1];

      endUpperLeft = v3;
      endUpperRight = v2;
    }


    int col = (int) (minUV[0]*this->dimensions[0]/this->pageSize[0]);
    globalMinUV[0] = minUV[0];
    localMinUV[0] = (globalMinUV[0] - col*pageSizef[0])/pageSizef[0];
    localMinUV[1] = (globalMinUV[1] - row*pageSizef[1])/pageSizef[1];
    lowerLeft = endLowerLeft;
    upperLeft = endUpperLeft;
    while (globalMinUV[0] != maxUV[0]) {
      if ((col + 1)*pageSizef[0] < maxUV[0]) {

        // Not the last quad on the row
        globalMaxUV[0] = (col + 1)*pageSizef[0];
        localMaxUV[0] = 1.0;

        // Interpolating the quad's rightmost vertices
        float k =
          float(globalMaxUV[0] - minUV[0])/float(maxUV[0] - minUV[0]);
        lowerRight = (1 - k)*endLowerLeft + k*endLowerRight;
        upperRight = (1 - k)*endUpperLeft + k*endUpperRight;

      }
      else {

        // The last quad on the row
        globalMaxUV[0] = maxUV[0];
        localMaxUV[0] = (maxUV[0] - col*pageSizef[0])/pageSizef[0];

        lowerRight = endLowerRight;
        upperRight = endUpperRight;
      }

      // rendering
      SoVolumeDataPage * page = this->getPage(col, row, transferFunction);
      if (page == NULL) { page = this->buildPage(col, row, transferFunction); }
      page->setActivePage(tick);

      glBegin(GL_QUADS);
      glColor4f(1, 1, 1, 1);
      glTexCoord2f(localMinUV[0], localMinUV[1]);
      glVertex3f(lowerLeft[0], lowerLeft[1], lowerLeft[2]);
      glTexCoord2f(localMaxUV[0], localMinUV[1]);
      glVertex3f(lowerRight[0], lowerRight[1], lowerRight[2]);
      glTexCoord2f(localMaxUV[0], localMaxUV[1]);
      glVertex3f(upperRight[0], upperRight[1], upperRight[2]);
      glTexCoord2f(localMinUV[0], localMaxUV[1]);
      glVertex3f(upperLeft[0], upperLeft[1], upperLeft[2]);
      glEnd();

      globalMinUV[0] = globalMaxUV[0];
      lowerLeft = lowerRight;
      upperLeft = upperRight;
      localMinUV[0] = 0.0;
      col++;
    }

    globalMinUV[1] = globalMaxUV[1];
    localMinUV[0] = 0.0;
    endLowerLeft = endUpperLeft;
    endLowerRight = endUpperRight;
    row++;
  }

}


int
SoVolumeDataSlice::calcPageIdx(int row, int col) const
{
  assert((row >= 0) && (row < this->numRows));
  assert((col >= 0) && (col < this->numCols));

  return (row * this->numCols) + col;
}

/*!
  Builds a page if it doesn't exist. Rebuilds it if it does exist.
*/
SoVolumeDataPage *
SoVolumeDataSlice::buildPage(int col, int row,
                             SoTransferFunction * transferFunction)
{
  assert(this->reader);
  assert(transferFunction);

  // Does the page exist already?
  SoVolumeDataPage * page = this->getPage(col, row, transferFunction);
  if (!page) {
    // First SoVolumeDataPage ever in this slice?
    if (this->pages == NULL) {
      int nrpages = this->numCols * this->numRows;
      this->pages = new SoVolumeDataPageItem*[nrpages];
      for (int i=0; i < nrpages; i++) { this->pages[i] = NULL; }
    }

    page = new SoVolumeDataPage;
    SoVolumeDataPageItem * pitem = new SoVolumeDataPageItem(page);

    SoVolumeDataPageItem * p = this->pages[this->calcPageIdx(row, col)];
    if (p == NULL) {
      this->pages[this->calcPageIdx(row, col)] = pitem;
    }
    else {
      while (p->next != NULL) { p = p->next; }
      p->next = pitem;
      pitem->prev = p;
    }
  }

  SbBox2s subSlice = SbBox2s(col * this->pageSize[0],
                             row * this->pageSize[1],
                             (col + 1) * this->pageSize[0],
                             (row + 1) * this->pageSize[1]);

  void * transferredTexture = NULL;
  float * palette = NULL;
  int paletteDataType;
  int outputDataType;
  int paletteSize;
  int texturebuffersize = this->pageSize[0] * this->pageSize[1] * 4;
  unsigned char * texture = new unsigned char[texturebuffersize];

  SoVolumeReader::Axis ax =
    this->axis == SoOrthoSlice::X ?
    SoVolumeReader::X : (this->axis == SoOrthoSlice::Y ?
                         SoVolumeReader::Y : SoVolumeReader::Z);
  reader->getSubSlice(subSlice, sliceIdx, texture, ax);

  transferFunction->transfer(texture,
                             this->dataType,
                             this->pageSize,
                             transferredTexture,
                             outputDataType,
                             palette,
                             paletteDataType,
                             paletteSize);

  delete[] texture;

  page->setData(SoVolumeDataPage::OPENGL,
                (unsigned char*)transferredTexture,
                this->pageSize,
                palette,
                paletteDataType,
                paletteSize);

  // FIXME: this is used to invalidate a page (as far as I can see),
  // but AFAICT there is no code for actually cleaning it out (or is
  // that handled automatically by the LRU
  // algos?). Investigate. 20021111 mortene.
  page->transferFunctionId = transferFunction->getNodeId();

  delete[] ((char*) transferredTexture);
  delete[] palette;

  this->numTexels += this->pageSize[0] * this->pageSize[1];
  this->numPages++;
  this->numBytesSW += page->numBytesSW;
  this->numBytesHW += page->numBytesHW;

  return page;
}


SoVolumeDataPage *
SoVolumeDataSlice::getPage(int col, int row,
                           SoTransferFunction * transferFunction)
{
  if (this->pages == NULL) return NULL;

  assert((col < this->numCols) && (row < this->numRows));

  SoVolumeDataPageItem * p = this->pages[this->calcPageIdx(row, col)];

  while (p != NULL) {
    if (p->page->transferFunctionId == transferFunction->getNodeId()) break;
    p = p->next;
  }

  return p ? p->page : NULL;
}