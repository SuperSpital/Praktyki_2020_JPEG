#include <iostream>
#include <cmath>
#include <fstream>

#include "stb_image.c"

using namespace std;

#define PJPG_INLINE
#define PJPG_WINOGRAD_QUANT_SCALE_BITS 10
#define PJPG_DCT_SCALE_BITS 7
#define PJPG_MAX_WIDTH 16384
#define PJPG_MAX_HEIGHT 16384
#define PJPG_MAXCOMPSINSCAN 3

#if PJPG_RIGHT_SHIFT_IS_ALWAYS_UNSIGNED
static int16 replicateSignBit16(int8 n)
{
   switch (n)
   {
      case 0:  return 0x0000;
      case 1:  return 0x8000;
      case 2:  return 0xC000;
      case 3:  return 0xE000;
      case 4:  return 0xF000;
      case 5:  return 0xF800;
      case 6:  return 0xFC00;
      case 7:  return 0xFE00;
      case 8:  return 0xFF00;
      case 9:  return 0xFF80;
      case 10: return 0xFFC0;
      case 11: return 0xFFE0;
      case 12: return 0xFFF0;
      case 13: return 0xFFF8;
      case 14: return 0xFFFC;
      case 15: return 0xFFFE;
      default: return 0xFFFF;
   }
}
static PJPG_INLINE int16 arithmeticRightShiftN16(int16 x, int8 n)
{
   int16 r = (uint16)x >> (uint8)n;
   if (x < 0)
      r |= replicateSignBit16(n);
   return r;
}
static PJPG_INLINE long arithmeticRightShift8L(long x)
{
   long r = (unsigned long)x >> 8U;
   if (x < 0)
      r |= ~(~(unsigned long)0U >> 8U);
   return r;
}
#define PJPG_ARITH_SHIFT_RIGHT_N_16(x, n) arithmeticRightShiftN16(x, n)
#define PJPG_ARITH_SHIFT_RIGHT_8_L(x) arithmeticRightShift8L(x)
#else
#define PJPG_ARITH_SHIFT_RIGHT_N_16(x, n) ((x) >> (n))
#define PJPG_ARITH_SHIFT_RIGHT_8_L(x) ((x) >> 8)
#endif

#define PJPG_DESCALE(x) PJPG_ARITH_SHIFT_RIGHT_N_16(((x) + (1 << (PJPG_DCT_SCALE_BITS - 1))), PJPG_DCT_SCALE_BITS)

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

typedef enum
{
    M_SOF0  = 0xC0,
    M_SOF1  = 0xC1,
    M_SOF2  = 0xC2,
    M_SOF3  = 0xC3,

    M_SOF5  = 0xC5,
    M_SOF6  = 0xC6,
    M_SOF7  = 0xC7,

    M_JPG   = 0xC8,
    M_SOF9  = 0xC9,
    M_SOF10 = 0xCA,
    M_SOF11 = 0xCB,

    M_SOF13 = 0xCD,
    M_SOF14 = 0xCE,
    M_SOF15 = 0xCF,

    M_DHT   = 0xC4,

    M_DAC   = 0xCC,

    M_RST0  = 0xD0,
    M_RST1  = 0xD1,
    M_RST2  = 0xD2,
    M_RST3  = 0xD3,
    M_RST4  = 0xD4,
    M_RST5  = 0xD5,
    M_RST6  = 0xD6,
    M_RST7  = 0xD7,

    M_SOI   = 0xD8,
    M_EOI   = 0xD9,
    M_SOS   = 0xDA,
    M_DQT   = 0xDB,
    M_DNL   = 0xDC,
    M_DRI   = 0xDD,
    M_DHP   = 0xDE,
    M_EXP   = 0xDF,

    M_APP0  = 0xE0,
    M_APP15 = 0xEF,

    M_JPG0  = 0xF0,
    M_JPG13 = 0xFD,
    M_COM   = 0xFE,

    M_TEM   = 0x01,

    M_ERROR = 0x100,

    RST0    = 0xD0
} JPEG_MARKER;

uint8_t gWinogradQuant[] =
        {
                128,  178,  178,  167,  246,  167,  151,  232,
                232,  151,  128,  209,  219,  209,  128,  101,
                178,  197,  197,  178,  101,   69,  139,  167,
                177,  167,  139,   69,   35,   96,  131,  151,
                151,  131,   96,   35,   49,   91,  118,  128,
                118,   91,   49,   46,   81,  101,  101,   81,
                46,   42,   69,   79,   69,   42,   35,   54,
                54,   35,   28,   37,   28,   19,   19,   10,
        };

static const int8_t ZAG[] =
        {
                0,  1,  8, 16,  9,  2,  3, 10,
                17, 24, 32, 25, 18, 11,  4,  5,
                12, 19, 26, 33, 40, 48, 41, 34,
                27, 20, 13,  6,  7, 14, 21, 28,
                35, 42, 49, 56, 57, 50, 43, 36,
                29, 22, 15, 23, 30, 37, 44, 51,
                58, 59, 52, 45, 38, 31, 39, 46,
                53, 60, 61, 54, 47, 55, 62, 63,
        };

uint16_t getExtendTest(uint8_t i)
{
    switch (i)
    {
        case 0: return 0;
        case 1: return 0x0001;
        case 2: return 0x0002;
        case 3: return 0x0004;
        case 4: return 0x0008;
        case 5: return 0x0010;
        case 6: return 0x0020;
        case 7: return 0x0040;
        case 8:  return 0x0080;
        case 9:  return 0x0100;
        case 10: return 0x0200;
        case 11: return 0x0400;
        case 12: return 0x0800;
        case 13: return 0x1000;
        case 14: return 0x2000;
        case 15: return 0x4000;
        default: return 0;
    }
}

int16_t getExtendOffset(uint8_t i)
{
    switch (i)
    {
        case 0: return 0;
        case 1: return ((-1)<<1) + 1;
        case 2: return ((-1)<<2) + 1;
        case 3: return ((-1)<<3) + 1;
        case 4: return ((-1)<<4) + 1;
        case 5: return ((-1)<<5) + 1;
        case 6: return ((-1)<<6) + 1;
        case 7: return ((-1)<<7) + 1;
        case 8: return ((-1)<<8) + 1;
        case 9: return ((-1)<<9) + 1;
        case 10: return ((-1)<<10) + 1;
        case 11: return ((-1)<<11) + 1;
        case 12: return ((-1)<<12) + 1;
        case 13: return ((-1)<<13) + 1;
        case 14: return ((-1)<<14) + 1;
        case 15: return ((-1)<<15) + 1;
        default: return 0;
    }
}

FILE *g_pInFile;
unsigned int g_nInFileSize;
unsigned int g_nInFileOfs;

enum
{
    PJPG_NO_MORE_BLOCKS = 1,
    PJPG_BAD_DHT_COUNTS,
    PJPG_BAD_DHT_INDEX,
    PJPG_BAD_DHT_MARKER,
    PJPG_BAD_DQT_MARKER,
    PJPG_BAD_DQT_TABLE,
    PJPG_BAD_PRECISION,
    PJPG_BAD_HEIGHT,
    PJPG_BAD_WIDTH,
    PJPG_TOO_MANY_COMPONENTS,
    PJPG_BAD_SOF_LENGTH,
    PJPG_BAD_VARIABLE_MARKER,
    PJPG_BAD_DRI_LENGTH,
    PJPG_BAD_SOS_LENGTH,
    PJPG_BAD_SOS_COMP_ID,
    PJPG_W_EXTRA_BYTES_BEFORE_MARKER,
    PJPG_NO_ARITHMITIC_SUPPORT,
    PJPG_UNEXPECTED_MARKER,
    PJPG_NOT_JPEG,
    PJPG_UNSUPPORTED_MARKER,
    PJPG_BAD_DQT_LENGTH,
    PJPG_TOO_MANY_BLOCKS,
    PJPG_UNDEFINED_QUANT_TABLE,
    PJPG_UNDEFINED_HUFF_TABLE,
    PJPG_NOT_SINGLE_SCAN,
    PJPG_UNSUPPORTED_COLORSPACE,
    PJPG_UNSUPPORTED_SAMP_FACTORS,
    PJPG_DECODE_ERROR,
    PJPG_BAD_RESTART_MARKER,
    PJPG_ASSERTION_ERROR,
    PJPG_BAD_SOS_SPECTRAL,
    PJPG_BAD_SOS_SUCCESSIVE,
    PJPG_STREAM_READ_ERROR,
    PJPG_NOTENOUGHMEM,
    PJPG_UNSUPPORTED_COMP_IDENT,
    PJPG_UNSUPPORTED_QUANT_TABLE,
    PJPG_UNSUPPORTED_MODE,        // picojpeg doesn't support progressive JPEG's
};

typedef enum
{
    PJPG_GRAYSCALE,
    PJPG_YH1V1,
    PJPG_YH2V1,
    PJPG_YH1V2,
    PJPG_YH2V2
} pjpeg_scan_type_t;

typedef struct HuffTableT
{
    uint16_t mMinCode[16];
    uint16_t mMaxCode[16];
    uint8_t mValPtr[16];
} HuffTable;

typedef struct
{
    // Image resolution
    int m_width;
    int m_height;

    // Number of components (1 or 3)
    int m_comps;

    // Total number of minimum coded units (MCU's) per row/col.
    int m_MCUSPerRow;
    int m_MCUSPerCol;

    // Scan type
    pjpeg_scan_type_t m_scanType;

    // MCU width/height in pixels (each is either 8 or 16 depending on the scan type)
    int m_MCUWidth;
    int m_MCUHeight;

    unsigned char *m_pMCUBufR;
    unsigned char *m_pMCUBufG;
    unsigned char *m_pMCUBufB;
} pjpeg_image_info_t;

unsigned char pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data)
{
    unsigned int n;
    pCallback_data;

    n = min(g_nInFileSize - g_nInFileOfs, buf_size);
    if (n && (fread(pBuf, 1, n, g_pInFile) != n))
        return PJPG_STREAM_READ_ERROR;
    *pBytes_actually_read = (unsigned char)(n);
    g_nInFileOfs += n;
    return 0;
}

typedef unsigned char (*pjpeg_need_bytes_callback_t)(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data);

pjpeg_need_bytes_callback_t g_pNeedBytesCallback;
void *g_pCallback_data;
uint8_t gCallbackStatus;
uint8_t gReduce;
uint16_t gImageXSize;
uint16_t gImageYSize;
uint8_t gCompsInFrame;
uint8_t gCompsInScan;
uint16_t gRestartInterval;
uint8_t gValidHuffTables;
uint8_t gValidQuantTables;
uint8_t gTemFlag;
#define PJPG_MAX_IN_BUF_SIZE 256
uint8_t gInBuf[PJPG_MAX_IN_BUF_SIZE];
uint8_t gInBufOfs;
uint8_t gInBufLeft;
uint16_t gBitBuf;
uint8_t gBitsLeft;
HuffTable gHuffTab0;
HuffTable gHuffTab1;
HuffTable gHuffTab2;
HuffTable gHuffTab3;
uint8_t gHuffVal0[16];
uint8_t gHuffVal1[16];
uint8_t gHuffVal2[256];
uint8_t gHuffVal3[256];
int16_t gQuant0[8*8];
int16_t gQuant1[8*8];
uint8_t gCompIdent[3];
uint8_t gCompHSamp[3];
uint8_t gCompVSamp[3];
uint8_t gCompQuant[3];
uint8_t gCompList[3];
uint8_t gCompDCTab[3]; // 0,1
uint8_t gCompACTab[3];
int16_t gLastDC[3];
uint16_t gNextRestartNum;
uint16_t gRestartsLeft;
pjpeg_scan_type_t gScanType;
uint8_t gMaxBlocksPerMCU;
uint8_t gMaxMCUXSize;
uint8_t gMaxMCUYSize;
uint16_t gMaxMCUSPerRow;
uint16_t gMaxMCUSPerCol;
uint16_t gNumMCUSRemainingX, gNumMCUSRemainingY;
uint8_t gMCUOrg[6];
uint8_t gMCUBufR[256];
uint8_t gMCUBufG[256];
uint8_t gMCUBufB[256];

int number_of_coeffs = 3133440;
int16_t coeffs[8][3133440];
int counter = 0;
int16_t gCoeffBuf_scaled[64];
int16_t gCoeffBuf[64];
int W[] = {1, 1, 1, 2, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 5, 4, 4, 3, 3, 3, 2, 2, 2, 2,
           1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 7, 6, 6, 5,
           5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
           1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 8,
           8, 8, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2,
           2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
           3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8,
           8, 8, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3,
           3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
           3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8,
           8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5,
           5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4,
           4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8,
           8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4,
           4, 4, 3, 3, 3, 2, 2, 1, 2, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8,
           8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 4, 4, 3, 4, 5, 5,
           6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 6, 6, 5, 6, 7, 7, 8, 8, 8, 8, 8, 7, 8};

