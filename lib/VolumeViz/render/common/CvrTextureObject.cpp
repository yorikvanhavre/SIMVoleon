/**************************************************************************\
 *
 *  This file is part of the SIM Voleon visualization library.
 *  Copyright (C) 2003-2004 by Systems in Motion.  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  ("GPL") version 2 as published by the Free Software Foundation.
 *  See the file LICENSE.GPL at the root directory of this source
 *  distribution for additional information about the GNU GPL.
 *
 *  For using SIM Voleon with software that can not be combined with
 *  the GNU GPL, and for taking advantage of the additional benefits
 *  of our support services, please contact Systems in Motion about
 *  acquiring a SIM Voleon Professional Edition License.
 *
 *  See <URL:http://www.coin3d.org/> for more information.
 *
 *  Systems in Motion, Teknobyen, Abels Gate 5, 7030 Trondheim, NORWAY.
 *  <URL:http://www.sim.no/>.
 *
\**************************************************************************/

#include <VolumeViz/render/common/CvrTextureObject.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include <assert.h>
#include <limits.h>

#include <Inventor/C/glue/gl.h>
#include <Inventor/C/tidbits.h>
#include <Inventor/SbBox2s.h>
#include <Inventor/SbName.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/errors/SoDebugError.h>

#include <VolumeViz/caches/CvrGLTextureCache.h>
#include <VolumeViz/elements/CvrCompressedTexturesElement.h>
#include <VolumeViz/elements/CvrGLInterpolationElement.h>
#include <VolumeViz/elements/CvrVoxelBlockElement.h>
#include <VolumeViz/misc/CvrCLUT.h>
#include <VolumeViz/misc/CvrUtil.h>
#include <VolumeViz/misc/CvrVoxelChunk.h>
#include <VolumeViz/render/common/Cvr2DRGBATexture.h>
#include <VolumeViz/render/common/Cvr2DPaletteTexture.h>
#include <VolumeViz/render/common/Cvr3DRGBATexture.h>
#include <VolumeViz/render/common/Cvr3DPaletteTexture.h>
#include <VolumeViz/render/common/resourcehandler.h>

// *************************************************************************

// FIXME: suggestion found on comp.graphics.api.opengl on how to check
// how much texture VRAM is available:
//
// [...]
// Once you create texture(s), you can give it(them) a priority using
// glPrioritizeTexture
// (http://www.3dlabs.com/support/developer/GLmanpages/glprioritizetextures.htm)
// 
// this should tell the ICD to put textures with the highest priority
// (and according to available texture memory on your hardware) into
// the actual video ram on you sweet geforce (or whatever card you
// happen to use:-).
// 
// and then, you can see if that texture(s) is really resident (stored
// in vram) by calling glAreTexturesResident
// (http://www.3dlabs.com/support/developer/GLmanpages/glaretexturesresident.htm)
// 
// So if you want to test how many textures you can cram into your
// vram, you can just keep on creating new textures and setting their
// priority to high (1) and for every new one you create see if it is
// really stored in vram.  Keep in mind that you need to use a given
// texture at least once before it can be "prioritized", put into
// vram.
// 
// This is really really cool, because say you want to use 10 textures
// interchangeably. If you put them in vram then calls to
// glBindTexture will be ultra fricking fast, because the texture will
// no longer have to go through the AGP bus.
// [...]
//
// 20021130 mortene.
//
// Update 20021201 mortene: I think the technique above will be
// dangerous on UMA-machines (like the SGI O2) were tex mem is the
// same as other system mem. One would at least have to set a upper
// limit before running the test.


// For reference, here's some information from Thomas Roell of Xi
// Graphics on glPrioritizeTextures() from c.g.a.opengl:
//
// [...]
//
//   Texture priorities would be a nice thing, but only few OpenGL
//   implementations actually use them. There are a lot of reasons
//   that they ignore priorities. One is that the default priority is
//   set to 1.0, which is the highest priority. That means unless all
//   textures for all you applications running at the same time
//   explicitely use texture priorities, the one that uses them
//   (i.e. lower priorities) will be at a disadvantage. The other
//   problem is that typically textures are not the only objects that
//   live in HW accessable memory. There are display lists, color
//   tables, vertex array objects and so on. However there is no way
//   to prioritize them. Hence if you are using textures and display
//   lists at the same time, useng priorities might cause a lot of
//   texture cache trashing.
//
// [...]
//
// 20021201 mortene.

