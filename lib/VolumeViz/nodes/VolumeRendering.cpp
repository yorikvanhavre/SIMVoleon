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

/*!
  \mainpage

  SIM Voleon is a software development system for aiding in writing 3D
  graphics application containing so-called "volumetric data", as is
  often used in applications targeted at analysis tasks within the oil
  & gas industry (e.g. from seismic measurements), in medical imaging
  (for data generated by e.g. Nuclear Magnetic Resonance, Computer
  Tomography, or ultrasound equipment), and for many other purposes.

  SIM Voleon is an add-on library to Systems in Motion's visualization
  platform technology, known as "Coin". Coin is a high-level 3D
  graphics library with a C++ Application Programming Interface. Its
  target audience is developers of 3D graphics applications worldwide.

  The library has the following major features:

  <ul>

  <li> It is fully portable over the full range of platforms also
  supported by the Coin library, including all versions of Microsoft
  Windows, Mac OS X, Linux, Sun Solaris, SGI IRIX, and most other 
  UNIX variants.</li>

  <li> Integrates smoothly with traditional polygonal 3D data
  visualization from the Coin library, in a manner which is effortless
  for the application programmer. </li>

  <li> Provides interaction possibilities for direct queries in the 3D
  scene on the volumetric data through picking support. </li>

  <li> Volumes are visualized by using functionality which is assisted
  by dedicated 3D hardware on most modern graphics cards, namely 2D
  texture mapping. </li>

  <li> Higher quality and better performance visualization is
  automatically done on graphics card which supports hardware assisted
  3D texture mapping. </li>

  <li> Colour transfer functions can be manipulated in realtime using
  the either the \c EXT_paletted_texture or the \c
  ARB_fragment_program OpenGL extensions. </li>

  <li> Maximum Intensity Projection and Sum Intensity Projection is
  supported through hardware if available. </li>

  <center>
   <img src="http://doc.coin3d.org/images/SIMVoleon/nodes/vol-spine.png">
  </center>
  
  </ul>

  Here's a simple usage example, which sets up a complete environment
  for voxel visualization with the SIM Voleon library:

  \code
  #include <Inventor/Qt/SoQt.h>
  #include <Inventor/Qt/viewers/SoQtExaminerViewer.h>
  #include <Inventor/nodes/SoSeparator.h>
  #include <VolumeViz/nodes/SoTransferFunction.h>
  #include <VolumeViz/nodes/SoVolumeData.h>
  #include <VolumeViz/nodes/SoVolumeRender.h>
  #include <VolumeViz/nodes/SoVolumeRendering.h>
  
  static uint8_t *
  generate8bitVoxelSet(SbVec3s & dim)
  {
    const size_t blocksize = dim[0] * dim[1] * dim[2];
    uint8_t * voxels = new uint8_t[blocksize];
    (void)memset(voxels, 0, blocksize);
  
    float t = 0;
  
    while (t < 50) {
      SbVec3f v(sin((t + 1.4234) * 1.9) * sin(t) * 0.45 + 0.5,
                cos((t * 2.5) - 10) * 0.45 + 0.5,
                cos((t - 0.23123) * 3) * sin(t + 0.5) * cos(t) * 0.45 + 0.5);
  
      assert(v[0] < 1.0f && v[1] < 1.0f && v[2] < 1.0f);
      const int nx = int(dim[0] * v[0]);
      const int ny = int(dim[1] * v[1]);
      const int nz = int(dim[2] * v[2]);
  
      const int memposition = nz*dim[0]*dim[1] + ny*dim[0] + nx;
      voxels[memposition] = (uint8_t)(255.0 * cos(t));
  
      t += 0.001;
    }
  
    return voxels;
  }
  
  int
  main(int argc, char ** argv)
  {
    QWidget * window = SoQt::init(argv[0]);
    SoVolumeRendering::init();
  
    SoSeparator * root = new SoSeparator;
    root->ref();
  
    SbVec3s dim = SbVec3s(64, 64, 64);
    uint8_t * voxeldata = generate8bitVoxelSet(dim);
  
    // Add SoVolumeData to scene graph
    SoVolumeData * volumedata = new SoVolumeData();
    volumedata->setVolumeData(dim, voxeldata, SoVolumeData::UNSIGNED_BYTE);
    root->addChild(volumedata);
  
    // Add TransferFunction (color map) to scene graph
    SoTransferFunction * transfunc = new SoTransferFunction();
    root->addChild(transfunc);
  
    // Add VolumeRender to scene graph
    SoVolumeRender * volrend = new SoVolumeRender();
    root->addChild(volrend);
  
    SoQtExaminerViewer * viewer = new SoQtExaminerViewer(window);
    viewer->setBackgroundColor(SbColor(0.1f, 0.3f, 0.5f));
    viewer->setSceneGraph(root);
  
    viewer->show();
    SoQt::show(window);
    SoQt::mainLoop();
    delete viewer;
  
    root->unref();
    delete[] voxeldata;
  
    return 0;
  }
  \endcode


  <b>Hardware requirements:</b> 
  <ul>

  <li>
  SIM Voleon can render volumes using both 2D and 3D textures. The
  former will demand three times more texture memory than the latter,
  due to the static axis alignment for the volume slices. 3D textures
  provide the best rendering quality.

  SIM Voleon will do a performance test of the host system's graphics
  card at startup to determine whether the 3D texture support, if
  present, is hardware accelerated or not. If the support is not
  hardware accelerated (which is the case with many older cards), SIM
  Voleon will fall back to regular 2D texture rendering.

  Beware that certain nodes requires 3D texture support in the OpenGL
  driver to function properly (e.g. SoObliqueSlice), but does not
  demand hardware-acceleration of 3D-texturing to be present. Required
  3D-texture support was included in the OpenGL specification starting
  with version 1.2, and it had additionally been present in the form
  of vendor and ARB extensions for some time before that. At the
  present day, the chances of running on a system without 3D texture
  support should therefore be small.
  </li>

  <li>
  Due to the fact that volume data sets are usually quite large,
  memory is an important factor for volume rendering. It is also
  important to remember that 2D texture rendering requires three times
  more memory than 3D texture rendering. For large datasets, available
  graphics card memory is currently the main bottleneck for SIM
  Voleon.
  </li>

  <li>
  SIM Voleon will take advantage of the \c EXT_paletted_texture or the
  \c ARB_fragment_program OpenGL extension if available for paletted
  rendering. If neither of these extensions are present, all data sets
  will be converted into RGBA textures before uploaded by OpenGL,
  multiplying the memory usage by four.
  </li>

  </ul>

  <p>
  <img src="http://doc.coin3d.org/images/SIMVoleon/nodes/vol-engine.png" align=right>

  SIM Voleon does currently support loading of VOL files, which is a
  format introduces by the book <i>"Introduction To Volume
  Rendering"</i>, by Lichtenbelt, Crane and Naqvi (Hewlett-Packard /
  Prentice Hall), <i>ISBN 0-13-861683-3</i>. (See the
  SoVRVolFileReader class doc for info). Support for more file-formats
  can be added by extending the SoVolumeReader class.

  Beware that large voxel sets are divided into sub cubes. The largest
  default sub cube size is by default set to 128x128x128, to match the
  TGS VolumeViz API. Current graphics cards can do much larger
  textures than this, achieving a higher framerate due to the reduced
  overhead of sub cube switching and slicing. A graphics card with
  128+ MB and true hardware 3D texture support can easily handle voxel
  sets of size 256x256x256. Increasing the maximum sub cube size can
  really boost the performance if your graphics card can handle
  it. Call SoVolumeData::setPageSize(const SbVec3s & size) to adjust
  the maximum sub cube size. Keep in mind that allowing really large
  3D textures might cause other textures to be swapped out of graphics
  memory, leading to reduced performance.
  </p>

  \sa The documentation for the \COIN library: <http://doc.coin3d.org/Coin>.
  \sa The documentation for the SoQt library: <http://doc.coin3d.org/SoQt>.
*/