int X[] = {1, 1, 2, 1, 1, 2, 1, 3, 2, 1, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1, 2, 1, 3, 2, 1, 4, 3, 2, 1,
           5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1, 1, 2, 1, 3,
           2, 1, 4, 3, 2, 1, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6,
           7, 8, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 1, 2, 1,
           2, 1, 3, 2, 1, 4, 3, 2, 1, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6,
           5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5,
           6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 1, 2, 3, 4, 1, 2, 3, 4, 3,
           2, 1, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7,
           6, 5, 4, 3, 2, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 3, 4, 5, 6, 7,
           8, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 1,
           2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4,
           3, 2, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 8, 7, 6, 5, 8, 7, 6, 7, 8, 6, 7, 8, 5, 6, 7, 8, 4,
           5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5,
           6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 8, 7,
           6, 5, 8, 7, 6, 8, 7, 8, 8, 7, 8, 6, 7, 8, 5, 6, 7, 8, 4, 5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 2,
           3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 8, 7, 6, 5, 8, 7, 6, 8, 7, 8, 8, 7, 8,
           6, 7, 8, 5, 6, 7, 8, 4, 5, 6, 7, 8, 8, 7, 6, 5, 8, 7, 6, 8, 7, 8, 8, 7, 8, 6, 7, 8, 8, 7, 8, 8};

int Z[] = {1, 2, 1, 1, 1, 1, 2, 1, 2, 3, 4, 3, 2, 1, 3, 2, 1, 2, 1, 1, 1, 1, 2, 1, 2, 3, 1, 2, 3, 4,
           1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 4, 3, 2, 1, 3, 2, 1, 2, 1, 1, 1, 1, 2, 1,
           2, 3, 1, 2, 3, 4, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3,
           2, 1, 7, 6, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 4, 3, 2, 1, 3, 2, 1, 2, 1, 1,
           1, 2, 1, 2, 3, 1, 2, 3, 4, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3,
           4, 5, 6, 7, 8, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 3, 2, 8, 7, 6, 5, 4,
           3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 5, 4, 3, 2, 1, 4, 3, 2, 1, 3, 2, 1, 1, 2,
           3, 4, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7, 8, 2, 3,
           4, 5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 8, 7, 6, 5, 8, 7, 6, 5, 4, 8, 7, 6, 5, 4,
           3, 8, 7, 6, 5, 4, 3, 2, 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 6, 5, 4, 3, 2, 1, 5,
           4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6,
           7, 8, 3, 4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 5, 6, 7, 8, 6, 7, 8, 8, 7, 8, 7, 6, 8, 7, 6, 5, 8,
           7, 6, 5, 4, 8, 7, 6, 5, 4, 3, 8, 7, 6, 5, 4, 3, 2, 8, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3,
           2, 1, 1, 2, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 5, 6,
           7, 8, 6, 7, 8, 7, 8, 8, 8, 8, 7, 8, 7, 6, 8, 7, 6, 5, 8, 7, 6, 5, 4, 8, 7, 6, 5, 4, 3, 8,
           7, 6, 5, 4, 3, 2, 3, 4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 5, 6, 7, 8, 6, 7, 8, 7, 8, 8, 8, 8, 7,
           8, 7, 6, 8, 7, 6, 5, 8, 7, 6, 5, 4, 5, 6, 7, 8, 6, 7, 8, 7, 8, 8, 8, 8, 7, 8, 7, 6, 7, 8, 8, 8};

int16_t zigzagcoeffs[48960][512];

const uint8_t ZigZagInv[8*8] =
        {  0, 1, 8,16, 9, 2, 3,10,   // ZigZag[] =  0, 1, 5, 6,14,15,27,28,
           17,24,32,25,18,11, 4, 5,   //             2, 4, 7,13,16,26,29,42,
           12,19,26,33,40,48,41,34,   //             3, 8,12,17,25,30,41,43,
           27,20,13, 6, 7,14,21,28,   //             9,11,18,24,31,40,44,53,
           35,42,49,56,57,50,43,36,   //            10,19,23,32,39,45,52,54,
           29,22,15,23,30,37,44,51,   //            20,22,33,38,46,51,55,60,
           58,59,52,45,38,31,39,46,   //            21,34,37,47,50,56,59,61,
           53,60,61,54,47,55,62,63 }; //            35,36,48,49,57,58,62,63

const uint8_t DefaultQuantLuminance[8*8] =
        { 16, 11, 10, 16, 24, 40, 51, 61, // there are a few experts proposing slightly more efficient values,
          12, 12, 14, 19, 26, 58, 60, 55, // e.g. https://www.imagemagick.org/discourse-server/viewtopic.php?t=20333
          14, 13, 16, 24, 40, 57, 69, 56, // btw: Google's Guetzli project optimizes the quantization tables per image
          14, 17, 22, 29, 51, 87, 80, 62,
          18, 22, 37, 56, 68,109,103, 77,
          24, 35, 55, 64, 81,104,113, 92,
          49, 64, 78, 87,103,121,120,101,
          72, 92, 95, 98,112,100,103, 99 };

const uint8_t DefaultQuantChrominance[8*8] =
        { 17, 18, 24, 47, 99, 99, 99, 99,
          18, 21, 26, 66, 99, 99, 99, 99,
          24, 26, 56, 99, 99, 99, 99, 99,
          47, 66, 99, 99, 99, 99, 99, 99,
          99, 99, 99, 99, 99, 99, 99, 99,
          99, 99, 99, 99, 99, 99, 99, 99,
          99, 99, 99, 99, 99, 99, 99, 99,
          99, 99, 99, 99, 99, 99, 99, 99 };

const uint8_t DcLuminanceCodesPerBitsize[16]   = { 0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };   // sum = 12
const uint8_t DcLuminanceValues         [12]   = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes
const uint8_t AcLuminanceCodesPerBitsize[16]   = { 0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125 }; // sum = 162
const uint8_t AcLuminanceValues        [162]   =                                        // => 162 codes
        { 0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08, // 16*10+2 symbols because
          0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28, // upper 4 bits can be 0..F
          0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59, // while lower 4 bits can be 1..A
          0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89, // plus two special codes 0x00 and 0xF0
          0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6, // order of these symbols was determined empirically by JPEG committee
          0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
          0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };
// Huffman definitions for second DC/AC tables (chrominance / Cb and Cr channels)
const uint8_t DcChrominanceCodesPerBitsize[16] = { 0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };   // sum = 12
const uint8_t DcChrominanceValues         [12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };         // => 12 codes (identical to DcLuminanceValues)
const uint8_t AcChrominanceCodesPerBitsize[16] = { 0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119 }; // sum = 162
const uint8_t AcChrominanceValues        [162] =                                        // => 162 codes
        { 0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91, // same number of symbol, just different order
          0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26, // (which is more efficient for AC coding)
          0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,
          0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
          0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,
          0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
          0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA };
const int16_t CodeWordLimit = 2048; // +/-2^11, maximum value after DCT

void fillInBuf(void)
{
    unsigned char status;

    // Reserve a few bytes at the beginning of the buffer for putting back ("stuffing") chars.
    gInBufOfs = 4;
    gInBufLeft = 0;

    status = (*g_pNeedBytesCallback)(gInBuf + gInBufOfs, PJPG_MAX_IN_BUF_SIZE - gInBufOfs, &gInBufLeft, g_pCallback_data);
    if (status)
    {
        // The user provided need bytes callback has indicated an error, so record the error and continue trying to decode.
        // The highest level pjpeg entrypoints will catch the error and return the non-zero status.
        gCallbackStatus = status;
    }
}

PJPG_INLINE uint8_t getChar(void)
{
    if (!gInBufLeft)
    {
        fillInBuf();
        if (!gInBufLeft)
        {
            gTemFlag = ~gTemFlag;
            return gTemFlag ? 0xFF : 0xD9;
        }
    }

    gInBufLeft--;
    return gInBuf[gInBufOfs++];
}

PJPG_INLINE void stuffChar(uint8_t i)
{
    gInBufOfs--;
    gInBuf[gInBufOfs] = i;
    gInBufLeft++;
}

PJPG_INLINE uint8_t getOctet(uint8_t FFCheck)
{
    uint8_t c = getChar();

    if ((FFCheck) && (c == 0xFF))
    {
        uint8_t n = getChar();

        if (n)
        {
            stuffChar(n);
            stuffChar(0xFF);
        }
    }

    return c;
}

uint16_t getBits(uint8_t numBits, uint8_t FFCheck)
{
    uint8_t origBits = numBits;
    uint16_t ret = gBitBuf;

    if (numBits > 8)
    {
        numBits -= 8;

        gBitBuf <<= gBitsLeft;

        gBitBuf |= getOctet(FFCheck);

        gBitBuf <<= (8 - gBitsLeft);

        ret = (ret & 0xFF00) | (gBitBuf >> 8);
    }

    if (gBitsLeft < numBits)
    {
        gBitBuf <<= gBitsLeft;

        gBitBuf |= getOctet(FFCheck);

        gBitBuf <<= (numBits - gBitsLeft);

        gBitsLeft = 8 - (numBits - gBitsLeft);
    }
    else
    {
        gBitsLeft = (uint8_t)(gBitsLeft - numBits);
        gBitBuf <<= numBits;
    }

    return ret >> (16 - origBits);
}

PJPG_INLINE uint16_t getBits1(uint8_t numBits)
{
return getBits(numBits, 0);
}

PJPG_INLINE uint16_t getBits2(uint8_t numBits)
{
return getBits(numBits, 1);
}

PJPG_INLINE uint8_t getBit(void)
{
    uint8_t ret = 0;
    if (gBitBuf & 0x8000)
        ret = 1;

    if (!gBitsLeft)
    {
        gBitBuf |= getOctet(1);

        gBitsLeft += 8;
    }

    gBitsLeft--;
    gBitBuf <<= 1;

    return ret;
}

uint8_t init(void)
{
    gImageXSize = 0;
    gImageYSize = 0;
    gCompsInFrame = 0;
    gRestartInterval = 0;
    gCompsInScan = 0;
    gValidHuffTables = 0;
    gValidQuantTables = 0;
    gTemFlag = 0;
    gInBufOfs = 0;
    gInBufLeft = 0;
    gBitBuf = 0;
    gBitsLeft = 8;

    getBits1(8);
    getBits1(8);

    return 0;
}

uint8_t locateSOIMarker(void)
{
    uint16_t bytesleft;

    uint8_t lastchar = (uint8_t)getBits1(8);

    uint8_t thischar = (uint8_t)getBits1(8);

    /* ok if it's a normal JPEG file without a special header */

    if ((lastchar == 0xFF) && (thischar == M_SOI))
        return 0;

    bytesleft = 4096; //512;

    for ( ; ; )
    {
        if (--bytesleft == 0)
            return PJPG_NOT_JPEG;

        lastchar = thischar;

        thischar = (uint8_t)getBits1(8);

        if (lastchar == 0xFF)
        {
            if (thischar == M_SOI)
                break;
            else if (thischar == M_EOI)	//getBits1 will keep returning M_EOI if we read past the end
                return PJPG_NOT_JPEG;
        }
    }

    /* Check the next character after marker: if it's not 0xFF, it can't
    be the start of the next marker, so the file is bad */

    thischar = (uint8_t)((gBitBuf >> 8) & 0xFF);

    if (thischar != 0xFF)
        return PJPG_NOT_JPEG;

    return 0;
}

uint8_t nextMarker(void)
{
    uint8_t c;
    uint8_t bytes = 0;

    do
    {
        do
        {
            bytes++;

            c = (uint8_t)getBits1(8);

        } while (c != 0xFF);

        do
        {
            c = (uint8_t)getBits1(8);

        } while (c == 0xFF);

    } while (c == 0);

    // If bytes > 0 here, there where extra bytes before the marker (not good).

    return c;
}

HuffTable* getHuffTable(uint8_t index)
{
    // 0-1 = DC
    // 2-3 = AC
    switch (index)
    {
        case 0: return &gHuffTab0;
        case 1: return &gHuffTab1;
        case 2: return &gHuffTab2;
        case 3: return &gHuffTab3;
        default: return 0;
    }
}

uint8_t* getHuffVal(uint8_t index)
{
    // 0-1 = DC
    // 2-3 = AC
    switch (index)
    {
        case 0: return gHuffVal0;
        case 1: return gHuffVal1;
        case 2: return gHuffVal2;
        case 3: return gHuffVal3;
        default: return 0;
    }
}

uint16_t getMaxHuffCodes(uint8_t index)
{
    return (index < 2) ? 12 : 255;
}

void huffCreate(const uint8_t* pBits, HuffTable* pHuffTable)
{
    uint8_t i = 0;
    uint8_t j = 0;

    uint16_t code = 0;

    for ( ; ; )
    {
        uint8_t num = pBits[i];

        if (!num)
        {
            pHuffTable->mMinCode[i] = 0x0000;
            pHuffTable->mMaxCode[i] = 0xFFFF;
            pHuffTable->mValPtr[i] = 0;
        }
        else
        {
            pHuffTable->mMinCode[i] = code;
            pHuffTable->mMaxCode[i] = code + num - 1;
            pHuffTable->mValPtr[i] = j;

            j = (uint8_t)(j + num);

            code = (uint16_t)(code + num);
        }

        code <<= 1;

        i++;
        if (i > 15)
            break;
    }
}