// *************************************************************************

// Don't set value explicitly to SoType::badType(), to avoid a bug in
// Sun CC v4.0. (Bitpattern 0x0000 equals SoType::badType()).
SoType CvrTextureObject::classTypeId;

SoType CvrTextureObject::getTypeId(void) const { return CvrTextureObject::classTypeId; }
SoType CvrTextureObject::getClassTypeId(void) { return CvrTextureObject::classTypeId; }

// *************************************************************************

SbDict * CvrTextureObject::instancedict = NULL;

// *************************************************************************

void
CvrTextureObject::initClass(void)
{
  assert(CvrTextureObject::classTypeId == SoType::badType());
  CvrTextureObject::classTypeId = SoType::createType(SoType::badType(),
                                                     "CvrTextureObject");

  CvrRGBATexture::initClass();
  CvrPaletteTexture::initClass();
  Cvr2DRGBATexture::initClass();
  Cvr2DPaletteTexture::initClass();
  Cvr3DRGBATexture::initClass();
  Cvr3DPaletteTexture::initClass();

  // FIXME: leak, never deallocated. 20040721 mortene.
  CvrTextureObject::instancedict = new SbDict;
}

CvrTextureObject::CvrTextureObject(void)
{
  assert(CvrTextureObject::classTypeId != SoType::badType());
  this->refcounter = 0;
}

CvrTextureObject::~CvrTextureObject()
{
  // Kill all existing CvrGLTextureCache instances (which will
  // indirectly deallocate GL textures):

  SbPList keys, values;
  this->glctxdict.makePList(keys, values);
  for (unsigned int i = 0; i < (unsigned int)values.getLength(); i++) {
    SbList<CvrGLTextureCache *> * l = (SbList<CvrGLTextureCache *> *)values[i];
    for (unsigned int j = 0; j < (unsigned int)l->getLength(); j++) { (*l)[j]->unref(); }
    delete l;
  }
  this->glctxdict.clear();


  // Take us out of the static list of all CvrTextureObject instances:

  const unsigned long key = this->hashKey();
  void * ptr;
  const SbBool ok = CvrTextureObject::instancedict->find(key, ptr);
  assert(ok);

  // Calculated hash key is not guaranteed to be unique, so a list is
  // stored in the hash, which we must do comparisons on the elements
  // in.
  SbList<CvrTextureObject *> * l = (SbList<CvrTextureObject *> *)ptr;
  const int idx = l->find(this);
  assert(idx != -1);
  l->removeFast(idx);

  if (l->getLength() == 0) {
    delete l;
    const SbBool ok = CvrTextureObject::instancedict->remove(key);
    assert(ok);
  }
}

// *************************************************************************

const SbVec3s &
CvrTextureObject::getDimensions(void) const
{
  return this->dimensions;
}

// *************************************************************************

SbList<CvrGLTextureCache *> *
CvrTextureObject::cacheListForGLContext(const uint32_t glctxid) const
{
  void * ptr;
  const SbBool found = this->glctxdict.find((unsigned long)glctxid, ptr);
  if (!found) { return NULL; }
  return (SbList<CvrGLTextureCache *> *)ptr;
}

SbBool
CvrTextureObject::findGLTexture(const SoGLRenderAction * action, GLuint & texid) const
{
  SbList<CvrGLTextureCache *> * cachelist =
    this->cacheListForGLContext(action->getCacheContext());
  if (cachelist == NULL) { return FALSE; }

  for (int i=0; i < cachelist->getLength(); i++) {
    CvrGLTextureCache * cache = (*cachelist)[i];

    if (cache->isDead()) {
      cache->unref();
      cachelist->remove(i);
      i--;
      continue;
    }

    if (cache->isValid(action->getState())) {
      texid = cache->getGLTextureId();
      return TRUE;
    }
  }
  return FALSE;
}

