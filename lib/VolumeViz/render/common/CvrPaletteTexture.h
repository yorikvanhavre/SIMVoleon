#ifndef COIN_CVRPALETTETEXTURE_H
#define COIN_CVRPALETTETEXTURE_H

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

class CvrCLUT;


class CvrPaletteTexture : public CvrTextureObject {
public:
  static void initClass(void);

  CvrPaletteTexture();
  virtual ~CvrPaletteTexture();

  virtual SoType getTypeId(void) const;
  static SoType getClassTypeId(void);

  virtual uint8_t * getIndex8Buffer(void) const;

  void setCLUT(const CvrCLUT * table);
  const CvrCLUT * getCLUT(void) const;

private:
  friend class Cvr2DPaletteTexture;
  friend class Cvr3DPaletteTexture;

  uint8_t * indexbuffer;
  const CvrCLUT * clut;
  static SoType classTypeId;

};

#endif // !COIN_CVRPALETTETEXTURE_H