uint8_t readDHTMarker(void)
{
    uint8_t bits[16];
    uint16_t left = getBits1(16);

    if (left < 2)
        return PJPG_BAD_DHT_MARKER;

    left -= 2;

    while (left)
    {
        uint8_t i, tableIndex, index;
        uint8_t* pHuffVal;
        HuffTable* pHuffTable;
        uint16_t count, totalRead;

        index = (uint8_t)getBits1(8);

        if ( ((index & 0xF) > 1) || ((index & 0xF0) > 0x10) )
            return PJPG_BAD_DHT_INDEX;

        tableIndex = ((index >> 3) & 2) + (index & 1);

        pHuffTable = getHuffTable(tableIndex);
        pHuffVal = getHuffVal(tableIndex);

        gValidHuffTables |= (1 << tableIndex);

        count = 0;
        for (i = 0; i <= 15; i++)
        {
            uint8_t n = (uint8_t)getBits1(8);
            bits[i] = n;
            count = (uint16_t)(count + n);
        }

        if (count > getMaxHuffCodes(tableIndex))
            return PJPG_BAD_DHT_COUNTS;

        for (i = 0; i < count; i++)
            pHuffVal[i] = (uint8_t)getBits1(8);

        totalRead = 1 + 16 + count;

        if (left < totalRead)
            return PJPG_BAD_DHT_MARKER;

        left = (uint16_t)(left - totalRead);

        huffCreate(bits, pHuffTable);
    }

    return 0;
}

void createWinogradQuant(int16_t* pQuant)
{
    uint8_t i;

    for (i = 0; i < 64; i++)
    {
        long x = pQuant[i];
        x *= gWinogradQuant[i];
        pQuant[i] = (int16_t)((x + (1 << (PJPG_WINOGRAD_QUANT_SCALE_BITS - PJPG_DCT_SCALE_BITS - 1))) >> (PJPG_WINOGRAD_QUANT_SCALE_BITS - PJPG_DCT_SCALE_BITS));
    }
}

uint8_t readDQTMarker(void)
{
    uint16_t left = getBits1(16);

    if (left < 2)
        return PJPG_BAD_DQT_MARKER;

    left -= 2;

    while (left)
    {
        uint8_t i;
        uint8_t n = (uint8_t)getBits1(8);
        uint8_t prec = n >> 4;
        uint16_t totalRead;

        n &= 0x0F;

        if (n > 1)
            return PJPG_BAD_DQT_TABLE;

        gValidQuantTables |= (n ? 2 : 1);

        // read quantization entries, in zag order
        for (i = 0; i < 64; i++)
        {
            uint16_t temp = getBits1(8);

            if (prec)
                temp = (temp << 8) + getBits1(8);

            if (n)
                gQuant1[i] = (int16_t)temp;
            else
                gQuant0[i] = (int16_t)temp;
        }

        createWinogradQuant(n ? gQuant1 : gQuant0);

        totalRead = 64 + 1;

        if (prec)
            totalRead += 64;

        if (left < totalRead)
            return PJPG_BAD_DQT_LENGTH;

        left = (uint16_t)(left - totalRead);
    }

    return 0;
}

uint8_t readDRIMarker(void)
{
    if (getBits1(16) != 4)
        return PJPG_BAD_DRI_LENGTH;

    gRestartInterval = getBits1(16);

    return 0;
}

uint8_t skipVariableMarker(void)
{
    uint16_t left = getBits1(16);

    if (left < 2)
        return PJPG_BAD_VARIABLE_MARKER;

    left -= 2;

    while (left)
    {
        getBits1(8);
        left--;
    }

    return 0;
}

uint8_t processMarkers(uint8_t* pMarker)
{
    for ( ; ; )
    {
        uint8_t c = nextMarker();

        switch (c)
        {
            case M_SOF0:
            case M_SOF1:
            case M_SOF2:
            case M_SOF3:
            case M_SOF5:
            case M_SOF6:
            case M_SOF7:
                //      case M_JPG:
            case M_SOF9:
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
            case M_SOI:
            case M_EOI:
            case M_SOS:
            {
                *pMarker = c;
                return 0;
            }
            case M_DHT:
            {
                readDHTMarker();
                break;
            }
                // Sorry, no arithmetic support at this time. Dumb patents!
            case M_DAC:
            {
                return PJPG_NO_ARITHMITIC_SUPPORT;
            }
            case M_DQT:
            {
                readDQTMarker();
                break;
            }
            case M_DRI:
            {
                readDRIMarker();
                break;
            }
                //case M_APP0:  /* no need to read the JFIF marker */

            case M_JPG:
            case M_RST0:    /* no parameters */
            case M_RST1:
            case M_RST2:
            case M_RST3:
            case M_RST4:
            case M_RST5:
            case M_RST6:
            case M_RST7:
            case M_TEM:
            {
                return PJPG_UNEXPECTED_MARKER;
            }
            default:    /* must be DNL, DHP, EXP, APPn, JPGn, COM, or RESn or APP0 */
            {
                skipVariableMarker();
                break;
            }
        }
    }
//   return 0;
}

uint8_t readSOFMarker(void)
{
    uint8_t i;
    uint16_t left = getBits1(16);

    if (getBits1(8) != 8)
        return PJPG_BAD_PRECISION;

    gImageYSize = getBits1(16);

    if ((!gImageYSize) || (gImageYSize > PJPG_MAX_HEIGHT))
        return PJPG_BAD_HEIGHT;

    gImageXSize = getBits1(16);

    if ((!gImageXSize) || (gImageXSize > PJPG_MAX_WIDTH))
        return PJPG_BAD_WIDTH;

    gCompsInFrame = (uint8_t)getBits1(8);

    if (gCompsInFrame > 3)
        return PJPG_TOO_MANY_COMPONENTS;

    if (left != (gCompsInFrame + gCompsInFrame + gCompsInFrame + 8))
        return PJPG_BAD_SOF_LENGTH;

    for (i = 0; i < gCompsInFrame; i++)
    {
        gCompIdent[i] = (uint8_t)getBits1(8);
        gCompHSamp[i] = (uint8_t)getBits1(4);
        gCompVSamp[i] = (uint8_t)getBits1(4);
        gCompQuant[i] = (uint8_t)getBits1(8);

        if (gCompQuant[i] > 1)
            return PJPG_UNSUPPORTED_QUANT_TABLE;
    }

    return 0;
}

uint8_t locateSOFMarker(void)
{
    uint8_t c;

    uint8_t status = locateSOIMarker();
    if (status)
        return status;

    status = processMarkers(&c);
    if (status)
        return status;

    switch (c)
    {
        case M_SOF2:
        {
            // Progressive JPEG - not supported by picojpeg (would require too
            // much memory, or too many IDCT's for embedded systems).
            return PJPG_UNSUPPORTED_MODE;
        }
        case M_SOF0:  /* baseline DCT */
        {
            status = readSOFMarker();
            if (status)
                return status;

            break;
        }
        case M_SOF9:
        {
            return PJPG_NO_ARITHMITIC_SUPPORT;
        }
        case M_SOF1:  /* extended sequential DCT */
        default:
        {
            return PJPG_UNSUPPORTED_MARKER;
        }
    }

    return 0;
}

uint8_t readSOSMarker(void)
{
    uint8_t i;
    uint16_t left = getBits1(16);
    uint8_t spectral_start, spectral_end, successive_high, successive_low;

    gCompsInScan = (uint8_t)getBits1(8);

    left -= 3;

    if ( (left != (gCompsInScan + gCompsInScan + 3)) || (gCompsInScan < 1) || (gCompsInScan > PJPG_MAXCOMPSINSCAN) )
        return PJPG_BAD_SOS_LENGTH;

    for (i = 0; i < gCompsInScan; i++)
    {
        uint8_t cc = (uint8_t)getBits1(8);
        uint8_t c = (uint8_t)getBits1(8);
        uint8_t ci;

        left -= 2;

        for (ci = 0; ci < gCompsInFrame; ci++)
            if (cc == gCompIdent[ci])
                break;

        if (ci >= gCompsInFrame)
            return PJPG_BAD_SOS_COMP_ID;

        gCompList[i]    = ci;
        gCompDCTab[ci] = (c >> 4) & 15;
        gCompACTab[ci] = (c & 15);
    }

    spectral_start  = (uint8_t)getBits1(8);
    spectral_end    = (uint8_t)getBits1(8);
    successive_high = (uint8_t)getBits1(4);
    successive_low  = (uint8_t)getBits1(4);

    left -= 3;

    while (left)
    {
        getBits1(8);
        left--;
    }

    return 0;
}

uint8_t locateSOSMarker(uint8_t* pFoundEOI)
{
    uint8_t c;
    uint8_t status;

    *pFoundEOI = 0;

    status = processMarkers(&c);
    if (status)
        return status;

    if (c == M_EOI)
    {
        *pFoundEOI = 1;
        return 0;
    }
    else if (c != M_SOS)
        return PJPG_UNEXPECTED_MARKER;

    return readSOSMarker();
}

uint8_t checkHuffTables(void)
{
    uint8_t i;

    for (i = 0; i < gCompsInScan; i++)
    {
        uint8_t compDCTab = gCompDCTab[gCompList[i]];
        uint8_t compACTab = gCompACTab[gCompList[i]] + 2;

        if ( ((gValidHuffTables & (1 << compDCTab)) == 0) ||
             ((gValidHuffTables & (1 << compACTab)) == 0) )
            return PJPG_UNDEFINED_HUFF_TABLE;
    }

    return 0;
}

uint8_t checkQuantTables(void)
{
    uint8_t i;

    for (i = 0; i < gCompsInScan; i++)
    {
        uint8_t compQuantMask = gCompQuant[gCompList[i]] ? 2 : 1;

        if ((gValidQuantTables & compQuantMask) == 0)
            return PJPG_UNDEFINED_QUANT_TABLE;
    }

    return 0;
}

void fixInBuffer(void)
{
    /* In case any 0xFF's where pulled into the buffer during marker scanning */

    if (gBitsLeft > 0)
        stuffChar((uint8_t)gBitBuf);

    stuffChar((uint8_t)(gBitBuf >> 8));

    gBitsLeft = 8;
    getBits2(8);
    getBits2(8);
}

uint8_t initScan(void)
{
    uint8_t foundEOI;
    uint8_t status = locateSOSMarker(&foundEOI);
    if (status)
        return status;
    if (foundEOI)
        return PJPG_UNEXPECTED_MARKER;

    status = checkHuffTables();
    if (status)
        return status;

    status = checkQuantTables();
    if (status)
        return status;

    gLastDC[0] = 0;
    gLastDC[1] = 0;
    gLastDC[2] = 0;

    if (gRestartInterval)
    {
        gRestartsLeft = gRestartInterval;
        gNextRestartNum = 0;
    }

    fixInBuffer();

    return 0;
}

uint8_t initFrame(void)
{
    if (gCompsInFrame == 1)
    {
        if ((gCompHSamp[0] != 1) || (gCompVSamp[0] != 1))
            return PJPG_UNSUPPORTED_SAMP_FACTORS;

        gScanType = PJPG_GRAYSCALE;

        gMaxBlocksPerMCU = 1;
        gMCUOrg[0] = 0;

        gMaxMCUXSize     = 8;
        gMaxMCUYSize     = 8;
    }
    else if (gCompsInFrame == 3)
    {
        if ( ((gCompHSamp[1] != 1) || (gCompVSamp[1] != 1)) ||
             ((gCompHSamp[2] != 1) || (gCompVSamp[2] != 1)) )
            return PJPG_UNSUPPORTED_SAMP_FACTORS;

        if ((gCompHSamp[0] == 1) && (gCompVSamp[0] == 1))
        {
            gScanType = PJPG_YH1V1;

            gMaxBlocksPerMCU = 3;
            gMCUOrg[0] = 0;
            gMCUOrg[1] = 1;
            gMCUOrg[2] = 2;

            gMaxMCUXSize = 8;
            gMaxMCUYSize = 8;
        }
        else if ((gCompHSamp[0] == 1) && (gCompVSamp[0] == 2))
        {
            gScanType = PJPG_YH1V2;

            gMaxBlocksPerMCU = 4;
            gMCUOrg[0] = 0;
            gMCUOrg[1] = 0;
            gMCUOrg[2] = 1;
            gMCUOrg[3] = 2;

            gMaxMCUXSize = 8;
            gMaxMCUYSize = 16;
        }
        else if ((gCompHSamp[0] == 2) && (gCompVSamp[0] == 1))
        {
            gScanType = PJPG_YH2V1;

            gMaxBlocksPerMCU = 4;
            gMCUOrg[0] = 0;
            gMCUOrg[1] = 0;
            gMCUOrg[2] = 1;
            gMCUOrg[3] = 2;

            gMaxMCUXSize = 16;
            gMaxMCUYSize = 8;
        }
        else if ((gCompHSamp[0] == 2) && (gCompVSamp[0] == 2))
        {
            gScanType = PJPG_YH2V2;

            gMaxBlocksPerMCU = 6;
            gMCUOrg[0] = 0;
            gMCUOrg[1] = 0;
            gMCUOrg[2] = 0;
            gMCUOrg[3] = 0;
            gMCUOrg[4] = 1;
            gMCUOrg[5] = 2;

            gMaxMCUXSize = 16;
            gMaxMCUYSize = 16;
        }
        else
            return PJPG_UNSUPPORTED_SAMP_FACTORS;
    }
    else
        return PJPG_UNSUPPORTED_COLORSPACE;

    gMaxMCUSPerRow = (gImageXSize + (gMaxMCUXSize - 1)) >> ((gMaxMCUXSize == 8) ? 3 : 4);
    gMaxMCUSPerCol = (gImageYSize + (gMaxMCUYSize - 1)) >> ((gMaxMCUYSize == 8) ? 3 : 4);

    // This can overflow on large JPEG's.
    //gNumMCUSRemaining = gMaxMCUSPerRow * gMaxMCUSPerCol;
    gNumMCUSRemainingX = gMaxMCUSPerRow;
    gNumMCUSRemainingY = gMaxMCUSPerCol;

    return 0;
}

