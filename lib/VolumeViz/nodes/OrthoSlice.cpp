#include <VolumeViz/nodes/SoOrthoSlice.h>

#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/system/gl.h>
#include <VolumeViz/elements/SoTransferFunctionElement.h>
#include <VolumeViz/elements/SoVolumeDataElement.h>
#include <VolumeViz/nodes/SoVolumeData.h>
#include <VolumeViz/render/2D/Cvr2DTexPage.h>

// *************************************************************************

SO_NODE_SOURCE(SoOrthoSlice);

// *************************************************************************

class SoOrthoSliceP {
public:
  SoOrthoSliceP(SoOrthoSlice * master)
  {
    this->master = master;
  }

  static void renderBox(SoGLRenderAction * action, SbBox3f box);

  void getPageGeometry(SoVolumeData * volumedata, SbVec3f & origo, SbVec3f & horizspan, SbVec3f & verticalspan) const;
  
private:
  SoOrthoSlice * master;
};

#define PRIVATE(p) (p->pimpl)
#define PUBLIC(p) (p->master)

// *************************************************************************

SoOrthoSlice::SoOrthoSlice(void)
{
  SO_NODE_CONSTRUCTOR(SoOrthoSlice);

  PRIVATE(this) = new SoOrthoSliceP(this);

  SO_NODE_DEFINE_ENUM_VALUE(Axis, X);
  SO_NODE_DEFINE_ENUM_VALUE(Axis, Y);
  SO_NODE_DEFINE_ENUM_VALUE(Axis, Z);
  SO_NODE_SET_SF_ENUM_TYPE(axis, Axis);

  SO_NODE_DEFINE_ENUM_VALUE(Interpolation, NEAREST);
  SO_NODE_DEFINE_ENUM_VALUE(Interpolation, LINEAR);
  SO_NODE_SET_SF_ENUM_TYPE(interpolation, Interpolation);

  SO_NODE_DEFINE_ENUM_VALUE(AlphaUse, ALPHA_AS_IS);
  SO_NODE_DEFINE_ENUM_VALUE(AlphaUse, ALPHA_OPAQUE);
  SO_NODE_DEFINE_ENUM_VALUE(AlphaUse, ALPHA_BINARY);
  SO_NODE_SET_SF_ENUM_TYPE(alphaUse, AlphaUse);

  SO_NODE_DEFINE_ENUM_VALUE(ClippingSide, FRONT);
  SO_NODE_DEFINE_ENUM_VALUE(ClippingSide, BACK);
  SO_NODE_SET_SF_ENUM_TYPE(clippingSide, ClippingSide);

  SO_NODE_ADD_FIELD(sliceNumber, (0));
  SO_NODE_ADD_FIELD(axis, (Z));
  SO_NODE_ADD_FIELD(interpolation, (LINEAR));
  SO_NODE_ADD_FIELD(alphaUse, (ALPHA_BINARY));
  SO_NODE_ADD_FIELD(clippingSide, (BACK));
  SO_NODE_ADD_FIELD(clipping, (FALSE));
}

SoOrthoSlice::~SoOrthoSlice()
{
  delete PRIVATE(this);
}

// Doc from parent class.
void
SoOrthoSlice::initClass(void)
{
  SO_NODE_INIT_CLASS(SoOrthoSlice, SoShape, "SoShape");

  SO_ENABLE(SoGLRenderAction, SoVolumeDataElement);
  SO_ENABLE(SoGLRenderAction, SoTransferFunctionElement);

  SO_ENABLE(SoRayPickAction, SoVolumeDataElement);
  SO_ENABLE(SoRayPickAction, SoTransferFunctionElement);
}

// doc in super
SbBool
SoOrthoSlice::affectsState(void) const
{
  return this->clipping.getValue();
}

