#ifndef COIN_CVR3DTEXPAGE_H
#define COIN_CVR3DTEXPAGE_H

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

#include <Inventor/SbVec3s.h>
#include <Inventor/misc/SoState.h>
#include <VolumeViz/readers/SoVolumeReader.h>
#include <VolumeViz/render/3D/Cvr3DTexSubCube.h>


class Cvr3DTexCube {
  
public:
  Cvr3DTexCube(SoVolumeReader * reader);
  ~Cvr3DTexCube();
  
  void render(SoGLRenderAction * action, const SbVec3f & origo,
              const SbVec3f & cubespan,
              Cvr3DTexSubCube::Interpolation interpolation,
              unsigned int numslices);
  
  void setPalette(const CvrCLUT * c);
  const CvrCLUT * getPalette(void) const;
  
private:
  class Cvr3DTexSubCubeItem * getSubCube(SoState * state, int col, int row, int depth);

  class Cvr3DTexSubCubeItem * buildSubCube(SoGLRenderAction * action,
                                           int col, int row, int depth,
                                           SbVec3f cubescale);
   
  void releaseAllSubCubes(void);
  void releaseSubCube(const int row, const int col, const int depth);

  int calcSubCubeIdx(int row, int col, int depth) const;
  void calculateOptimalSubCubeSize();

  class Cvr3DTexSubCubeItem ** subcubes;
  SoVolumeReader * reader;

  SbVec3s subcubesize;
  SbVec3s dimensions;

  SbBool rendersubcubeoutline;
  int nrcolumns;
  int nrrows;
  int nrdepths;

  const CvrCLUT * clut;
};

#endif // !COIN_CVR3DTEXPAGE_H