unsigned char pjpeg_decode_init(pjpeg_image_info_t *pInfo, pjpeg_need_bytes_callback_t pNeed_bytes_callback, void *pCallback_data, unsigned char reduce)
{
    uint8_t status;

    pInfo->m_width = 0; pInfo->m_height = 0; pInfo->m_comps = 0;
    pInfo->m_MCUSPerRow = 0; pInfo->m_MCUSPerCol = 0;
    pInfo->m_scanType = PJPG_GRAYSCALE;
    pInfo->m_MCUWidth = 0; pInfo->m_MCUHeight = 0;
    pInfo->m_pMCUBufR = (unsigned char*)0; pInfo->m_pMCUBufG = (unsigned char*)0; pInfo->m_pMCUBufB = (unsigned char*)0;

    g_pNeedBytesCallback = pNeed_bytes_callback;
    g_pCallback_data = pCallback_data;
    gCallbackStatus = 0;
    gReduce = reduce;

    status = init();
    if ((status) || (gCallbackStatus))
        return gCallbackStatus ? gCallbackStatus : status;

    status = locateSOFMarker();
    if ((status) || (gCallbackStatus))
        return gCallbackStatus ? gCallbackStatus : status;

    status = initFrame();
    if ((status) || (gCallbackStatus))
        return gCallbackStatus ? gCallbackStatus : status;

    status = initScan();
    if ((status) || (gCallbackStatus))
        return gCallbackStatus ? gCallbackStatus : status;

    pInfo->m_width = gImageXSize; pInfo->m_height = gImageYSize; pInfo->m_comps = gCompsInFrame;
    pInfo->m_scanType = gScanType;
    pInfo->m_MCUSPerRow = gMaxMCUSPerRow; pInfo->m_MCUSPerCol = gMaxMCUSPerCol;
    pInfo->m_MCUWidth = gMaxMCUXSize; pInfo->m_MCUHeight = gMaxMCUYSize;
    pInfo->m_pMCUBufR = gMCUBufR; pInfo->m_pMCUBufG = gMCUBufG; pInfo->m_pMCUBufB = gMCUBufB;

    return 0;
}

uint8_t processRestart(void)
{
    // Let's scan a little bit to find the marker, but not _too_ far.
    // 1536 is a "fudge factor" that determines how much to scan.
    uint16_t i;
    uint8_t c = 0;

    for (i = 1536; i > 0; i--)
        if (getChar() == 0xFF)
            break;

    if (i == 0)
        return PJPG_BAD_RESTART_MARKER;

    for ( ; i > 0; i--)
        if ((c = getChar()) != 0xFF)
            break;

    if (i == 0)
        return PJPG_BAD_RESTART_MARKER;

    // Is it the expected marker? If not, something bad happened.
    if (c != (gNextRestartNum + M_RST0))
        return PJPG_BAD_RESTART_MARKER;

    // Reset each component's DC prediction values.
    gLastDC[0] = 0;
    gLastDC[1] = 0;
    gLastDC[2] = 0;

    gRestartsLeft = gRestartInterval;

    gNextRestartNum = (gNextRestartNum + 1) & 7;

    // Get the bit buffer going again

    gBitsLeft = 8;
    getBits2(8);
    getBits2(8);

    return 0;
}

PJPG_INLINE uint8_t huffDecode(const HuffTable* pHuffTable, const uint8_t* pHuffVal)
{
    uint8_t i = 0;
    uint8_t j;
    uint16_t code = getBit();

    // This func only reads a bit at a time, which on modern CPU's is not terribly efficient.
    // But on microcontrollers without strong integer shifting support this seems like a
    // more reasonable approach.
    for ( ; ; )
    {
        uint16_t maxCode;

        if (i == 16)
            return 0;

        maxCode = pHuffTable->mMaxCode[i];
        if ((code <= maxCode) && (maxCode != 0xFFFF))
            break;

        i++;
        code <<= 1;
        code |= getBit();
    }

    j = pHuffTable->mValPtr[i];
    j = (uint8_t)(j + (code - pHuffTable->mMinCode[i]));

    return pHuffVal[j];
}

PJPG_INLINE int16_t huffExtend(uint16_t x, uint8_t s)
{
    return ((x < getExtendTest(s)) ? ((int16_t)x + getExtendOffset(s)) : (int16_t)x);
}

PJPG_INLINE int16_t imul_b1_b3(int16_t w)
{
    long x = (w * 362L);
    x += 128L;
    return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

PJPG_INLINE int16_t imul_b2(int16_t w)
{
    long x = (w * 669L);
    x += 128L;
    return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

PJPG_INLINE int16_t imul_b4(int16_t w)
{
    long x = (w * 277L);
    x += 128L;
    return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

PJPG_INLINE int16_t imul_b5(int16_t w)
{
    long x = (w * 196L);
    x += 128L;
    return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

PJPG_INLINE uint8_t clamp(int16_t s)
{
    if ((uint16_t)s > 255U)
    {
        if (s < 0)
            return 0;
        else if (s > 255)
            return 255;
    }

    return (uint8_t)s;
}

void idctRows()
{
    uint8_t i;
    int16_t* pSrc = gCoeffBuf;

    for (i = 0; i < 8; i++)
    {
        if ((pSrc[1] | pSrc[2] | pSrc[3] | pSrc[4] | pSrc[5] | pSrc[6] | pSrc[7]) == 0)
        {
            // Short circuit the 1D IDCT if only the DC component is non-zero
            int16_t src0 = *pSrc;

            *(pSrc+1) = src0;
            *(pSrc+2) = src0;
            *(pSrc+3) = src0;
            *(pSrc+4) = src0;
            *(pSrc+5) = src0;
            *(pSrc+6) = src0;
            *(pSrc+7) = src0;
        }
        else
        {
            int16_t src4 = *(pSrc+5);
            int16_t src7 = *(pSrc+3);
            int16_t x4  = src4 - src7;
            int16_t x7  = src4 + src7;

            int16_t src5 = *(pSrc+1);
            int16_t src6 = *(pSrc+7);
            int16_t x5  = src5 + src6;
            int16_t x6  = src5 - src6;

            int16_t tmp1 = imul_b5(x4 - x6);
            int16_t stg26 = imul_b4(x6) - tmp1;

            int16_t x24 = tmp1 - imul_b2(x4);

            int16_t x15 = x5 - x7;
            int16_t x17 = x5 + x7;

            int16_t tmp2 = stg26 - x17;
            int16_t tmp3 = imul_b1_b3(x15) - tmp2;
            int16_t x44 = tmp3 + x24;

            int16_t src0 = *(pSrc+0);
            int16_t src1 = *(pSrc+4);
            int16_t x30 = src0 + src1;
            int16_t x31 = src0 - src1;

            int16_t src2 = *(pSrc+2);
            int16_t src3 = *(pSrc+6);
            int16_t x12 = src2 - src3;
            int16_t x13 = src2 + src3;

            int16_t x32 = imul_b1_b3(x12) - x13;

            int16_t x40 = x30 + x13;
            int16_t x43 = x30 - x13;
            int16_t x41 = x31 + x32;
            int16_t x42 = x31 - x32;

            *(pSrc+0) = x40 + x17;
            *(pSrc+1) = x41 + tmp2;
            *(pSrc+2) = x42 + tmp3;
            *(pSrc+3) = x43 - x44;
            *(pSrc+4) = x43 + x44;
            *(pSrc+5) = x42 - tmp3;
            *(pSrc+6) = x41 - tmp2;
            *(pSrc+7) = x40 - x17;
        }

        pSrc += 8;
    }
}

void idctCols()
{
    uint8_t i;

    int16_t* pSrc = gCoeffBuf;

    for (i = 0; i < 8; i++)
    {
        if ((pSrc[1*8] | pSrc[2*8] | pSrc[3*8] | pSrc[4*8] | pSrc[5*8] | pSrc[6*8] | pSrc[7*8]) == 0)
        {
            // Short circuit the 1D IDCT if only the DC component is non-zero
            //uint8_t c = clamp(PJPG_DESCALE(*pSrc) + 128);
            uint8_t c = clamp(PJPG_DESCALE(*pSrc));
            *(pSrc+0*8) = c;
            *(pSrc+1*8) = c;
            *(pSrc+2*8) = c;
            *(pSrc+3*8) = c;
            *(pSrc+4*8) = c;
            *(pSrc+5*8) = c;
            *(pSrc+6*8) = c;
            *(pSrc+7*8) = c;
        }
        else
        {
            int16_t src4 = *(pSrc+5*8);
            int16_t src7 = *(pSrc+3*8);
            int16_t x4  = src4 - src7;
            int16_t x7  = src4 + src7;

            int16_t src5 = *(pSrc+1*8);
            int16_t src6 = *(pSrc+7*8);
            int16_t x5  = src5 + src6;
            int16_t x6  = src5 - src6;

            int16_t tmp1 = imul_b5(x4 - x6);
            int16_t stg26 = imul_b4(x6) - tmp1;

            int16_t x24 = tmp1 - imul_b2(x4);

            int16_t x15 = x5 - x7;
            int16_t x17 = x5 + x7;

            int16_t tmp2 = stg26 - x17;
            int16_t tmp3 = imul_b1_b3(x15) - tmp2;
            int16_t x44 = tmp3 + x24;

            int16_t src0 = *(pSrc+0*8);
            int16_t src1 = *(pSrc+4*8);
            int16_t x30 = src0 + src1;
            int16_t x31 = src0 - src1;

            int16_t src2 = *(pSrc+2*8);
            int16_t src3 = *(pSrc+6*8);
            int16_t x12 = src2 - src3;
            int16_t x13 = src2 + src3;

            int16_t x32 = imul_b1_b3(x12) - x13;

            int16_t x40 = x30 + x13;
            int16_t x43 = x30 - x13;
            int16_t x41 = x31 + x32;
            int16_t x42 = x31 - x32;

            // descale, convert to unsigned and clamp to 8-bit
           /* *(pSrc+0*8) = clamp(PJPG_DESCALE(x40 + x17)  + 128);
            *(pSrc+1*8) = clamp(PJPG_DESCALE(x41 + tmp2) + 128);
            *(pSrc+2*8) = clamp(PJPG_DESCALE(x42 + tmp3) + 128);
            *(pSrc+3*8) = clamp(PJPG_DESCALE(x43 - x44)  + 128);
            *(pSrc+4*8) = clamp(PJPG_DESCALE(x43 + x44)  + 128);
            *(pSrc+5*8) = clamp(PJPG_DESCALE(x42 - tmp3) + 128);
            *(pSrc+6*8) = clamp(PJPG_DESCALE(x41 - tmp2) + 128);
            *(pSrc+7*8) = clamp(PJPG_DESCALE(x40 - x17)  + 128);*/
            *(pSrc+0*8) = clamp(PJPG_DESCALE(x40 + x17) );
            *(pSrc+1*8) = clamp(PJPG_DESCALE(x41 + tmp2));
            *(pSrc+2*8) = clamp(PJPG_DESCALE(x42 + tmp3));
            *(pSrc+3*8) = clamp(PJPG_DESCALE(x43 - x44));
            *(pSrc+4*8) = clamp(PJPG_DESCALE(x43 + x44));
            *(pSrc+5*8) = clamp(PJPG_DESCALE(x42 - tmp3));
            *(pSrc+6*8) = clamp(PJPG_DESCALE(x41 - tmp2));
            *(pSrc+7*8) = clamp(PJPG_DESCALE(x40 - x17));
        }

        pSrc++;
    }
}

PJPG_INLINE uint8_t addAndClamp(uint8_t a, int16_t b)
{
    b = a + b;

    if ((uint16_t)b > 255)
    {
        if (b < 0)
            return 0;
        else if (b > 255)
            return 255;
    }

    return (uint8_t)b;
}

PJPG_INLINE uint8_t subAndClamp(uint8_t a, int16_t b)
{
    b = a - b;

    if ((uint16_t)b > 255)
    {
        if (b < 0)
            return 0;
        else if (b > 255)
            return 255;
    }

    return (uint8_t)b;
}

void copyY(uint8_t dstOfs)
{
    uint8_t i;
    uint8_t* pRDst = gMCUBufR + dstOfs;
    uint8_t* pGDst = gMCUBufG + dstOfs;
    uint8_t* pBDst = gMCUBufB + dstOfs;
    int16_t* pSrc = gCoeffBuf;

    for (i = 64; i > 0; i--)
    {
        uint8_t c = (uint8_t)*pSrc++;
        *pRDst++ = c;
        *pGDst++ = c;
        *pBDst++ = c;
    }
}

void convertCb(uint8_t dstOfs)
{
    uint8_t i;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    uint8_t* pDstB = gMCUBufB + dstOfs;
    int16_t* pSrc = gCoeffBuf;

    for (i = 64; i > 0; i--)
    {
        uint8_t cb = (uint8_t)*pSrc++;
        int16_t cbG, cbB;

        cbG = ((cb * 88U) >> 8U) - 44U;
        *pDstG++ = subAndClamp(pDstG[0], cbG);

        cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
        *pDstB++ = addAndClamp(pDstB[0], cbB);
    }
}

void convertCr(uint8_t dstOfs)
{
    uint8_t i;
    uint8_t* pDstR = gMCUBufR + dstOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    int16_t* pSrc = gCoeffBuf;

    for (i = 64; i > 0; i--)
    {
        uint8_t cr = (uint8_t)*pSrc++;
        int16_t crR, crG;

        crR = (cr + ((cr * 103U) >> 8U)) - 179;
        *pDstR++ = addAndClamp(pDstR[0], crR);

        crG = ((cr * 183U) >> 8U) - 91;
        *pDstG++ = subAndClamp(pDstG[0], crG);
    }
}

void upsampleCb(uint8_t srcOfs, uint8_t dstOfs)
{
    // Cb - affects G and B
    uint8_t x, y;
    int16_t* pSrc = gCoeffBuf + srcOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    uint8_t* pDstB = gMCUBufB + dstOfs;
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            uint8_t cb = (uint8_t)*pSrc++;
            int16_t cbG, cbB;

            cbG = ((cb * 88) >> 8) - 44;
            pDstG[0] = subAndClamp(pDstG[0], cbG);
            pDstG[1] = subAndClamp(pDstG[1], cbG);
            pDstG[8] = subAndClamp(pDstG[8], cbG);
            pDstG[9] = subAndClamp(pDstG[9], cbG);

            cbB = (cb + ((cb * 198) >> 8)) - 227;
            pDstB[0] = addAndClamp(pDstB[0], cbB);
            pDstB[1] = addAndClamp(pDstB[1], cbB);
            pDstB[8] = addAndClamp(pDstB[8], cbB);
            pDstB[9] = addAndClamp(pDstB[9], cbB);

            pDstG += 2;
            pDstB += 2;
        }

        pSrc = pSrc - 4 + 8;
        pDstG = pDstG - 8 + 16;
        pDstB = pDstB - 8 + 16;
    }
}

void upsampleCbH(uint8_t srcOfs, uint8_t dstOfs)
{
    // Cb - affects G and B
    uint8_t x, y;
    int16_t* pSrc = gCoeffBuf + srcOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    uint8_t* pDstB = gMCUBufB + dstOfs;
    for (y = 0; y < 8; y++)
    {
        for (x = 0; x < 4; x++)
        {
            uint8_t cb = (uint8_t)*pSrc++;
            int16_t cbG, cbB;

            cbG = ((cb * 88U) >> 8U) - 44U;
            pDstG[0] = subAndClamp(pDstG[0], cbG);
            pDstG[1] = subAndClamp(pDstG[1], cbG);

            cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
            pDstB[0] = addAndClamp(pDstB[0], cbB);
            pDstB[1] = addAndClamp(pDstB[1], cbB);

            pDstG += 2;
            pDstB += 2;
        }

        pSrc = pSrc - 4 + 8;
    }
}

void upsampleCbV(uint8_t srcOfs, uint8_t dstOfs)
{
    uint8_t x, y;
    int16_t* pSrc = gCoeffBuf + srcOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    uint8_t* pDstB = gMCUBufB + dstOfs;
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 8; x++)
        {
            uint8_t cb = (uint8_t)*pSrc++;
            int16_t cbG, cbB;

            cbG = ((cb * 88U) >> 8U) - 44U;
            pDstG[0] = subAndClamp(pDstG[0], cbG);
            pDstG[8] = subAndClamp(pDstG[8], cbG);

            cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
            pDstB[0] = addAndClamp(pDstB[0], cbB);
            pDstB[8] = addAndClamp(pDstB[8], cbB);

            ++pDstG;
            ++pDstB;
        }

        pDstG = pDstG - 8 + 16;
        pDstB = pDstB - 8 + 16;
    }
}

