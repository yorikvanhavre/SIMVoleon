/*!
  \class SoTransferFunction VolumeViz/nodes/SoTransferFunction.h
  \brief Contains the transfer function definition.
  \ingroup volviz
*/

#include <VolumeViz/nodes/SoTransferFunction.h>

#include <string.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/system/gl.h>
#include <VolumeViz/elements/SoTransferFunctionElement.h>
#include <VolumeViz/misc/CvrGIMPGradient.h>

// *************************************************************************

#include <gradients/GREY.h>
#include <gradients/TEMPERATURE.h>
#include <gradients/PHYSICS.h>
#include <gradients/STANDARD.h>
#include <gradients/GLOW.h>
#include <gradients/BLUE_RED.h>
#include <gradients/SEISMIC.h>

// *************************************************************************

SO_NODE_SOURCE(SoTransferFunction);

// *************************************************************************

class SoTransferFunctionP {
public:
  SoTransferFunctionP(SoTransferFunction * master) {
    this->master = master;

    // Init to lowest and highest uint16_t values.
    this->opaquethresholds[0] = 0;
    this->opaquethresholds[1] = (2 << 16) - 1;
  }

  int unpack(const void * data, int numBits, int index);
  void pack(void * data, int numBits, int index, int val);

  static uint8_t PREDEFGRADIENTS[SoTransferFunction::SEISMIC + 1][256][4];
  static void initPredefGradients(void);

  int opaquethresholds[2];

private:
  SoTransferFunction * master;
};

uint8_t SoTransferFunctionP::PREDEFGRADIENTS[SoTransferFunction::SEISMIC + 1][256][4];

#define PRIVATE(p) (p->pimpl)
#define PUBLIC(p) (p->master)

// *************************************************************************

SoTransferFunction::SoTransferFunction(void)
{
  SO_NODE_CONSTRUCTOR(SoTransferFunction);

  PRIVATE(this) = new SoTransferFunctionP(this);

  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, NONE);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, GREY);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, GRAY);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, TEMPERATURE);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, PHYSICS);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, STANDARD);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, GLOW);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, BLUE_RED);
  SO_NODE_DEFINE_ENUM_VALUE(PredefColorMap, SEISMIC);
  SO_NODE_SET_SF_ENUM_TYPE(predefColorMap, PredefColorMap);

  SO_NODE_DEFINE_ENUM_VALUE(ColorMapType, ALPHA);
  SO_NODE_DEFINE_ENUM_VALUE(ColorMapType, LUM_ALPHA);
  SO_NODE_DEFINE_ENUM_VALUE(ColorMapType, RGBA);
  SO_NODE_SET_SF_ENUM_TYPE(colorMapType, ColorMapType);

  SO_NODE_ADD_FIELD(shift, (0));
  SO_NODE_ADD_FIELD(offset, (0));
  SO_NODE_ADD_FIELD(predefColorMap, (GREY));
  SO_NODE_ADD_FIELD(colorMapType, (RGBA));
  SO_NODE_ADD_FIELD(colorMap, (0));
}


SoTransferFunction::~SoTransferFunction()
{
  delete PRIVATE(this);
}


void
SoTransferFunction::initClass(void)
{
  SO_NODE_INIT_CLASS(SoTransferFunction, SoVolumeRendering, "SoVolumeRendering");

  SO_ENABLE(SoGLRenderAction, SoTransferFunctionElement);
  SO_ENABLE(SoCallbackAction, SoTransferFunctionElement);
  SO_ENABLE(SoPickAction, SoTransferFunctionElement);

  SoTransferFunctionP::initPredefGradients();
}

void
SoTransferFunction::doAction(SoAction * action)
{
  SoTransferFunctionElement::setTransferFunction(action->getState(), this, this);
}

void
SoTransferFunction::GLRender(SoGLRenderAction * action)
{
  this->doAction(action);
}