// *************************************************************************

/*!
  \class SoVolumeRendering VolumeViz/nodes/SoVolumeRendering.h
  \brief Abstract base class for all nodes related to volume rendering.

  The sole purpose of this class is really just to initialize the
  volume rendering framework, and to provide a convenient method for
  the application programmer to make queries about the capabilities of
  the underlying visualization library.

  This class should never have been a node class, as it has no
  distinguishing features in that regard. We duplicate this design
  flaw for the sake of being compatible with code written for the TGS
  VolumeViz extension library.
*/

// *************************************************************************

#include <VolumeViz/nodes/SoVolumeRendering.h>

#include <Inventor/actions/SoGLRenderAction.h>

#include <VolumeViz/details/SoObliqueSliceDetail.h>
#include <VolumeViz/details/SoOrthoSliceDetail.h>
#include <VolumeViz/details/SoVolumeRenderDetail.h>
#include <VolumeViz/details/SoVolumeSkinDetail.h>
#include <VolumeViz/details/SoVolumeDetail.h>
#include <VolumeViz/elements/CvrCompressedTexturesElement.h>
#include <VolumeViz/elements/CvrGLInterpolationElement.h>
#include <VolumeViz/elements/CvrPageSizeElement.h>
#include <VolumeViz/elements/CvrPalettedTexturesElement.h>
#include <VolumeViz/elements/CvrStorageHintElement.h>
#include <VolumeViz/elements/CvrVoxelBlockElement.h>
#include <VolumeViz/elements/SoTransferFunctionElement.h>
#include <VolumeViz/nodes/SoObliqueSlice.h>
#include <VolumeViz/nodes/SoOrthoSlice.h>
#include <VolumeViz/nodes/SoTransferFunction.h>
#include <VolumeViz/nodes/SoVolumeData.h>
#include <VolumeViz/nodes/SoVolumeFaceSet.h>
#include <VolumeViz/nodes/SoVolumeIndexedFaceSet.h>
#include <VolumeViz/nodes/SoVolumeIndexedTriangleStripSet.h>
#include <VolumeViz/nodes/SoVolumeRender.h>
#include <VolumeViz/nodes/SoVolumeSkin.h>
#include <VolumeViz/nodes/SoVolumeTriangleStripSet.h>
#include <VolumeViz/render/common/CvrTextureObject.h>