void upsampleCr(uint8_t srcOfs, uint8_t dstOfs)
{
    // Cr - affects R and G
    uint8_t x, y;
    int16_t* pSrc = gCoeffBuf + srcOfs;
    uint8_t* pDstR = gMCUBufR + dstOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            uint8_t cr = (uint8_t)*pSrc++;
            int16_t crR, crG;

            crR = (cr + ((cr * 103) >> 8)) - 179;
            pDstR[0] = addAndClamp(pDstR[0], crR);
            pDstR[1] = addAndClamp(pDstR[1], crR);
            pDstR[8] = addAndClamp(pDstR[8], crR);
            pDstR[9] = addAndClamp(pDstR[9], crR);

            crG = ((cr * 183) >> 8) - 91;
            pDstG[0] = subAndClamp(pDstG[0], crG);
            pDstG[1] = subAndClamp(pDstG[1], crG);
            pDstG[8] = subAndClamp(pDstG[8], crG);
            pDstG[9] = subAndClamp(pDstG[9], crG);

            pDstR += 2;
            pDstG += 2;
        }

        pSrc = pSrc - 4 + 8;
        pDstR = pDstR - 8 + 16;
        pDstG = pDstG - 8 + 16;
    }
}

void upsampleCrH(uint8_t srcOfs, uint8_t dstOfs)
{
    // Cr - affects R and G
    uint8_t x, y;
    int16_t* pSrc = gCoeffBuf + srcOfs;
    uint8_t* pDstR = gMCUBufR + dstOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    for (y = 0; y < 8; y++)
    {
        for (x = 0; x < 4; x++)
        {
            uint8_t cr = (uint8_t)*pSrc++;
            int16_t crR, crG;

            crR = (cr + ((cr * 103U) >> 8U)) - 179;
            pDstR[0] = addAndClamp(pDstR[0], crR);
            pDstR[1] = addAndClamp(pDstR[1], crR);

            crG = ((cr * 183U) >> 8U) - 91;
            pDstG[0] = subAndClamp(pDstG[0], crG);
            pDstG[1] = subAndClamp(pDstG[1], crG);

            pDstR += 2;
            pDstG += 2;
        }

        pSrc = pSrc - 4 + 8;
    }
}

void upsampleCrV(uint8_t srcOfs, uint8_t dstOfs)
{
    // Cr - affects R and G
    uint8_t x, y;
    int16_t* pSrc = gCoeffBuf + srcOfs;
    uint8_t* pDstR = gMCUBufR + dstOfs;
    uint8_t* pDstG = gMCUBufG + dstOfs;
    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 8; x++)
        {
            uint8_t cr = (uint8_t)*pSrc++;
            int16_t crR, crG;

            crR = (cr + ((cr * 103U) >> 8U)) - 179;
            pDstR[0] = addAndClamp(pDstR[0], crR);
            pDstR[8] = addAndClamp(pDstR[8], crR);

            crG = ((cr * 183U) >> 8U) - 91;
            pDstG[0] = subAndClamp(pDstG[0], crG);
            pDstG[8] = subAndClamp(pDstG[8], crG);

            ++pDstR;
            ++pDstG;
        }

        pDstR = pDstR - 8 + 16;
        pDstG = pDstG - 8 + 16;
    }
}

void transformBlock(uint8_t mcuBlock)
{
    idctRows();
    idctCols();

    switch (gScanType)
    {
        case PJPG_GRAYSCALE:
        {
            // MCU size: 1, 1 block per MCU
            copyY(0);
            break;
        }
        case PJPG_YH1V1:
        {
            // MCU size: 8x8, 3 blocks per MCU
            switch (mcuBlock)
            {
                case 0:
                {
                    copyY(0);
                    break;
                }
                case 1:
                {
                    convertCb(0);
                    break;
                }
                case 2:
                {
                    convertCr(0);
                    break;
                }
            }

            break;
        }
        case PJPG_YH1V2:
        {
            // MCU size: 8x16, 4 blocks per MCU
            switch (mcuBlock)
            {
                case 0:
                {
                    copyY(0);
                    break;
                }
                case 1:
                {
                    copyY(128);
                    break;
                }
                case 2:
                {
                    upsampleCbV(0, 0);
                    upsampleCbV(4*8, 128);
                    break;
                }
                case 3:
                {
                    upsampleCrV(0, 0);
                    upsampleCrV(4*8, 128);
                    break;
                }
            }

            break;
        }
        case PJPG_YH2V1:
        {
            // MCU size: 16x8, 4 blocks per MCU
            switch (mcuBlock)
            {
                case 0:
                {
                    copyY(0);
                    break;
                }
                case 1:
                {
                    copyY(64);
                    break;
                }
                case 2:
                {
                    upsampleCbH(0, 0);
                    upsampleCbH(4, 64);
                    break;
                }
                case 3:
                {
                    upsampleCrH(0, 0);
                    upsampleCrH(4, 64);
                    break;
                }
            }

            break;
        }
        case PJPG_YH2V2:
        {
            // MCU size: 16x16, 6 blocks per MCU
            switch (mcuBlock)
            {
                case 0:
                {
                    copyY(0);
                    break;
                }
                case 1:
                {
                    copyY(64);
                    break;
                }
                case 2:
                {
                    copyY(128);
                    break;
                }
                case 3:
                {
                    copyY(192);
                    break;
                }
                case 4:
                {
                    upsampleCb(0, 0);
                    upsampleCb(4, 64);
                    upsampleCb(4*8, 128);
                    upsampleCb(4+4*8, 192);
                    break;
                }
                case 5:
                {
                    upsampleCr(0, 0);
                    upsampleCr(4, 64);
                    upsampleCr(4*8, 128);
                    upsampleCr(4+4*8, 192);
                    break;
                }
            }

            break;
        }
    }
}

uint8_t decodeNextMCU(int frame, int mode)
{   //mode 0 - getting coeffs mode 1 - setting coeffs
    uint8_t status;
    uint8_t mcuBlock;

    if (gRestartInterval)
    {
        if (gRestartsLeft == 0)
        {
            status = processRestart();
            if (status)
                return status;
        }
        gRestartsLeft--;
    }

    for (mcuBlock = 0; mcuBlock < gMaxBlocksPerMCU; mcuBlock++) //6 razy dla H2V2
    {

        uint8_t componentID = gMCUOrg[mcuBlock];
        uint8_t compQuant = gCompQuant[componentID];
        uint8_t compDCTab = gCompDCTab[componentID];
        uint8_t numExtraBits, compACTab, k;
        const int16_t* pQ = compQuant ? gQuant1 : gQuant0;
        uint16_t r, dc;

        uint8_t s = huffDecode(compDCTab ? &gHuffTab1 : &gHuffTab0, compDCTab ? gHuffVal1 : gHuffVal0);

        r = 0;
        numExtraBits = s & 0xF;
        if (numExtraBits)
            r = getBits2(numExtraBits);
        dc = huffExtend(r, s);

        dc = dc + gLastDC[componentID];
        gLastDC[componentID] = dc;

        if(mode) {

            for (int i = 0; i < 64; i++) {
                gCoeffBuf_scaled[i] = coeffs[frame][counter];
                counter++;
            }
        }

        if(mode)
            gCoeffBuf[0] = gCoeffBuf_scaled[0] * pQ[0];
        else
            gCoeffBuf_scaled[0] = dc;

        compACTab = gCompACTab[componentID];

        if (gReduce)
        {
            // Decode, but throw out the AC coefficients in reduce mode.
            for (k = 1; k < 64; k++)
            {
                s = huffDecode(compACTab ? &gHuffTab3 : &gHuffTab2, compACTab ? gHuffVal3 : gHuffVal2);

                numExtraBits = s & 0xF;
                if (numExtraBits)
                    getBits2(numExtraBits);

                r = s >> 4;
                s &= 15;

                if (s)
                {
                    if (r)
                    {
                        if ((k + r) > 63)
                            return PJPG_DECODE_ERROR;

                        k = (uint8_t)(k + r);
                    }
                }
                else
                {
                    if (r == 15)
                    {
                        if ((k + 16) > 64)
                            return PJPG_DECODE_ERROR;

                        k += (16 - 1); // - 1 because the loop counter is k
                    }
                    else
                        break;
                }
            }

          //  transformBlockReduce(mcuBlock);
        }
        else
        {
            // Decode and dequantize AC coefficients
            for (k = 1; k < 64; k++)
            {
                uint16_t extraBits;

                s = huffDecode(compACTab ? &gHuffTab3 : &gHuffTab2, compACTab ? gHuffVal3 : gHuffVal2);

                extraBits = 0;
                numExtraBits = s & 0xF;
                if (numExtraBits)
                    extraBits = getBits2(numExtraBits);

                r = s >> 4;
                s &= 15;

                if (s)
                {
                    int16_t ac;

                    if (r)
                    {
                        if ((k + r) > 63)
                            return PJPG_DECODE_ERROR;
                        //  return (k+r);

                        while (r)
                        {
                            //k++
                                // gCoeffBuf[ZAG[k++]] = 0;
                          //  gCoeffBuf[ZAG[k]] = 0;
                            if(mode)
                                //k++;
                                gCoeffBuf[ZAG[k++]] = 0;
                            else
                                gCoeffBuf_scaled[ZAG[k++]] = 0;
                            r--;
                        }
                    }

                    ac = huffExtend(extraBits, s);

                        //gCoeffBuf[ZAG[k]] = ac * pQ[k];
                    if(mode)
                        gCoeffBuf[ZAG[k]] = gCoeffBuf_scaled[ZAG[k]] * pQ[k];
                    else
                        gCoeffBuf_scaled[ZAG[k]] = ac;
                }
                else
                {
                    if (r == 15)
                    {
                        if ((k + 16) > 64)
                            return PJPG_DECODE_ERROR;

                        for (r = 16; r > 0; r--) {
                           // k++;
                           //    gCoeffBuf[ZAG[k++]] = 0;
                       //     gCoeffBuf[ZAG[k]] = 0;
                            if(mode)
                               // k++;
                                gCoeffBuf[ZAG[k++]] = 0;
                            else
                                gCoeffBuf_scaled[ZAG[k++]] = 0;
                        }

                        k--; // - 1 because the loop counter is k
                    }
                    else
                        break;
                }
            }

            while (k < 64) {
               // k++;
               //    gCoeffBuf[ZAG[k++]] = 0;
             //   gCoeffBuf[ZAG[k]] = 0;
                if(mode)
                   // k++;
                    gCoeffBuf[ZAG[k++]] = 0;
                else
                    gCoeffBuf_scaled[ZAG[k++]] = 0;
            }

            if(mode)
            {
                transformBlock(mcuBlock);
            }
            else {

               for (int i = 0; i < 64; i++) {
                   coeffs[frame][counter] = gCoeffBuf_scaled[i];
                   counter++;
               }
           }
        }
    }

    return 0;
}