void
SoOrthoSliceP::getPageGeometry(SoVolumeData * volumedata,
                               SbVec3f & origo,
                               SbVec3f & horizspan,
                               SbVec3f & verticalspan) const
{
  SbBox3f spacesize = volumedata->getVolumeSize();
  SbVec3f spacemin, spacemax;
  spacesize.getBounds(spacemin, spacemax);

  const int axis = PUBLIC(this)->axis.getValue();

  const SbBox2f QUAD = (axis == SoOrthoSlice::Z) ?
    SbBox2f(spacemin[0], spacemin[1], spacemax[0], spacemax[1]) :
    ((axis == SoOrthoSlice::X) ?
     SbBox2f(spacemin[2], spacemin[1], spacemax[2], spacemax[1]) :
     // then it's along Y
     SbBox2f(spacemin[0], spacemin[2], spacemax[0], spacemax[2]));

  SbVec2f qmax, qmin;
  QUAD.getBounds(qmin, qmax);

  SbVec3s dimensions;
  void * data;
  SoVolumeData::DataType type;
  SbBool ok = volumedata->getVolumeData(dimensions, data, type);
  assert(ok);

  const float depthprslice = (spacemax[axis] - spacemin[axis]) / dimensions[axis];
  const float depth = spacemin[axis] + PUBLIC(this)->sliceNumber.getValue() * depthprslice;

  switch (axis) {
  case SoOrthoSlice::X: origo = SbVec3f(depth, qmax[1], qmin[0]); break;
  case SoOrthoSlice::Y: origo = SbVec3f(qmin[0], depth, qmin[1]); break;
  case SoOrthoSlice::Z: origo = SbVec3f(qmin[0], qmax[1], depth); break;
  default: assert(FALSE); break;
  }

  const float width = qmax[0] - qmin[0];
  const float height = qmax[1] - qmin[1];

  switch (axis) {
  case 0:
    horizspan = SbVec3f(0, 0, width);
    verticalspan = SbVec3f(0, -height, 0);
    break;
  case 1:
    // The last component is "flipped" to make the y-direction slices
    // not come out upside-down. FIXME: should really investigate if
    // this is the correct fix. 20021124 mortene.
    horizspan = SbVec3f(width, 0, 0);
    verticalspan = SbVec3f(0, 0, height);
    break;
  case 2:
    horizspan = SbVec3f(width, 0, 0);
    verticalspan = SbVec3f(0, -height, 0);
    break;
  default: assert(FALSE); break;
  }

  const SbVec3f SCALE((spacemax[0] - spacemin[0]) / dimensions[0],
                      (spacemax[1] - spacemin[1]) / dimensions[1],
                      (spacemax[2] - spacemin[2]) / dimensions[2]);

  const SbVec2f QUADSCALE = (axis == 2) ?
    SbVec2f(SCALE[0], SCALE[1]) :
    ((axis == 0) ?
     SbVec2f(SCALE[2], SCALE[1]) :
     SbVec2f(SCALE[0], SCALE[2]));

  horizspan.normalize();
  horizspan *= QUADSCALE[0];
  verticalspan.normalize();
  verticalspan *= QUADSCALE[1];
}

void
SoOrthoSlice::GLRender(SoGLRenderAction * action)
{
  // FIXME: need to make sure we're not cached in a renderlist

  SbBox3f slicebox;
  SbVec3f dummy;
  this->computeBBox(action, slicebox, dummy);

  // debug
  SoOrthoSliceP::renderBox(action, slicebox);

  // Fetching the current volumedata
  SoState * state = action->getState();
  const SoVolumeDataElement * volumedataelement = SoVolumeDataElement::getInstance(state);
  assert(volumedataelement != NULL);
  SoVolumeData * volumedata = volumedataelement->getVolumeData();
  assert(volumedata != NULL);

  // XXX FIXME XXX cache pages
  Cvr2DTexPage * page = new Cvr2DTexPage();
  page->init(volumedata->getReader(), this->sliceNumber.getValue(),
             this->axis.getValue(), SbVec2s(64, 64) /* subpagetexsize */);

  SbVec3f origo, horizspan, verticalspan;
  PRIVATE(this)->getPageGeometry(volumedata, origo, horizspan, verticalspan);
  page->render(action, origo, horizspan, verticalspan,
               // FIXME: ugly cast
               (Cvr2DTexSubPage::Interpolation)this->interpolation.getValue());
}