void
SoTransferFunction::callback(SoCallbackAction * action)
{
  this->doAction(action);
}

void
SoTransferFunction::pick(SoPickAction * action)
{
  this->doAction(action);
}


// FIXME: expose tidbits.h properly from Coin. 20021109 mortene.
extern "C" {
enum CoinEndiannessValues { COIN_HOST_IS_UNKNOWNENDIAN = -1, COIN_HOST_IS_LITTLEENDIAN = 0, COIN_HOST_IS_BIGENDIAN = 1 };
extern int coin_host_get_endianness(void);
}

static inline uint32_t
compileRGBALittleEndian(const uint8_t red, const uint8_t green,
                        const uint8_t blue, const uint8_t opaqueness)
{
  return
    (uint32_t(red) << 0) | (uint32_t(green) << 8) |
    (uint32_t(blue) << 16) | (uint32_t(opaqueness) << 24);
}

static inline uint32_t
compileRGBABigEndian(const uint8_t red, const uint8_t green,
                     const uint8_t blue, const uint8_t opaqueness)
{
  return
    (uint32_t(red) << 24) | (uint32_t(green) << 16) |
    (uint32_t(blue) << 8) | (uint32_t(opaqueness) << 0);
}

// Transfers voxel data from input buffer to output buffer, according
// to specified parameters.
//
// Output buffer is allocated within the function.
void
SoTransferFunction::transfer(const void * input, 
                             SoVolumeData::DataType inputdatatype,
                             const SbVec2s & size,
                             void *& output,
                             int & outputFormat,
                             float *& palette,
                             int & paletteFormat,
                             int & palettesize)
{
  static int endianness = COIN_HOST_IS_UNKNOWNENDIAN;
  if (endianness == COIN_HOST_IS_UNKNOWNENDIAN) {
    endianness = coin_host_get_endianness();
    assert(endianness != COIN_HOST_IS_UNKNOWNENDIAN && "weird hardware!");
  }

  // FIXME: we're using a GLEnum here, but SoVolumeData enum
  // below. 20021112 mortene.
  outputFormat = GL_RGBA;
  // We only support non-paletted textures so far.
  output = new uint32_t[size[0]*size[1]];
  palette = NULL;
  paletteFormat = 0;
  palettesize = 0;

  // Handling RGBA inputdata. Just forwarding to output
  if (inputdatatype == SoVolumeData::RGBA) {
    (void)memcpy(output, input, size[0]*size[1]*sizeof(int));
    return;
  }

  if (inputdatatype == SoVolumeData::UNSIGNED_BYTE) {
    // FIXME: does not heed all possible SoTransferFunction
    // settings. 20021118 mortene.

    uint32_t * outp = (uint32_t *)output;
    const uint8_t * inp = (const uint8_t *)input;

    int colormaptype = this->colorMapType.getValue();
    int nrcomponents = 1;
    switch (colormaptype) {
    case ALPHA: nrcomponents = 1; break;
    case LUM_ALPHA: nrcomponents = 2; break;
    case RGBA: nrcomponents = 4; break;
    default: assert(FALSE && "invalid SoTransferFunction::colorMapType value"); break;
    }

    const float * colormap = this->colorMap.getValues(0);
    int nrcols = this->colorMap.getNum() / nrcomponents;
    // FIXME: a bit strict this, should warn instead. 20021119 mortene.
    assert((this->colorMap.getNum() % nrcomponents) == 0);

    uint8_t * predefmap = NULL;
    const int predefmapidx = this->predefColorMap.getValue();
    assert(predefmapidx >= NONE);
    assert(predefmapidx <= SEISMIC);

    if (predefmapidx != NONE) {
      predefmap = &(SoTransferFunctionP::PREDEFGRADIENTS[predefmapidx][0][0]);
    }

    int32_t shiftval = this->shift.getValue();
    int32_t offsetval = this->offset.getValue();

    for (int j=0; j < size[0]*size[1]; j++) {
      uint8_t inval = inp[j];

      if (shiftval != 0) inval <<= shiftval;
      if (offsetval != 0) inval += offsetval;

      if ((inval == 0x00) || // FIXME: not sure about this.. 20021119 mortene.
          (inval < PRIVATE(this)->opaquethresholds[0]) ||
          (inval > PRIVATE(this)->opaquethresholds[1])) {
        outp[j] = 0x00000000;
      }
      else {
        uint8_t rgba[4];

        if (predefmapidx == NONE) {
          assert(inval < nrcols);
          const float * colvals = &(colormap[inval * nrcomponents]);
#if 1 // ROBUSTNESS
          for (int cvchk = 0; cvchk < nrcomponents; cvchk++) {
            assert(colvals[cvchk] >= 0.0f && colvals[cvchk] <= 1.0f);
          }
#endif // robustness
          switch (colormaptype) {
          case ALPHA:
            rgba[0] = rgba[1] = rgba[2] = rgba[3] = uint8_t(colvals[0] * 255.0f);
            break;

          case LUM_ALPHA:
            rgba[0] = rgba[1] = rgba[2] = uint8_t(colvals[0] * 255.0f);
            rgba[3] = uint8_t(colvals[1] * 255.0f);
            break;

          case RGBA:
            rgba[0] = uint8_t(colvals[0] * 255.0f);
            rgba[1] = uint8_t(colvals[1] * 255.0f);
            rgba[2] = uint8_t(colvals[2] * 255.0f);
            rgba[3] = uint8_t(colvals[3] * 255.0f);
            break;

          default:
            assert(FALSE && "impossible");
            break;
          }
        }
        else { // using one of the predefined colormaps
          rgba[0] = predefmap[inval * 4 + 0];
          rgba[1] = predefmap[inval * 4 + 1];
          rgba[2] = predefmap[inval * 4 + 2];
          rgba[3] = predefmap[inval * 4 + 3];
        }

        outp[j] = (endianness == COIN_HOST_IS_LITTLEENDIAN) ?
          compileRGBALittleEndian(rgba[0], rgba[1], rgba[2], rgba[3]) :
          compileRGBABigEndian(rgba[0], rgba[1], rgba[2], rgba[3]);
      }
    }

    return;
  }

  if (inputdatatype == SoVolumeData::UNSIGNED_SHORT) {
    // FIXME: this is of course completely wrong -- the data should be
    // handled according to the SoTransferFunction settings. 20021112 mortene.

    uint32_t * outp = (uint32_t *)output;
    const uint16_t * inp = (const uint16_t *)input;

    if (endianness == COIN_HOST_IS_LITTLEENDIAN) {
      for (int j=0; j < size[0]*size[1]; j++) {
        outp[j] =
          (0 << 0) | // red
          (uint32_t(inp[j] & 0x00ff) << 8) | // green
          (uint32_t((inp[j] >> 8) & 0x00ff) << 16) | // blue
          ((inp[j] ? 0xff : 0) << 24); // alpha
      }
    }
    else {
      for (int j=0; j < size[0]*size[1]; j++) {
        outp[j] =
          (0 << 24) | // red
          (uint32_t(inp[j] & 0x00ff) << 16) | // green
          (uint32_t((inp[j] >> 8) & 0x00ff) << 8) | // blue
          ((inp[j] ? 0xff : 0) << 0); // alpha
      }
    }
    return;
  }

  assert(FALSE && "unknown input format");

  // FIXME: tmp disabled all use of paletted (and compressed?)
  // textures. (The code wasn't working at all, as far as I could
  // see.) 20021112 mortene.
#if 0
  int numbits;
  switch (inputdatatype) {
  case SoVolumeData::UNSIGNED_BYTE: numbits = 8; break;
        
  case SoVolumeData::UNSIGNED_SHORT: numbits = 16; break;

  default: assert(FALSE && "unknown input data type"); break;
  }

  int maxpalentries = 1 << numbits;

  // Counting number of references to each palette entry     
  SbBool * palcountarray = new SbBool[maxpalentries];
  (void)memset(palcountarray, 0, sizeof(SbBool) * maxpalentries);

  int nrpalentries = 0;

  int32_t shift = this->shift.getValue(); // for optimized access in loop below
  int32_t offset = this->offset.getValue();  // for optimized access in loop

  int i;
  for (i = 0; i < size[0]*size[1]; i++) {
    int unpacked = PRIVATE(this)->unpack(input, numbits, i);
    int idx = (unpacked << shift) + offset;

    if (idx >= maxpalentries) {
#if 1 // debug
      SoDebugError::postInfo("SoTransferFunction::transfer",
                             "idx %d out-of-bounds [0, %d] "
                             "(unpacked==%d, shift==%d, offset==%d)",
                             idx, maxpalentries - 1,
                             unpacked, shift, offset);
#endif // debug
    }

    assert(idx < maxpalentries);

    if (palcountarray[idx] == FALSE) {
      palcountarray[idx] = TRUE;
      nrpalentries++;
    }
  }

  // Creating remap-table and counting number of entries in new palette
  int * remap = new int[maxpalentries];
  int palidx = 0;
  for (i = 0; i < maxpalentries; i++) {
    if (palcountarray[i]) { remap[i] = palidx++; }
    else { remap[i] = -1; }
  }

  // Calculating the new palette's size to a power of two.
  palettesize = 2;
  while (palettesize < nrpalentries) { palettesize <<= 1; }

#if 0 // debug
  SoDebugError::postInfo("SoTransferFunction::transfer",
                         "nrpalentries==%d, palettesize==%d",
                         nrpalentries, palettesize);
#endif // debug

  // Building new palette
  // FIXME: convert this to a bytearray. 20021112 mortene.
  palette = new float[palettesize*4]; // "*4" is to cover 1 byte each for RGBA
  (void)memset(palette, 0, sizeof(float)*palettesize*4);
  const float * oldPal = this->colorMap.getValues(0);
#if 0 // debug
  SoDebugError::postInfo("SoTransferFunction::transfer",
                         "this->colorMap.getNum()==%d, maxpalentries==%d",
                         this->colorMap.getNum(), maxpalentries);
#endif // debug
  // FIXME: the way we use this->colorMap here leaves much to be
  // desired wrt both robustness and correctness. 20021112 mortene.
  int tmp = 0;
  for (i = 0; i < maxpalentries; i++) {
    if (palcountarray[i]) {
      // FIXME: out-of-bounds read on oldPal! 20021112 mortene.
      (void)memcpy(&palette[tmp*4], &oldPal[i*4], sizeof(float)*4);
      tmp++;
    }
  }

    // FIXME: we're using a SoVolumeData enum here, but GLEnum
    // above for outputFormat. 20021112 mortene.

  // Deciding outputformat
  int newNumBits = 8;
  outputFormat = SoVolumeData::UNSIGNED_BYTE;
  if (palettesize > 256) {
    newNumBits = 16;
    outputFormat = SoVolumeData::UNSIGNED_SHORT;
  }

  // Rebuilding texturedata
  unsigned char * newTexture = 
    new unsigned char[size[0] * size[1] * newNumBits / 8];
  memset(newTexture, 0, size[0] * size[1] * newNumBits / 8);

  for (i = 0; i < size[0]*size[1]; i++) {

    int unpacked = PRIVATE(this)->unpack(input, numbits, i);
    int idx = (unpacked << shift) + offset;

    if (idx >= maxpalentries) {
#if 1 // debug
      SoDebugError::postInfo("SoTransferFunction::transfer",
                             "idx %d out-of-bounds [0, %d] "
                             "(unpacked==%d, shift==%d, offset==%d)",
                             idx, maxpalentries - 1,
                             unpacked, shift, offset);
#endif // debug
    }

    assert(idx < maxpalentries);

    idx = remap[idx];
    PRIVATE(this)->pack(newTexture, newNumBits, i, idx);
  }
  output = newTexture;
  paletteFormat = GL_RGBA;

  delete[] palcountarray;
  delete[] remap;
#endif // TMP DISABLED
}