uint8_t decodeNextMCU3D(int frame, int mode)
{   //mode 0 - getting coeffs mode 1 - setting coeffs
    uint8_t status;
    uint8_t mcuBlock;

    if (gRestartInterval)
    {
        if (gRestartsLeft == 0)
        {
            status = processRestart();
            if (status)
                return status;
        }
        gRestartsLeft--;
    }

    for (mcuBlock = 0; mcuBlock < gMaxBlocksPerMCU; mcuBlock++) //6 razy dla H2V2
    {

        uint8_t componentID = gMCUOrg[mcuBlock];
        uint8_t compQuant = gCompQuant[componentID];
        uint8_t compDCTab = gCompDCTab[componentID];
        uint8_t numExtraBits, compACTab;
        const int16_t* pQ = compQuant ? gQuant1 : gQuant0;
        uint16_t r, dc, k;

        uint8_t s = huffDecode(compDCTab ? &gHuffTab1 : &gHuffTab0, compDCTab ? gHuffVal1 : gHuffVal0);

        r = 0;
        numExtraBits = s & 0xF;
        if (numExtraBits)
            r = getBits2(numExtraBits);
        dc = huffExtend(r, s);

        dc = dc + gLastDC[componentID];
        gLastDC[componentID] = dc;

        zigzagcoeffs[counter][0] = dc;

        compACTab = gCompACTab[componentID];

        if (gReduce)
        {
            // Decode, but throw out the AC coefficients in reduce mode.
            for (k = 1; k < 64; k++)
            {
                s = huffDecode(compACTab ? &gHuffTab3 : &gHuffTab2, compACTab ? gHuffVal3 : gHuffVal2);

                numExtraBits = s & 0xF;
                if (numExtraBits)
                    getBits2(numExtraBits);

                r = s >> 4;
                s &= 15;

                if (s)
                {
                    if (r)
                    {
                        if ((k + r) > 63)
                            return PJPG_DECODE_ERROR;

                        k = (uint8_t)(k + r);
                    }
                }
                else
                {
                    if (r == 15)
                    {
                        if ((k + 16) > 64)
                            return PJPG_DECODE_ERROR;

                        k += (16 - 1); // - 1 because the loop counter is k
                    }
                    else
                        break;
                }
            }

            //  transformBlockReduce(mcuBlock);
        }
        else
        {
            // Decode and dequantize AC coefficients
            for (k = 1; k < 512; k++)
            {
                uint16_t extraBits;

                s = huffDecode(compACTab ? &gHuffTab3 : &gHuffTab2, compACTab ? gHuffVal3 : gHuffVal2);

                extraBits = 0;
                numExtraBits = s & 0xF;
                if (numExtraBits)
                    extraBits = getBits2(numExtraBits);

                r = s >> 4;
                s &= 15;

                if (s)
                {
                    int16_t ac;

                    if (r)
                    {
                        if ((k + r) > 511)
                           return PJPG_DECODE_ERROR;
                        //  return (k+r);

                        while (r)
                        {
                            zigzagcoeffs[counter][k++] = 0;
                            r--;
                        }
                    }

                    ac = huffExtend(extraBits, s);

                    zigzagcoeffs[counter][k] = ac;
                }
                else
                {
                    if (r == 15)
                    {
                        if ((k + 16) > 512)
                            return PJPG_DECODE_ERROR;

                        for (r = 16; r > 0; r--) {
                            zigzagcoeffs[counter][k++] = 0;
                        }

                        k--; // - 1 because the loop counter is k
                    }
                    else
                        break;
                }
            }
            while (k < 512) {
                zigzagcoeffs[counter][k++] = 0;
            }
        }
        counter++;
    }

    return 0;
}

unsigned char pjpeg_decode_mcu(int frame, int mode, int D3)
{
    uint8_t status;

    if (gCallbackStatus)
        return gCallbackStatus;

    if ((!gNumMCUSRemainingX) && (!gNumMCUSRemainingY))
        return PJPG_NO_MORE_BLOCKS;

    if(D3)
        status = decodeNextMCU3D(frame, mode);
    else
        status = decodeNextMCU(frame, mode);

    if ((status) || (gCallbackStatus))
        return gCallbackStatus ? gCallbackStatus : status;

    gNumMCUSRemainingX--;
    if (!gNumMCUSRemainingX)
    {
        gNumMCUSRemainingY--;
        if (gNumMCUSRemainingY > 0)
            gNumMCUSRemainingX = gMaxMCUSPerRow;
    }

    return 0;
}

uint8_t *pjpeg_load_from_file(const char *pFilename, int *x, int *y, int *comps, pjpeg_scan_type_t *pScan_type, int reduce, int frame, int mode, int D3) {
    pjpeg_image_info_t image_info;
    int mcu_x = 0;
    int mcu_y = 0;
    unsigned int row_pitch;
    uint8_t *pImage;
    uint8_t status;
    unsigned int decoded_width, decoded_height;
    unsigned int row_blocks_per_mcu, col_blocks_per_mcu;

    *x = 0;
    *y = 0;
    *comps = 0;
    if (pScan_type) *pScan_type = PJPG_GRAYSCALE;

    g_pInFile = fopen(pFilename, "rb");
    if (!g_pInFile)
        return NULL;

    g_nInFileOfs = 0;

    fseek(g_pInFile, 0, SEEK_END);
    g_nInFileSize = ftell(g_pInFile);
    fseek(g_pInFile, 0, SEEK_SET);

    status = pjpeg_decode_init(&image_info, pjpeg_need_bytes_callback, NULL, (unsigned char)reduce);

    if (status) {
        printf("pjpeg_decode_init() failed with status %u\n", status);
        if (status == PJPG_UNSUPPORTED_MODE) {
            printf("Progressive JPEG files are not supported.\n");
        }

        fclose(g_pInFile);
        return NULL;
    }

    if (pScan_type)
        *pScan_type = image_info.m_scanType;

    // In reduce mode output 1 pixel per 8x8 block.
    decoded_width = reduce ? (image_info.m_MCUSPerRow * image_info.m_MCUWidth) / 8 : image_info.m_width;
    decoded_height = reduce ? (image_info.m_MCUSPerCol * image_info.m_MCUHeight) / 8 : image_info.m_height;

    row_pitch = decoded_width * image_info.m_comps;
    pImage = (uint8_t *) malloc(row_pitch * decoded_height); //dla kolorowego full HD = 1920 * 1080 * 3 = 6220800

    if (!pImage) {
        fclose(g_pInFile);
        return NULL;
    }

    row_blocks_per_mcu = image_info.m_MCUWidth >> 3; //mamy 16 i dzielimy przez 8, czyli mamy dwa bloki
    col_blocks_per_mcu = image_info.m_MCUHeight >> 3;

    for (;;) {
        int y, x;
        uint8_t *pDst_row;
        status = pjpeg_decode_mcu(frame, mode, D3);

        if (status) {
            if (status != PJPG_NO_MORE_BLOCKS) {
                printf("pjpeg_decode_mcu() failed with status %u\n", status);

                free(pImage);
                fclose(g_pInFile);
                return NULL;
            }

            break;
        }

        if (mcu_y >= image_info.m_MCUSPerCol) {
            free(pImage);
            fclose(g_pInFile);
            return NULL;
        }

        if (reduce) {
            // In reduce mode, only the first pixel of each 8x8 block is valid.
            pDst_row =
                    pImage + mcu_y * col_blocks_per_mcu * row_pitch + mcu_x * row_blocks_per_mcu * image_info.m_comps;
            if (image_info.m_scanType == PJPG_GRAYSCALE) {
                *pDst_row = image_info.m_pMCUBufR[0];
            } else {
                unsigned int y, x;
                for (y = 0; y < col_blocks_per_mcu; y++) {
                    unsigned int src_ofs = (y * 128U);
                    for (x = 0; x < row_blocks_per_mcu; x++) {
                        pDst_row[0] = image_info.m_pMCUBufR[src_ofs];
                        pDst_row[1] = image_info.m_pMCUBufG[src_ofs];
                        pDst_row[2] = image_info.m_pMCUBufB[src_ofs];
                        pDst_row += 3;
                        src_ofs += 64;
                    }

                    pDst_row += row_pitch - 3 * row_blocks_per_mcu;
                }
            }
        } else {
            // Copy MCU's pixel blocks into the destination bitmap.
            pDst_row = pImage + (mcu_y * image_info.m_MCUHeight) * row_pitch +
                       (mcu_x * image_info.m_MCUWidth * image_info.m_comps);

            for (y = 0; y < image_info.m_MCUHeight; y += 8) {
                const int by_limit = min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

                for (x = 0; x < image_info.m_MCUWidth; x += 8) {

                    uint8_t *pDst_block = pDst_row + x * image_info.m_comps;

                    // Compute source byte offset of the block in the decoder's MCU buffer.
                    unsigned int src_ofs = (x * 8U) + (y * 16U);
                    const uint8_t *pSrcR = image_info.m_pMCUBufR + src_ofs;
                    const uint8_t *pSrcG = image_info.m_pMCUBufG + src_ofs;
                    const uint8_t *pSrcB = image_info.m_pMCUBufB + src_ofs;

                    const int bx_limit = min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

                    if (image_info.m_scanType == PJPG_GRAYSCALE) {
                        int bx, by;
                        for (by = 0; by < by_limit; by++) {
                            uint8_t *pDst = pDst_block;

                            for (bx = 0; bx < bx_limit; bx++)
                                *pDst++ = *pSrcR++;

                            pSrcR += (8 - bx_limit);

                            pDst_block += row_pitch;
                        }
                    } else {
                        int bx, by;
                        for (by = 0; by < by_limit; by++) {
                            uint8_t *pDst = pDst_block;

                            for (bx = 0; bx < bx_limit; bx++) {
                                pDst[0] = *pSrcR++;
                                pDst[1] = *pSrcG++;
                                pDst[2] = *pSrcB++;
                                pDst += 3;
                            }

                            pSrcR += (8 - bx_limit);
                            pSrcG += (8 - bx_limit);
                            pSrcB += (8 - bx_limit);

                            pDst_block += row_pitch;
                        }
                    }
                }

                pDst_row += (row_pitch * 8);
            }
        }

        mcu_x++;
        if (mcu_x == image_info.m_MCUSPerRow) //1920 po 16 mcus = 120
        {
            mcu_x = 0;
            mcu_y++;
        }
    }

    fclose(g_pInFile);

    *x = decoded_width;
    *y = decoded_height;
    *comps = image_info.m_comps;

    return pImage;
}

//tooJPEGpart:

std::ofstream myFile;

void myOutput(unsigned char byte)
{
    myFile << byte;
}

typedef void (*WRITE_ONE_BYTE)(unsigned char);

struct BitCode
{
    BitCode() = default; // undefined state, must be initialized at a later time
    BitCode(uint16_t code_, uint8_t numBits_)
            : code(code_), numBits(numBits_) {}
    uint16_t code;       // JPEG's Huffman codes are limited to 16 bits
    uint8_t  numBits;    // number of valid bits
};

struct BitWriter
{
    // user-supplied callback that writes/stores one byte
    WRITE_ONE_BYTE output;
    // initialize writer
    explicit BitWriter(WRITE_ONE_BYTE output_) : output(output_) {}

    // store the most recently encoded bits that are not written yet
    struct BitBuffer
    {
        int32_t data    = 0; // actually only at most 24 bits are used
        uint8_t numBits = 0; // number of valid bits (the right-most bits)
    } buffer;

    // write Huffman bits stored in BitCode, keep excess bits in BitBuffer
    BitWriter& operator<<(const BitCode& data)
    {
        // append the new bits to those bits leftover from previous call(s)
        buffer.numBits += data.numBits;
        buffer.data   <<= data.numBits;
        buffer.data    |= data.code;

        // write all "full" bytes
        while (buffer.numBits >= 8)
        {
            // extract highest 8 bits
            buffer.numBits -= 8;
            auto oneByte = uint8_t(buffer.data >> buffer.numBits);
            output(oneByte);

            if (oneByte == 0xFF) // 0xFF has a special meaning for JPEGs (it's a block marker)
                output(0);         // therefore pad a zero to indicate "nope, this one ain't a marker, it's just a coincidence"

            // note: I don't clear those written bits, therefore buffer.bits may contain garbage in the high bits
            //       if you really want to "clean up" (e.g. for debugging purposes) then uncomment the following line
            //buffer.bits &= (1 << buffer.numBits) - 1;
        }
        return *this;
    }