void
SoOrthoSlice::rayPick(SoRayPickAction * action)
{
  // FIXME: implement
}

void
SoOrthoSlice::generatePrimitives(SoAction * action)
{
  // FIXME: implement
}

// doc in super
void
SoOrthoSlice::computeBBox(SoAction * action, SbBox3f & box, SbVec3f & center)
{
  SoState * state = action->getState();

  const SoVolumeDataElement * volumedataelement =
    SoVolumeDataElement::getInstance(state);
  if (volumedataelement == NULL) return;
  SoVolumeData * volumedata = volumedataelement->getVolumeData();
  if (volumedata == NULL) return;

  SbBox3f vdbox = volumedata->getVolumeSize();
  SbVec3f bmin, bmax;
  vdbox.getBounds(bmin, bmax);

  SbVec3s dimensions;
  void * data;
  SoVolumeData::DataType type;
  SbBool ok = volumedata->getVolumeData(dimensions, data, type);
  assert(ok);

  const int axisidx = (int)axis.getValue();
  const int slice = this->sliceNumber.getValue();
  const float depth = (float)fabs(bmax[axisidx] - bmin[axisidx]);

  bmin[axisidx] = bmax[axisidx] =
    (bmin[axisidx] + (depth / dimensions[axisidx]) * slice);

  vdbox.setBounds(bmin, bmax);
  box.extendBy(vdbox);
  center = vdbox.getCenter();
}

// *************************************************************************

// Render 3D box.
//
// FIXME: should make bbox debug rendering part of SoSeparator /
// SoGroup. 20030305 mortene.
void
SoOrthoSliceP::renderBox(SoGLRenderAction * action, SbBox3f box)
{
  SbVec3f bmin, bmax;
  box.getBounds(bmin, bmax);

  // FIXME: push attribs

  glDisable(GL_TEXTURE_2D);
  glLineStipple(1, 0xffff);
  glLineWidth(2);

  glBegin(GL_LINES);

  glVertex3f(bmin[0], bmin[1], bmin[2]);
  glVertex3f(bmin[0], bmin[1], bmax[2]);

  glVertex3f(bmax[0], bmin[1], bmin[2]);
  glVertex3f(bmax[0], bmin[1], bmax[2]);

  glVertex3f(bmax[0], bmax[1], bmin[2]);
  glVertex3f(bmax[0], bmax[1], bmax[2]);

  glVertex3f(bmin[0], bmax[1], bmin[2]);
  glVertex3f(bmin[0], bmax[1], bmax[2]);


  glVertex3f(bmin[0], bmin[1], bmin[2]);
  glVertex3f(bmax[0], bmin[1], bmin[2]);

  glVertex3f(bmin[0], bmin[1], bmax[2]);
  glVertex3f(bmax[0], bmin[1], bmax[2]);

  glVertex3f(bmin[0], bmax[1], bmax[2]);
  glVertex3f(bmax[0], bmax[1], bmax[2]);

  glVertex3f(bmin[0], bmax[1], bmin[2]);
  glVertex3f(bmax[0], bmax[1], bmin[2]);


  glVertex3f(bmin[0], bmin[1], bmin[2]);
  glVertex3f(bmin[0], bmax[1], bmin[2]);

  glVertex3f(bmin[0], bmin[1], bmax[2]);
  glVertex3f(bmin[0], bmax[1], bmax[2]);

  glVertex3f(bmax[0], bmin[1], bmax[2]);
  glVertex3f(bmax[0], bmax[1], bmax[2]);

  glVertex3f(bmax[0], bmin[1], bmin[2]);
  glVertex3f(bmax[0], bmax[1], bmin[2]);

  glEnd();

  // FIXME: pop attribs

  glEnable(GL_TEXTURE_2D);
}

// *************************************************************************
