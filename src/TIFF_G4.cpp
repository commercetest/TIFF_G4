//
// TIFF G4 decoder
// A TIFF / 1-bpp image library
// written by Larry Bank
// bitbank@pobox.com
// Arduino port started 8/10/2020
// Original code written 20 years ago :)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifdef __cplusplus

#include "TIFF_G4.h"
// forward references
static int TIFFInit(TIFFIMAGE *pTIFF);
static int TIFFParseInfo(TIFFIMAGE *pPage);
static void TIFFGetMoreData(TIFFIMAGE *pPage);
static int Decode(TIFFIMAGE *pImage);

#include "tiffg4.c"
//
// Memory initialization
// Open a TIFF file and parse the header
//
int TIFFG4::openTIFF(uint8_t *pData, int iDataSize, TIFF_DRAW_CALLBACK *pfnDraw)
{
    memset(&_tiff, 0, sizeof(TIFFIMAGE));
    _tiff.pfnRead = readMem;
    _tiff.pfnSeek = seekMem;
    _tiff.pfnDraw = pfnDraw;
    _tiff.pfnOpen = NULL;
    _tiff.pfnClose = NULL;
    _tiff.TIFFFile.iSize = iDataSize;
    _tiff.TIFFFile.pData = pData;
    return TIFFInit(&_tiff);
} /* openTIFF() */
//
// Work with 'headerless' compressed G4 data
// Pass the width, height and fill order
//
int TIFFG4::openRAW(int iWidth, int iHeight, int iFillOrder, uint8_t *pData, int iDataSize, TIFF_DRAW_CALLBACK *pfnDraw)
{
    memset(&_tiff, 0, sizeof(TIFFIMAGE));
    _tiff.pfnRead = readMem;
    _tiff.pfnSeek = seekMem;
    _tiff.pfnDraw = pfnDraw;
    _tiff.pfnOpen = NULL;
    _tiff.pfnClose = NULL;
    _tiff.TIFFFile.iSize = iDataSize;
    _tiff.TIFFFile.pData = pData;
    _tiff.iWidth = iWidth;
    _tiff.iHeight = iHeight;
    _tiff.ucFillOrder = (uint8_t)iFillOrder;
    // Default output values
    _tiff.window.iScale = 65536; // 1.0 scale
    _tiff.window.x = 0;
    _tiff.window.y = 0; // upper left corner of interest (source pixels)
    _tiff.window.iWidth = iWidth; // dest window size
    _tiff.window.iHeight = iHeight;
    _tiff.window.ucPixelType = TIFF_PIXEL_1BPP;
    return 1;
} /* openRAW() */

void TIFFG4::decodeIncBegin(int iWidth, int iHeight, uint8_t ucFillOrder, TIFF_DRAW_CALLBACK *pfnDraw)
{
    Decode_Inc_Begin(&_tiff, iWidth, iHeight, ucFillOrder, pfnDraw);
} /* decodeIncBegin() */

int TIFFG4::decodeInc(int bHasMoreData)
{
    return Decode_Inc(&_tiff, bHasMoreData);
} /* decodeInc() */

int TIFFG4::addData(uint8_t *pData, int iLen)
{
   return Add_Data(&_tiff, pData, iLen);
} /* addData() */

int TIFFG4::getLastError()
{
    return _tiff.iError;
} /* getLastError() */

int TIFFG4::getWidth()
{
    return _tiff.iWidth;
} /* getWidth() */

int TIFFG4::getHeight()
{
    return _tiff.iHeight;
} /* getHeight() */

//
// File (SD/MMC) based initialization
//
int TIFFG4::openTIFF(const char *szFilename, TIFF_OPEN_CALLBACK *pfnOpen, TIFF_CLOSE_CALLBACK *pfnClose, TIFF_READ_CALLBACK *pfnRead, TIFF_SEEK_CALLBACK *pfnSeek, TIFF_DRAW_CALLBACK *pfnDraw)
{
    memset(&_tiff, 0, sizeof(TIFFIMAGE));
    _tiff.pfnRead = pfnRead;
    _tiff.pfnSeek = pfnSeek;
    _tiff.pfnDraw = pfnDraw;
    _tiff.pfnOpen = pfnOpen;
    _tiff.pfnClose = pfnClose;
    _tiff.TIFFFile.fHandle = (*pfnOpen)(szFilename, &_tiff.TIFFFile.iSize);
    if (_tiff.TIFFFile.fHandle == NULL)
       return 0;
    return TIFFInit(&_tiff);

} /* openTIFF() */

void TIFFG4::close()
{
    if (_tiff.pfnClose)
        (*_tiff.pfnClose)(_tiff.TIFFFile.fHandle);
} /* close() */

int TIFFG4::drawIcon(float scale, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, uint16_t usFGColor, uint16_t usBGColor)
{
    _tiff.window.iScale = (uint32_t)(scale * 65536.0f); // convert to uint32
    _tiff.window.x = iSrcX;
    _tiff.window.y = iSrcY; // upper left corner of interest (source pixels)
    _tiff.window.iWidth = iSrcWidth; // width of destination window (for clipping purposes)
    _tiff.window.iHeight = iSrcHeight;
    _tiff.window.dstx = iDestX;
    _tiff.window.dsty = iDestY;
    _tiff.window.ucPixelType = TIFF_PIXEL_16BPP;
    _tiff.usFG = usFGColor;
    _tiff.usBG = usBGColor;
    return Decode(&_tiff);
} /* drawIcon() */

//
// set draw callback user pointer variable
//
void TIFFG4::setUserPointer(void *p)
{
    _tiff.pUser = p;
}

void TIFFG4::setDrawParameters(float scale, int iPixelType, int iStartX, int iStartY, int iWidth, int iHeight, uint8_t *p4BPPBuf)
{
    _tiff.window.iScale = (uint32_t)(scale * 65536.0f); // convert to uint32
    _tiff.window.x = iStartX;
    _tiff.window.y = iStartY; // upper left corner of interest (source pixels)
    _tiff.window.iWidth = iWidth; // width of destination window (for clipping purposes)
    _tiff.window.iHeight = iHeight;
    _tiff.window.p4BPP = p4BPPBuf; // user-supplied buffer for 4-bpp grayscale output
    _tiff.window.ucPixelType = (uint8_t)iPixelType;
} /* setDrawParameters() */
//
// Decode an image
// returns:
// 1 = good result
// 0 = error
//
int TIFFG4::decode(int iDestX, int iDestY)
{
    _tiff.window.dstx = iDestX;
    _tiff.window.dsty = iDestY;
    return Decode(&_tiff);
} /* decode() */
#endif // __cplusplus