    // write all non-yet-written bits, fill gaps with 1s (that's a strange JPEG thing)
    void flush()
    {
        // at most seven set bits needed to "fill" the last byte: 0x7F = binary 0111 1111
        *this << BitCode(0x7F, 7); // I should set buffer.numBits = 0 but since there are no single bits written after flush() I can safely ignore it
    }

    // NOTE: all the following BitWriter functions IGNORE the BitBuffer and write straight to output !
    // write a single byte
    BitWriter& operator<<(uint8_t oneByte)
    {
        output(oneByte);
        return *this;
    }

    // write an array of bytes
    template <typename T, int Size>
    BitWriter& operator<<(T (&manyBytes)[Size])
    {
        for (auto c : manyBytes)
            output(c);
        return *this;
    }

    // start a new JFIF block
    void addMarker(uint8_t id, uint16_t length)
    {
        output(0xFF); output(id);     // ID, always preceded by 0xFF
        output(uint8_t(length >> 8)); // length of the block (big-endian, includes the 2 length bytes as well)
        output(uint8_t(length & 0xFF));
    }
};

template <typename Number, typename Limit>
Number clamp2(Number value, Limit minValue, Limit maxValue)
{
    if (value <= minValue) return minValue; // never smaller than the minimum
    if (value >= maxValue) return maxValue; // never bigger  than the maximum
    return value;                           // value was inside interval, keep it
}

void generateHuffmanTable(const uint8_t numCodes[16], const uint8_t* values, BitCode result[256])
{
    // process all bitsizes 1 thru 16, no JPEG Huffman code is allowed to exceed 16 bits
    auto huffmanCode = 0;
    for (auto numBits = 1; numBits <= 16; numBits++)
    {
        // ... and each code of these bitsizes
        for (auto i = 0; i < numCodes[numBits - 1]; i++) // note: numCodes array starts at zero, but smallest bitsize is 1
            result[*values++] = BitCode(huffmanCode++, numBits);

        // next Huffman code needs to be one bit wider
        huffmanCode <<= 1;
    }
}

int16_t encodeBlock(BitWriter& writer, int index,const float scaled[8*8], int16_t lastDC,
                    const BitCode huffmanDC[256], const BitCode huffmanAC[256], const BitCode* codewords)
{

    auto DC = zigzagcoeffs[index][0];

    auto posNonZero = 0; // find last coefficient which is not zero (because trailing zeros are encoded differently)
    for (auto i = 1; i < 512; i++) // start at 1 because block64[0]=DC was already processed
    {
        if (zigzagcoeffs[index][i] != 0)
            posNonZero = i;
    }

    auto diff = DC - lastDC;

    if (diff == 0)
        writer << huffmanDC[0x00];   // yes, write a special short symbol
    else
    {
        auto bits = codewords[diff]; // nope, encode the difference to previous block's average color
        writer << huffmanDC[bits.numBits] << bits;
    }

    // encode ACs (quantized[1..63])
    auto offset = 0; // upper 4 bits count the number of consecutive zeros
    for (auto i = 1; i <= posNonZero; i++) // quantized[0] was already written, skip all trailing zeros, too
    {
        // zeros are encoded in a special way
        while (zigzagcoeffs[index][i] == 0) // found another zero ?
        {
            offset    += 0x10; // add 1 to the upper 4 bits
            // split into blocks of at most 16 consecutive zeros
            if (offset > 0xF0) // remember, the counter is in the upper 4 bits, 0xF = 15
            {
                writer << huffmanAC[0xF0]; // 0xF0 is a special code for "16 zeros"
                offset = 0;
            }
            i++;
        }

        auto encoded = codewords[zigzagcoeffs[index][i]];
        // combine number of zeros with the number of bits of the next non-zero value
        writer << huffmanAC[offset + encoded.numBits] << encoded; // and the value itself
        offset = 0;
    }

    // send end-of-block code (0x00), only needed if there are trailing zeros
    if (posNonZero < 512 - 1) // = 511
        writer << huffmanAC[0x00];

    return DC;
}

namespace TooJpeg
{
    bool writeJpeg(WRITE_ONE_BYTE output, unsigned short width, unsigned short height,
                   bool isRGB, unsigned char quality_, bool downsample)
    {

        const auto numComponents = isRGB ? 3 : 1;

        BitWriter bitWriter(output);

        // ////////////////////////////////////////
        // JFIF headers
        const uint8_t HeaderJfif[2+2+16] =
                { 0xFF,0xD8,         // SOI marker (start of image)
                  0xFF,0xE0,         // JFIF APP0 tag
                  0,16,              // length: 16 bytes (14 bytes payload + 2 bytes for this length field)
                  'J','F','I','F',0, // JFIF identifier, zero-terminated
                  1,1,               // JFIF version 1.1
                  0,                 // no density units specified
                  0,1,0,1,           // density: 1 pixel "per pixel" horizontally and vertically
                  0,0 };             // no thumbnail (size 0 x 0)
        bitWriter << HeaderJfif;

        // ////////////////////////////////////////
        // adjust quantization tables to desired quality

        // quality level must be in 1 ... 100
        auto quality = clamp2<uint16_t>(quality_, 1, 100);
        // convert to an internal JPEG quality factor, formula taken from libjpeg
        quality = quality < 50 ? 5000 / quality : 200 - quality * 2;

        uint8_t quantLuminance  [8*8];
        uint8_t quantChrominance[8*8];
        for (auto i = 0; i < 8*8; i++)
        {
            int luminance   = (DefaultQuantLuminance  [ZigZagInv[i]] * quality + 50) / 100;
            int chrominance = (DefaultQuantChrominance[ZigZagInv[i]] * quality + 50) / 100;

            // clamp to 1..255
            quantLuminance  [i] = clamp2(luminance,   1, 255);
            quantChrominance[i] = clamp2(chrominance, 1, 255);
        }

        // write quantization tables
        bitWriter.addMarker(0xDB, 2 + (isRGB ? 2 : 1) * (1 + 8*8)); // length: 65 bytes per table + 2 bytes for this length field
        // each table has 64 entries and is preceded by an ID byte

        bitWriter   << 0x00 << quantLuminance;   // first  quantization table
        if (isRGB)
            bitWriter << 0x01 << quantChrominance; // second quantization table, only relevant for color images

        // ////////////////////////////////////////
        // write image infos (SOF0 - start of frame)
        bitWriter.addMarker(0xC0, 2+6+3*numComponents); // length: 6 bytes general info + 3 per channel + 2 bytes for this length field

        // 8 bits per channel
        bitWriter << 0x08
                  // image dimensions (big-endian)
                  << (height >> 8) << (height & 0xFF)
                  << (width  >> 8) << (width  & 0xFF);

        // sampling and quantization tables for each component
        bitWriter << numComponents;       // 1 component (grayscale, Y only) or 3 components (Y,Cb,Cr)
        for (auto id = 1; id <= numComponents; id++)
            bitWriter <<  id                // component ID (Y=1, Cb=2, Cr=3)
                      // bitmasks for sampling: highest 4 bits: horizontal, lowest 4 bits: vertical
                      << (id == 1 && downsample ? 0x22 : 0x11) // 0x11 is default YCbCr 4:4:4 and 0x22 stands for YCbCr 4:2:0
                      << (id == 1 ? 0 : 1); // use quantization table 0 for Y, table 1 for Cb and Cr


        bitWriter.addMarker(0xC4, isRGB ? (2+208+208) : (2+208));

        // store luminance's DC+AC Huffman table definitions
        bitWriter << 0x00 // highest 4 bits: 0 => DC, lowest 4 bits: 0 => Y (baseline)
                  << DcLuminanceCodesPerBitsize
                  << DcLuminanceValues;
        bitWriter << 0x10 // highest 4 bits: 1 => AC, lowest 4 bits: 0 => Y (baseline)
                  << AcLuminanceCodesPerBitsize
                  << AcLuminanceValues;

        // compute actual Huffman code tables (see Jon's code for precalculated tables)
        BitCode huffmanLuminanceDC[256];
        BitCode huffmanLuminanceAC[256];
        generateHuffmanTable(DcLuminanceCodesPerBitsize, DcLuminanceValues, huffmanLuminanceDC);
        generateHuffmanTable(AcLuminanceCodesPerBitsize, AcLuminanceValues, huffmanLuminanceAC);

        // chrominance is only relevant for color images
        BitCode huffmanChrominanceDC[256];
        BitCode huffmanChrominanceAC[256];
        if (isRGB)
        {
            // store luminance's DC+AC Huffman table definitions
            bitWriter << 0x01 // highest 4 bits: 0 => DC, lowest 4 bits: 1 => Cr,Cb (baseline)
                      << DcChrominanceCodesPerBitsize
                      << DcChrominanceValues;
            bitWriter << 0x11 // highest 4 bits: 1 => AC, lowest 4 bits: 1 => Cr,Cb (baseline)
                      << AcChrominanceCodesPerBitsize
                      << AcChrominanceValues;

            // compute actual Huffman code tables (see Jon's code for precalculated tables)
            generateHuffmanTable(DcChrominanceCodesPerBitsize, DcChrominanceValues, huffmanChrominanceDC);
            generateHuffmanTable(AcChrominanceCodesPerBitsize, AcChrominanceValues, huffmanChrominanceAC);
        }

        // ////////////////////////////////////////
        // start of scan (there is only a single scan for baseline JPEGs)
        bitWriter.addMarker(0xDA, 2+1+2*numComponents+3); // 2 bytes for the length field, 1 byte for number of components,
        // then 2 bytes for each component and 3 bytes for spectral selection

        // assign Huffman tables to each component
        bitWriter << numComponents;
        for (auto id = 1; id <= numComponents; id++)
            // highest 4 bits: DC Huffman table, lowest 4 bits: AC Huffman table
            bitWriter << id << (id == 1 ? 0x00 : 0x11); // Y: tables 0 for DC and AC; Cb + Cr: tables 1 for DC and AC

        // constant values for our baseline JPEGs (which have a single sequential scan)
        static const uint8_t Spectral[3] = { 0, 63, 0 }; // spectral selection: must be from 0 to 63; successive approximation must be 0
        bitWriter << Spectral;

        // ////////////////////////////////////////
        // adjust quantization tables with AAN scaling factors to simplify DCT
        float scaledLuminance  [8*8];
        float scaledChrominance[8*8];
        for (auto i = 0; i < 8*8; i++)
        {
            auto row    = ZigZagInv[i] / 8; // same as ZigZagInv[i] >> 3
            auto column = ZigZagInv[i] % 8; // same as ZigZagInv[i] &  7

            // scaling constants for AAN DCT algorithm: AanScaleFactors[0] = 1, AanScaleFactors[k=1..7] = cos(k*PI/16) * sqrt(2)
            static const float AanScaleFactors[8] = { 1, 1.387039845f, 1.306562965f, 1.175875602f, 1, 0.785694958f, 0.541196100f, 0.275899379f };
            auto factor = 1 / (AanScaleFactors[row] * AanScaleFactors[column] * 8);
            scaledLuminance  [ZigZagInv[i]] = factor / quantLuminance  [i];
            scaledChrominance[ZigZagInv[i]] = factor / quantChrominance[i];
            // if you really want JPEGs that are bitwise identical to Jon Olick's code then you need slightly different formulas (note: sqrt(8) = 2.828427125f)
            //static const float aasf[] = { 1.0f * 2.828427125f, 1.387039845f * 2.828427125f, 1.306562965f * 2.828427125f, 1.175875602f * 2.828427125f, 1.0f * 2.828427125f, 0.785694958f * 2.828427125f, 0.541196100f * 2.828427125f, 0.275899379f * 2.828427125f }; // line 240 of jo_jpeg.cpp
            //scaledLuminance  [ZigZagInv[i]] = 1 / (quantLuminance  [i] * aasf[row] * aasf[column]); // lines 266-267 of jo_jpeg.cpp
            //scaledChrominance[ZigZagInv[i]] = 1 / (quantChrominance[i] * aasf[row] * aasf[column]);
        }

        // ////////////////////////////////////////
        // precompute JPEG codewords for quantized DCT
        BitCode  codewordsArray[2 * CodeWordLimit];          // note: quantized[i] is found at codewordsArray[quantized[i] + CodeWordLimit]
        BitCode* codewords = &codewordsArray[CodeWordLimit]; // allow negative indices, so quantized[i] is at codewords[quantized[i]]
        uint8_t numBits = 1; // each codeword has at least one bit (value == 0 is undefined)
        int32_t mask    = 1; // mask is always 2^numBits - 1, initial value 2^1-1 = 2-1 = 1
        for (int16_t value = 1; value < CodeWordLimit; value++)
        {
            // numBits = position of highest set bit (ignoring the sign)
            // mask    = (2^numBits) - 1
            if (value > mask) // one more bit ?
            {
                numBits++;
                mask = (mask << 1) | 1; // append a set bit
            }
            codewords[-value] = BitCode(mask - value, numBits); // note that I use a negative index => codewords[-value] = codewordsArray[CodeWordLimit  value]
            codewords[+value] = BitCode(       value, numBits);
        }


        // the next two variables are frequently used when checking for image borders
        const auto maxWidth  = width  - 1; // "last row"
        const auto maxHeight = height - 1; // "bottom line"

        // process MCUs (minimum codes units) => image is subdivided into a grid of 8x8 or 16x16 tiles
        const auto sampling = downsample ? 2 : 1; // 1x1 or 2x2 sampling
        const auto mcuSize  = 8 * sampling;


        // average color of the previous MCU
        int16_t lastYDC = 0, lastCbDC = 0, lastCrDC = 0;
        // convert from RGB to YCbCr
        float Y[8][8], Cb[8][8], Cr[8][8];
/*
        for (auto mcuY = 0; mcuY < height; mcuY += mcuSize) // each step is either 8 or 16 (=mcuSize)
            for (auto mcuX = 0; mcuX < width; mcuX += mcuSize)
            {
                for (auto blockY = 0; blockY < mcuSize; blockY += 8)
                    for (auto blockX = 0; blockX < mcuSize; blockX += 8)
                    {
                        lastYDC = encodeBlock(bitWriter, scaledLuminance, lastYDC, huffmanLuminanceDC, huffmanLuminanceAC, codewords);
                    }
                lastCbDC = encodeBlock(bitWriter, scaledChrominance, lastCbDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
                lastCrDC = encodeBlock(bitWriter, scaledChrominance, lastCrDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
            }
*/
        for(int i = 0; i < (number_of_coeffs/(64*6)); i++)
        {
            for(int j = 0; j < 4; j++)
            {
                lastYDC = encodeBlock(bitWriter, (i*6)+j,scaledLuminance, lastYDC, huffmanLuminanceDC, huffmanLuminanceAC, codewords);
            }
            lastCbDC = encodeBlock(bitWriter, (i*6)+4,scaledChrominance, lastCbDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
            lastCrDC = encodeBlock(bitWriter, (i*6)+5,scaledChrominance, lastCrDC, huffmanChrominanceDC, huffmanChrominanceAC, codewords);
        }

        bitWriter.flush(); // now image is completely encoded, write any bits still left in the buffer

        // ///////////////////////////
        // EOI marker
        bitWriter << 0xFF << 0xD9; // this marker has no length, therefore I can't use addMarker()
        return true;
    }
}

//mypart:

void DCT(float block[8], uint8_t stride = 1) // stride must be 1 (=horizontal) or 8 (=vertical)
{
    const auto SqrtHalfSqrt = 1.306562965f; //    sqrt((2 + sqrt(2)) / 2) = cos(pi * 1 / 8) * sqrt(2)
    const auto InvSqrt      = 0.707106781f; // 1 / sqrt(2)                = cos(pi * 2 / 8)
    const auto HalfSqrtSqrt = 0.382683432f; //     sqrt(2 - sqrt(2)) / 2  = cos(pi * 3 / 8)
    const auto InvSqrtSqrt  = 0.541196100f; // 1 / sqrt(2 - sqrt(2))      = cos(pi * 3 / 8) * sqrt(2)

    // modify in-place
    auto& block0 = block[0         ];
    auto& block1 = block[1 * stride];
    auto& block2 = block[2 * stride];
    auto& block3 = block[3 * stride];
    auto& block4 = block[4 * stride];
    auto& block5 = block[5 * stride];
    auto& block6 = block[6 * stride];
    auto& block7 = block[7 * stride];

    // based on https://dev.w3.org/Amaya/libjpeg/jfdctflt.c , the original variable names can be found in my comments
    auto add07 = block0 + block7; auto sub07 = block0 - block7; // tmp0, tmp7
    auto add16 = block1 + block6; auto sub16 = block1 - block6; // tmp1, tmp6
    auto add25 = block2 + block5; auto sub25 = block2 - block5; // tmp2, tmp5
    auto add34 = block3 + block4; auto sub34 = block3 - block4; // tmp3, tmp4

    auto add0347 = add07 + add34; auto sub07_34 = add07 - add34; // tmp10, tmp13 ("even part" / "phase 2")
    auto add1256 = add16 + add25; auto sub16_25 = add16 - add25; // tmp11, tmp12

    block0 = add0347 + add1256; block4 = add0347 - add1256; // "phase 3"

    auto z1 = (sub16_25 + sub07_34) * InvSqrt; // all temporary z-variables kept their original names
    block2 = sub07_34 + z1; block6 = sub07_34 - z1; // "phase 5"

    auto sub23_45 = sub25 + sub34; // tmp10 ("odd part" / "phase 2")
    auto sub12_56 = sub16 + sub25; // tmp11
    auto sub01_67 = sub16 + sub07; // tmp12

    auto z5 = (sub23_45 - sub01_67) * HalfSqrtSqrt;
    auto z2 = sub23_45 * InvSqrtSqrt  + z5;
    auto z3 = sub12_56 * InvSqrt;
    auto z4 = sub01_67 * SqrtHalfSqrt + z5;
    auto z6 = sub07 + z3; // z11 ("phase 5")
    auto z7 = sub07 - z3; // z13
    block1 = z6 + z4; block7 = z6 - z4; // "phase 6"
    block5 = z7 + z2; block3 = z7 - z2;
}

void IDCT(int16_t block[8])
{
    int16_t* pSrc = block;

    if ((pSrc[1] | pSrc[2] | pSrc[3] | pSrc[4] | pSrc[5] | pSrc[6] | pSrc[7]) == 0)
    {
        // Short circuit the 1D IDCT if only the DC component is non-zero
        int16_t src0 = *pSrc;

        *(pSrc+1) = src0;
        *(pSrc+2) = src0;
        *(pSrc+3) = src0;
        *(pSrc+4) = src0;
        *(pSrc+5) = src0;
        *(pSrc+6) = src0;
        *(pSrc+7) = src0;
    }
    else
    {
        int16_t src4 = *(pSrc+5);
        int16_t src7 = *(pSrc+3);
        int16_t x4  = src4 - src7;
        int16_t x7  = src4 + src7;

        int16_t src5 = *(pSrc+1);
        int16_t src6 = *(pSrc+7);
        int16_t x5  = src5 + src6;
        int16_t x6  = src5 - src6;

        int16_t tmp1 = imul_b5(x4 - x6);
        int16_t stg26 = imul_b4(x6) - tmp1;

        int16_t x24 = tmp1 - imul_b2(x4);

        int16_t x15 = x5 - x7;
        int16_t x17 = x5 + x7;

        int16_t tmp2 = stg26 - x17;
        int16_t tmp3 = imul_b1_b3(x15) - tmp2;
        int16_t x44 = tmp3 + x24;

        int16_t src0 = *(pSrc+0);
        int16_t src1 = *(pSrc+4);
        int16_t x30 = src0 + src1;
        int16_t x31 = src0 - src1;

        int16_t src2 = *(pSrc+2);
        int16_t src3 = *(pSrc+6);
        int16_t x12 = src2 - src3;
        int16_t x13 = src2 + src3;

        int16_t x32 = imul_b1_b3(x12) - x13;

        int16_t x40 = x30 + x13;
        int16_t x43 = x30 - x13;
        int16_t x41 = x31 + x32;
        int16_t x42 = x31 - x32;

        *(pSrc+0) = x40 + x17;
        *(pSrc+1) = x41 + tmp2;
        *(pSrc+2) = x42 + tmp3;
        *(pSrc+3) = x43 - x44;
        *(pSrc+4) = x43 + x44;
        *(pSrc+5) = x42 - tmp3;
        *(pSrc+6) = x41 - tmp2;
        *(pSrc+7) = x40 - x17;
    }
}

void DCT_function()
{
    for(int i = 0; i < number_of_coeffs; i++)
    {
        float block[8];

        for(int j = 0; j < 8; j++)
        {
            block[j] = (float)coeffs[j][i];
        }
        DCT(block);
        for(int j = 0; j < 8; j++)
        {
            coeffs[j][i] = nearbyint(block[j]/1.0);
        }
    }
}

void IDCT_function()
{
    for(int i = 0; i < number_of_coeffs; i++)
    {
        int16_t block[8];

        for(int j = 0; j < 8; j++)
        {
            block[j] = coeffs[j][i];
        }
        IDCT(block);
        for(int j = 0; j < 8; j++)
        {
            coeffs[j][i] = nearbyint(block[j]/8.0*1.0);
        }
    }
}

void zizgzag_function()
{
    for(int i = 0; i < (number_of_coeffs/64); i++)
    {
        int16_t matrix[8][8][8];

        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 8; y++)
            {
                for(int z = 0; z < 8; z++)
                {
                    matrix[x][y][z] = coeffs[x][(y*8)+z+(i*64)];
                }
            }
        }
        for(int j = 0; j < 512; j++)
        {
            zigzagcoeffs[i][j] = matrix[W[j]-1][X[j]-1][Z[j]-1];
        }
    }
}