GLuint 
CvrTextureObject::getGLTexture(const SoGLRenderAction * action) const
{
  GLuint texid;
  if (this->findGLTexture(action, texid)) { return texid; }

  SoState * state = action->getState();

  // FIXME: why is this necessary? Investigate. 20040722 mortene.
  const SbBool storedinvalid = SoCacheElement::setInvalid(FALSE);

  // FIXME: in SoAsciiText, pederb uses this right after making a
  // cache -- what does this do?:
  //
  //    * SoCacheElement::addCacheDependency(state, cache); ???
  //
  // 20040722 mortene.

  CvrGLTextureCache * cache = new CvrGLTextureCache(state);
  cache->ref();

  // FIXME: check up exactly why this is done. 20040722 mortene.
  state->push();
  SoCacheElement::set(state, cache);

  // Default values is for RGBA-texture:
  int colorformat = 4;
  unsigned int bitspertexel = 32;  // 8 bits each R, G, B & A
  if (this->isPaletted()) {
    colorformat = GL_COLOR_INDEX8_EXT;
    bitspertexel = 8;
  }

  const SbVec3s texdims = this->getDimensions();

  // FIXME: resource usage counting not implemented yet. 20040716 mortene.
#if 0 // tmp disabled
  const unsigned int nrtexels = texdims[0] * texdims[1] * texdims[2];
  const unsigned int texmem = (unsigned int)(nrtexels * bitspertexel / 8);
#endif

  // FIXME: glGenTextures() / glBindTexture() / glDeleteTextures() are
  // only supported in opengl >= 1.1, should have a fallback for 1.0
  // drivers, like we have in Coin, where we can use display lists
  // instead. 20040715 mortene.
  glGenTextures(1, &texid);
  assert(glGetError() == GL_NO_ERROR);

  const unsigned short nrtexdims = this->getNrOfTextureDimensions();
  const GLenum gltextypeenum = (nrtexdims == 2) ? GL_TEXTURE_2D : GL_TEXTURE_3D;

  glEnable(gltextypeenum);
  glBindTexture(gltextypeenum, texid);
  assert(glGetError() == GL_NO_ERROR);

  const uint32_t glctxid = action->getCacheContext();
  const cc_glglue * glw = cc_glglue_instance(glctxid);

  GLint wrapenum = GL_CLAMP;
  // FIXME: avoid using GL_CLAMP_TO_EDGE, since it may not be
  // available on all drivers. (Notably, it is missing from the
  // Microsoft OpenGL 1.1 software renderer, which is often used for
  // offscreen rendering on MSWin systems.)
  //
  // Disable this code, and fix any visual artifacts showing up. See
  // also description of bug #012 in SIMVoleon/BUGS.txt.
  //
  // 20040714 mortene.

  if (cc_glglue_has_texture_edge_clamp(glw) && (nrtexdims == 3)) {
    // We do this for now, to minimize the visible seams in the 3D
    // textures when interpolation is set to "LINEAR". See FIXME above
    // and bug #012.
    //
    // This work-around only active for 3D textures, as we can be
    // fairly certain we have at least OpenGL 1.2 (to which was added
    // both 3D textures and GL_CLAMP_TO_EDGE) if we get here. Besides,
    // the seams can be very ugly with 3D textures, but are usually
    // not easily visible with 2D textures, so there's less help in
    // this.
    wrapenum = GL_CLAMP_TO_EDGE;
  }

  glTexParameteri(gltextypeenum, GL_TEXTURE_WRAP_S, wrapenum);
  glTexParameteri(gltextypeenum, GL_TEXTURE_WRAP_T, wrapenum);
  if (nrtexdims == 3) {
    glTexParameteri(gltextypeenum, GL_TEXTURE_WRAP_R, wrapenum);
  }

  assert(glGetError() == GL_NO_ERROR);

  void * imgptr = NULL;
  if (this->isPaletted()) imgptr = ((CvrPaletteTexture *)this)->getIndex8Buffer();
  else imgptr = ((CvrRGBATexture *)this)->getRGBABuffer();


  int palettetype = GL_COLOR_INDEX;
#ifdef HAVE_ARB_FRAGMENT_PROGRAM
  if (cc_glglue_has_arb_fragment_program(glw)) { palettetype = GL_LUMINANCE; }
#endif // HAVE_ARB_FRAGMENT_PROGRAM

  // NOTE: Combining texture compression and GL_COLOR_INDEX doesn't
  // seem to work on NVIDIA cards (tested on GeForceFX 5600 &
  // GeForce2 MX) (20040316 handegar)
  //
  // FIXME: check if that is a general GL limitation. (I seem to
  // remember it is.) 20040716 mortene.
  if (cc_glue_has_texture_compression(glw) &&
      (palettetype != GL_COLOR_INDEX) &&
      // Important to check this last, as we want to avoid getting an
      // unnecessary cache dependency:
      CvrCompressedTexturesElement::get(state)) {
    if (colorformat == 4) colorformat = GL_COMPRESSED_RGBA_ARB;
    else colorformat = GL_COMPRESSED_INTENSITY_ARB;
  }

  // By default we modulate textures with the material settings.
  if (!CvrUtil::dontModulateTextures()) {
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  }

  if (nrtexdims == 2) {
    glTexImage2D(gltextypeenum,
                 0,
                 colorformat,
                 texdims[0], texdims[1],
                 0,
                 this->isPaletted() ? palettetype : GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 imgptr);
  }
  else {
    assert(nrtexdims == 3);
    cc_glglue_glTexImage3D(glw,
                           gltextypeenum,
                           0,
                           colorformat,
                           texdims[0], texdims[1], texdims[2],
                           0,
                           this->isPaletted() ? palettetype : GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           imgptr);
  }

  assert(glGetError() == GL_NO_ERROR);

  cache->setGLTextureId(action, texid);

  SbList<CvrGLTextureCache *> * l = this->cacheListForGLContext(glctxid);
  if (l == NULL) {
    l = new SbList<CvrGLTextureCache *>;
    ((CvrTextureObject *)this)->glctxdict.enter((unsigned long)glctxid, l);
  }
  l->append(cache);

  state->pop();
  SoCacheElement::setInvalid(storedinvalid);

  return texid;
}