/*!
  Handles packed data for 1, 2, 4, 8 and 16 bits. Assumes 16-bit data
  are stored in little-endian format (that is the x86-way,
  right?). And MSB is the leftmost of the bitstream...? Think so.
*/
int 
SoTransferFunctionP::unpack(const void * data, int numbits, int index)
{
  if (numbits == 8)
    return (int)((char*)data)[index];

  if (numbits == 16)
    return (int)((short*)data)[index];


  // Handling 1, 2 and 4 bit formats
  int bitIndex = numbits*index;
  int byteIndex = bitIndex/8;
  int localBitIndex = 8 - bitIndex%8 - numbits;

  char val = ((char*)data)[byteIndex];
  val >>= localBitIndex;
  val &= (1 << numbits) - 1;

  return val;
}



/*!
  Saves the index specified in val with the specified number of bits,
  at the specified index (not elementindex, not byteindex) in data.
*/
void 
SoTransferFunctionP::pack(void * data, int numbits, int index, int val)
{
  if (val >= (1 << numbits))
    val = (1 << numbits) - 1;

  if (numbits == 8) {
    ((char*)data)[index] = (char)val;
  }
  else 
  if (numbits == 16) {
    ((short*)data)[index] = (short)val;
  }
  else {
    int bitIndex = numbits*index;
    int byteIndex = bitIndex/8;
    int localBitIndex = 8 - bitIndex%8 - numbits;

    char byte = ((char*)data)[byteIndex];
    char mask = (((1 << numbits) - 1) << localBitIndex) ^ -1;
    byte &= mask;
    val <<= localBitIndex;
    byte |= val;
    ((char*)data)[byteIndex] = byte;
  }
}