// *************************************************************************

SO_NODE_SOURCE(SoVolumeRendering);

// *************************************************************************

class SoVolumeRenderingP {
public:
  static SbBool wasinitialized;
};

SbBool SoVolumeRenderingP::wasinitialized = FALSE;

#define PRIVATE(p) (p->pimpl)
#define PUBLIC(p) (p->master)

// *************************************************************************

SoVolumeRendering::SoVolumeRendering(void)
{
  SO_NODE_CONSTRUCTOR(SoVolumeRendering);

  PRIVATE(this) = NULL; // pimpl-class not yet needed
}

SoVolumeRendering::~SoVolumeRendering()
{
  delete PRIVATE(this);
}

/*!
  Does all necessary class initializations of the volume rendering
  system.

  Application programmers must call this method explicitly at the
  start of the application, before any volume rendering nodes are
  made. It must be invoked \e after SoXt::init() / SoQt::init() /
  SoWin::init(), though.
 */
void
SoVolumeRendering::init(void)
{
  if (SoVolumeRenderingP::wasinitialized) return;
  SoVolumeRenderingP::wasinitialized = TRUE;

  SoTransferFunctionElement::initClass();
  CvrCompressedTexturesElement::initClass();
  CvrGLInterpolationElement::initClass();
  CvrPageSizeElement::initClass();
  CvrPalettedTexturesElement::initClass();
  CvrStorageHintElement::initClass();
  CvrVoxelBlockElement::initClass();

  SoVolumeRendering::initClass();
  SoVolumeData::initClass();
  SoTransferFunction::initClass();

  SoVolumeRender::initClass();
  SoOrthoSlice::initClass();
  SoObliqueSlice::initClass();
  SoVolumeSkin::initClass();

  SoVolumeFaceSet::initClass();
  SoVolumeIndexedFaceSet::initClass();
  SoVolumeTriangleStripSet::initClass();
  SoVolumeIndexedTriangleStripSet::initClass();

  SoVolumeDetail::initClass();
  SoVolumeRenderDetail::initClass();
  SoVolumeSkinDetail::initClass();
  SoOrthoSliceDetail::initClass();
  SoObliqueSliceDetail::initClass();

  // Internal classes follows:

  CvrTextureObject::initClass();
}

/*!
  Sets up initialization for data common to all instances of this
  class, like submitting necessary information to the type system.
*/
void
SoVolumeRendering::initClass(void)
{
  SO_NODE_INIT_CLASS(SoVolumeRendering, SoNode, "SoNode");
}

SoVolumeRendering::HW_SupportStatus
SoVolumeRendering::isSupported(HW_Feature feature)
{
  switch (feature) {

  case SoVolumeRendering::HW_VOLUMEPRO:
    return SoVolumeRendering::NO;

  case SoVolumeRendering::HW_3DTEXMAP:
    // FIXME: update this when support is in place. 20021106 mortene.
    return SoVolumeRendering::NO;

  case SoVolumeRendering::HW_TEXCOLORMAP:
    // FIXME: update this when support is in place. 20021106 mortene.
    return SoVolumeRendering::NO;

  case SoVolumeRendering::HW_TEXCOMPRESSION:
    // FIXME: update this when support is in place. 20021106 mortene.
    return SoVolumeRendering::NO;

  default:
    assert(FALSE && "unknown feature");
    break;
  }

  return SoVolumeRendering::NO;
}