// *************************************************************************

uint32_t
CvrTextureObject::getRefCount(void) const
{
  return this->refcounter;
}

void
CvrTextureObject::ref(void) const
{
  ((CvrTextureObject *)this)->refcounter++;
}

void 
CvrTextureObject::unref(void) const
{
  assert(this->refcounter > 0);
  ((CvrTextureObject *)this)->refcounter--;
  if (this->refcounter == 0) { delete this; }
}

// *************************************************************************

CvrTextureObject *
CvrTextureObject::findInstanceMatch(const SoType t,
                                    const struct CvrTextureObject::EqualityComparison & obj)
{
  const unsigned long key = CvrTextureObject::hashKey(obj);
  void * ptr;
  const SbBool ok = CvrTextureObject::instancedict->find(key, ptr);
  if (!ok) { return NULL; }

  // Calculated hash key is not guaranteed to be unique, so a list is
  // stored in the hash, which we must do comparisons on the elements
  // in.
  SbList<CvrTextureObject *> * l = (SbList<CvrTextureObject *> *)ptr;

  for (int i = 0; i < l->getLength(); i++) {
    CvrTextureObject * to = (*l)[i];
    if ((to->eqcmp == obj) && (to->getTypeId() == t)) { return to; }
  }
  
  return NULL;
}

/*! Returns an instance which embodies a chunk of voxels out of the
    current SoVolumeData on the state stack, as given by the \a
    cutcube argument.

    Automatically takes care of sharing if an instance was already
    made to the same specifications.
*/
const CvrTextureObject *
CvrTextureObject::create(const SoGLRenderAction * action,
                         const SbVec3s & texsize,
                         const SbBox3s & cutcube)
{
  const SbBox2s dummy; // constructor initializes it to an empty box
  return CvrTextureObject::create(action, texsize, cutcube, dummy, UINT_MAX, INT_MAX);
}

const CvrTextureObject *
CvrTextureObject::create(const SoGLRenderAction * action,
                         const SbVec2s & texsize,
                         const SbBox2s & cutslice,
                         const unsigned int axisidx,
                         const int pageidx)
{
  const SbVec3s tex(texsize[0], texsize[1], 1);
  const SbBox3s dummy; // constructor initializes it to an empty box

  return CvrTextureObject::create(action, tex, dummy, cutslice, axisidx, pageidx);
}