void
SoTransferFunction::reMap(int low, int high)
{
  assert(low <= high);

  PRIVATE(this)->opaquethresholds[0] = low;
  PRIVATE(this)->opaquethresholds[1] = high;
  
  // This is done to update our node-id, which should automatically
  // invalidate any 2D texture slices or 3D textures generated with
  // the previous colormap transfer.
  this->touch();
}

// Initialize all 256-value predefined colormaps.
void
SoTransferFunctionP::initPredefGradients(void)
{
  const char * gradientbufs[] = {
    GREY_gradient, TEMPERATURE_gradient, PHYSICS_gradient,
    STANDARD_gradient, GLOW_gradient, BLUE_RED_gradient, SEISMIC_gradient
  };

  for (unsigned int j=0; j < sizeof(gradientbufs)/sizeof(gradientbufs[0]); j++) {
    CvrGIMPGradient * gg = CvrGIMPGradient::read(gradientbufs[j]);
    gg->convertToIntArray(SoTransferFunctionP::PREDEFGRADIENTS[j+1]);

#if 0 // DEBUG: spits out a 256x1 image of the colormap.
    SbString s;
    s.sprintf("/tmp/gradient-%d.ppm", j + 1);
    FILE * f = fopen(s.getString(), "w");
    assert(f);
    (void)fprintf(f, "P3\n256 1 255\n"); // width height maxcolval

    for (int i=0; i < 256; i++) {
      fprintf(f, "%d %d %d\n",
              SoTransferFunctionP::PREDEFGRADIENTS[TEMPERATURE][i][0],
              SoTransferFunctionP::PREDEFGRADIENTS[TEMPERATURE][i][1],
              SoTransferFunctionP::PREDEFGRADIENTS[TEMPERATURE][i][2]);
    }
    fclose(f);
#endif // DEBUG
  }
}