void izigzag_function()
{
    for(int i = 0; i < (number_of_coeffs/64); i++)
    {
        int16_t matrix[8][8][8];

        for(int j = 0; j < 512; j++)
        {
            matrix[W[j]-1][X[j]-1][Z[j]-1] = zigzagcoeffs[i][j];
        }
        for(int x = 0; x < 8; x++)
        {
            for(int y = 0; y < 8; y++)
            {
                for(int z = 0; z < 8; z++)
                {
                    coeffs[x][(y*8)+z+(i*64)] = matrix[x][y][z];
                }
            }
        }
    }
}

int16_t RLC_encode_block(int16_t lastDC, int nr_block, FILE * file)
{
    int16_t buf[1];
    int16_t DC = zigzagcoeffs[nr_block][0];
    buf[0] = DC - lastDC;

    fwrite(buf, sizeof(int16_t), 1, file);

    int16_t zeros = 0;

    for(int i = 1; i < 512; i++)
    {
        if(zigzagcoeffs[nr_block][i] != 0)
        {
            buf[0] = zeros;
            fwrite(buf, sizeof(int16_t), 1, file);
            buf[0] = zigzagcoeffs[nr_block][i];
            fwrite(buf, sizeof(int16_t), 1, file);
            zeros = 0;
        }
        else
        {
            zeros++;
        }
    }
    if(zeros > 0)
    {
        buf[0] = -69;
        fwrite(buf, sizeof(int16_t), 1, file);
       // fwrite(buf, sizeof(int16_t), 1, file);
    }

    return DC;
}

void RLC_encode(FILE * file)
{
    int16_t lastDC = 0;
    for(int i = 0; i < (number_of_coeffs/64); i++)
    {
        lastDC = RLC_encode_block(lastDC, i, file);
    }
}

void RLC_decode(FILE * file)
{
    int16_t lastDC = 0;
    int16_t buf[1];

    //for(int i = 0; i < (number_of_coeffs/64); i++)
    for(int i = 0; i < 1; i++)
    {
        int l = 0;
        fread(buf, sizeof(int16_t), 1, file);
        zigzagcoeffs[i][l] = buf[0] + lastDC;
        lastDC = buf[0];
        l++;

        while(true)
        {
            if(l == 512)
                break;

            fread(buf, sizeof(int16_t), 1, file);

           // if(buf[0] == 0 and buf[1] == 0)
            if(buf[0] == -69)
            {
                for(int j = l; j < 512; j++)
                {
                    zigzagcoeffs[i][j] = 0;
                }
                break;
            }
            else
            {
                for(int j = 0; j < buf[0]; j++)
                {
                    zigzagcoeffs[i][l] = 0;
                    l++;
                }
                fread(buf, sizeof(int16_t), 1, file);
                zigzagcoeffs[i][l] = buf[0];
                l++;
            }
        }
    }
}

int main() {

    int n = 1;
    const char *pSrc_filename;
    const char *pDst_filename;
    int width, height, comps;
    pjpeg_scan_type_t scan_type;
    uint8_t *pImage;
    int reduce = 0;

    for(int i = 0; i < 8; i++)
    {
        counter = 0;
        string x = "klatki/frame"+to_string(i)+".jpg";
        pSrc_filename = x.c_str();

        pImage = pjpeg_load_from_file(pSrc_filename, &width, &height, &comps, &scan_type, reduce, i, 0, 0);
        if (!pImage) {
            printf("Failed loading source image!\n");
            return EXIT_FAILURE;
        }
    }

    DCT_function();
    zizgzag_function();
/*
    FILE * file = fopen("coeffs", "wb");
    RLC_encode(file);
    fclose(file);

*/

    myFile.open("block.jpg", std::ios_base::out | std::ios_base::binary);
    uint8_t quality = 100;
    const auto bytesPerPixel = 3;
    const bool isRGB = true;
    const bool downsample = true;

    auto ok = TooJpeg::writeJpeg(myOutput, width, height, isRGB, quality, downsample);

    counter = 0;
    pImage = pjpeg_load_from_file("block.jpg", &width, &height, &comps, &scan_type, reduce, 0, 0, 1);

/*
    file = fopen("coeffs", "rb");
    RLC_decode(file);
    fclose(file);
*/

    izigzag_function();
    IDCT_function();

    for(int i = 0; i < 8; i++) {
        counter = 0;
        string x = "klatki/frame"+to_string(i)+".jpg";
        pSrc_filename = x.c_str();

        pImage = pjpeg_load_from_file(pSrc_filename, &width, &height, &comps, &scan_type, reduce, i, 1, 0);
        if (!pImage) {
            printf("Failed loading source image!\n");
            return EXIT_FAILURE;
        }

        string y = "klatki/decoded"+to_string(i)+".bmp";
        pDst_filename = y.c_str();

        if (!stbi_write_bmp(pDst_filename, width, height, comps, pImage))
        {
            printf("Failed writing image to destination file!\n");
            return EXIT_FAILURE;
        }
    }

    return 0;
}