// The common create function, used for both 2D and 3D cuts of the
// volume.
CvrTextureObject *
CvrTextureObject::create(const SoGLRenderAction * action,
                         /* common: */ const SbVec3s & texsize,
                         /* 3D only: */ const SbBox3s & cutcube,
                         /* 2D only: */ const SbBox2s & cutslice, const unsigned int axisidx, const int pageidx)
{
  const CvrVoxelBlockElement * vbelem = CvrVoxelBlockElement::getInstance(action->getState());
  assert(vbelem != NULL);

  const SbBool paletted = CvrCLUT::usePaletteTextures(action);
  const SbBool is2d = (axisidx != UINT_MAX);

  SoType t;
  if (is2d && paletted) { t = Cvr2DPaletteTexture::getClassTypeId(); }
  else if (is2d) { t = Cvr2DRGBATexture::getClassTypeId(); }
  else if (paletted) { t = Cvr3DPaletteTexture::getClassTypeId(); }
  else { t = Cvr3DRGBATexture::getClassTypeId(); }

  struct CvrTextureObject::EqualityComparison incoming;
  incoming.sovolumedata_id = vbelem->getNodeId();
  incoming.cutcube = cutcube; // For 3D tex
  incoming.cutslice = cutslice; // For 2D tex
  incoming.axisidx = axisidx; // For 2D tex
  incoming.pageidx = pageidx; // For 2D tex
  
  CvrTextureObject * obj = CvrTextureObject::findInstanceMatch(t, incoming);
  if (obj) { return obj; }

  const SbVec3s & voxdims = vbelem->getVoxelCubeDimensions();
  const void * dataptr = vbelem->getVoxels();

  // FIXME: improve buildSubPage() interface to fix this roundabout
  // way of calling it. 20021206 mortene.
  CvrVoxelChunk * input =
    new CvrVoxelChunk(voxdims, vbelem->getBytesPrVoxel(), dataptr);
  CvrVoxelChunk * cubechunk;
  if (is2d) { cubechunk = input->buildSubPage(axisidx, pageidx, cutslice); }
  else { cubechunk = input->buildSubCube(cutcube); }
  delete input;

  CvrTextureObject * newtexobj = (CvrTextureObject *)t.createInstance();

  // The actual dimensions of the GL texture must be values that are
  // power-of-two's:
  for (unsigned int i=0; i < 3; i++) {
    newtexobj->dimensions[i] = coin_geq_power_of_two(texsize[i]);
  }

  SbBool invisible = FALSE;
  cubechunk->transfer(action, newtexobj, invisible);
  delete cubechunk;

  // If completely transparent
  if (invisible) { return NULL; }

  // Must clear unused texture area to prevent artifacts due to
  // floating point inaccuracies when calculating texture coords.
  newtexobj->blankUnused(texsize);

  // We'll self-destruct when the SoVolumeData node is changed.
  //
  // FIXME: need to implement the self-destruction mechanism. Should
  // attach an SoNodeSensor to the SoVolumeData-node, have a
  // "about-to-be-destroyed" callback for the owners/auditors of
  // newtexobj, etc etc. 20040721 mortene.
  //
  // UPDATE: ..or is this already taken care of higher up in the
  // call-chain? I think it may be. Investigate. 20040722 mortene.
  newtexobj->eqcmp = incoming;

  const unsigned long key = newtexobj->hashKey();
  void * ptr;
  const SbBool ok = CvrTextureObject::instancedict->find(key, ptr);
  SbList<CvrTextureObject *> * l;
  if (ok) {
    l = (SbList<CvrTextureObject *> *)ptr;
  }
  else {
    l = new SbList<CvrTextureObject *>;
    const SbBool newentry = CvrTextureObject::instancedict->enter(key, l);
    assert(newentry);
  }
  l->append(newtexobj);

  return newtexobj;
}

// *************************************************************************

void
CvrTextureObject::activateTexture(const SoGLRenderAction * action) const
{
  const GLuint texid = this->getGLTexture(action);

  const unsigned short nrtexdims = this->getNrOfTextureDimensions();
  const GLenum gltextypeenum = (nrtexdims == 2) ? GL_TEXTURE_2D : GL_TEXTURE_3D;

  glEnable(gltextypeenum);
  glBindTexture(gltextypeenum, texid);

  const GLenum interp = CvrGLInterpolationElement::get(action->getState());
  glTexParameteri(gltextypeenum, GL_TEXTURE_MAG_FILTER, interp);
  glTexParameteri(gltextypeenum, GL_TEXTURE_MIN_FILTER, interp);

  assert(glGetError() == GL_NO_ERROR);

#if CVR_DEBUG && 0 // debug
  // FIXME: glAreTexturesResident() is OpenGL 1.1 only. 20021119 mortene.
  GLboolean residences[1];
  GLboolean resident = glAreTexturesResident(1, &texid, residences);
  if (!resident) {
    SoDebugError::postWarning("Cvr2DTexSubPage::activateTexture",
                              "texture %d not resident", texid);
    Cvr2DTexSubPage::detectedtextureswapping = TRUE;
  }

  // For reference, here's some information from Thomas Roell of Xi
  // Graphics on glAreTexturesResident() from c.g.a.opengl:
  //
  // [...]
  //
  //   With regards to glAreTexturesResident(), this is kind of
  //   tricky. This function returns which textures are currently
  //   resident is HW accessable memory (AGP, FB, TB). It does not
  //   return whether a set of textures could be made resident at a
  //   future point of time. A lot of OpenGL implementations (APPLE &
  //   XiGraphics for example) do cache a texture upon first use with
  //   3D primitive. Hence unless you had used a texture before it
  //   will not be resident. N.b that usually operations like
  //   glBindTexture, glTex*Image and so on will not make a texture
  //   resident for such caching implementations.
  //
  // [...]
  //
  // Additional information from Ian D Romanick (IBM engineer doing
  // Linux OpenGL work):
  //
  // [...]
  //
  //   AreTexturesResident is basically worthless, IMO.  All OpenGL
  //   rendering happens in a VERY high latency pipeline.  When an
  //   application calls AreTexturesResident, the textures may all be
  //   resident at that time.  However, there may already be
  //   primitives in the pipeline that will cause those textures to be
  //   removed from texturable memory before more primitives can be
  //   put in the pipe.
  //
  // [...]
  //
  // 20021201 mortene.

#endif // debug
}

// *************************************************************************

unsigned long
CvrTextureObject::hashKey(void) const
{
  return CvrTextureObject::hashKey(this->eqcmp);
}

unsigned long
CvrTextureObject::hashKey(const struct CvrTextureObject::EqualityComparison & obj)
{
  unsigned long key = obj.sovolumedata_id;

  if (obj.axisidx != UINT_MAX) { key += obj.axisidx; }
  if (obj.pageidx != INT_MAX) { key += obj.pageidx; }

  SbBox3s empty3;
  if (obj.cutcube.getMin() != empty3.getMin()) {
    short v[6];
    obj.cutcube.getBounds(v[0], v[1], v[2], v[3], v[4], v[5]);
    for (unsigned int i = 0; i < 6; i++) { key += v[i]; }
  }

  SbBox2s empty2;
  if (obj.cutslice.getMin() != empty2.getMin()) {
    short v[4];
    obj.cutslice.getBounds(v[0], v[1], v[2], v[3]);
    for (unsigned int i = 0; i < 4; i++) { key += v[i]; }
  }

  return key;
}

// *************************************************************************

int
CvrTextureObject::EqualityComparison::operator==(const CvrTextureObject::EqualityComparison & obj)
{
  return
    (this->sovolumedata_id == obj.sovolumedata_id) &&
    // Note: we compare SbBox3s corner points for the cutcube instead
    // of using the operator==() for SbBox3s, because the operator was
    // forgotten for export to the DLL interface up until and
    // including Coin version 2.3 (and SIM Voleon should be compatible
    // with anything from Coin 2.0 and upwards).
    (this->cutcube.getMin() == obj.cutcube.getMin()) &&
    (this->cutcube.getMax() == obj.cutcube.getMax()) &&
    (this->cutslice == obj.cutslice) &&
    (this->axisidx == obj.axisidx) &&
    (this->pageidx == obj.pageidx);
  
}

// *************************************************************************
