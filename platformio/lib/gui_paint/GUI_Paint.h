#ifndef __GUI_PAINT_H__
#define __GUI_PAINT_H__

#include <stdio.h>
#include <stdint.h>

#include "fonts.h"

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UBYTE *Image;
    UWORD Width;
    UWORD Height;
    UWORD WidthMemory;
    UWORD HeightMemory;
    UWORD Color;
    UWORD Rotate;
    UWORD Mirror;
    UWORD WidthByte;
    UWORD HeightByte;
    UWORD Scale;
} PAINT;
extern PAINT Paint;

#define ROTATE_0            0
#define ROTATE_90           90
#define ROTATE_180          180
#define ROTATE_270          270

typedef enum {
    MIRROR_NONE  = 0x00,
    MIRROR_HORIZONTAL = 0x01,
    MIRROR_VERTICAL = 0x02,
    MIRROR_ORIGIN = 0x03,
} MIRROR_IMAGE;
#define MIRROR_IMAGE_DFT MIRROR_NONE

#define WHITE          0xFFFF
#define BLACK          0x0000
#define BLUE           0x001F
#define BRED           0XF81F
#define GRED           0XFFE0
#define GBLUE          0X07FF
#define RED            0xF800
#define MAGENTA        0xF81F
#define GREEN          0x07E0
#define CYAN           0x7FFF
#define YELLOW         0xFFE0
#define BROWN          0XBC40
#define BRRED          0XFC07
#define GRAY           0X8430

#define IMAGE_BACKGROUND    WHITE
#define FONT_FOREGROUND     BLACK
#define FONT_BACKGROUND     WHITE

typedef enum {
    DOT_PIXEL_1X1  = 1,
    DOT_PIXEL_2X2  ,
    DOT_PIXEL_3X3  ,
    DOT_PIXEL_4X4  ,
    DOT_PIXEL_5X5  ,
    DOT_PIXEL_6X6  ,
    DOT_PIXEL_7X7  ,
    DOT_PIXEL_8X8  ,
} DOT_PIXEL;
#define DOT_PIXEL_DFT  DOT_PIXEL_1X1

typedef enum {
    DOT_FILL_AROUND  = 1,
    DOT_FILL_RIGHTUP  ,
} DOT_STYLE;
#define DOT_STYLE_DFT  DOT_FILL_AROUND

typedef enum {
    LINE_STYLE_SOLID = 0,
    LINE_STYLE_DOTTED,
} LINE_STYLE;

typedef enum {
    DRAW_FILL_EMPTY = 0,
    DRAW_FILL_FULL,
} DRAW_FILL;

typedef struct {
    UWORD	Year;
    UBYTE Month;
    UBYTE Day;
    UBYTE Hour;
    UBYTE Min;
    UBYTE Sec;
} PAINT_TIME;
extern PAINT_TIME sPaint_time;

void Paint_NewImage(UBYTE *image, UWORD Width, UWORD Height, UWORD Rotate, UWORD Color);
void Paint_SelectImage(UBYTE *image);
void Paint_SetRotate(UWORD Rotate);
void Paint_SetMirroring(UBYTE mirror);
void Paint_SetPixel(UWORD Xpoint, UWORD Ypoint, UWORD Color);
void Paint_SetScale(UBYTE scale);

void Paint_Clear(UWORD Color);
void Paint_ClearWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color);

void Paint_DrawPoint(UWORD Xpoint, UWORD Ypoint, UWORD Color, DOT_PIXEL Dot_Pixel, DOT_STYLE Dot_FillWay);
void Paint_DrawLine(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color, DOT_PIXEL Line_width, LINE_STYLE Line_Style);
void Paint_DrawRectangle(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill);
void Paint_DrawCircle(UWORD X_Center, UWORD Y_Center, UWORD Radius, UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill);

void Paint_DrawChar(UWORD Xstart, UWORD Ystart, const char Acsii_Char, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawString_EN(UWORD Xstart, UWORD Ystart, const char * pString, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawString_CN(UWORD Xstart, UWORD Ystart, const char * pString, cFONT* font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawNum(UWORD Xpoint, UWORD Ypoint, double Nummber, sFONT* Font, UWORD Digit, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawTime(UWORD Xstart, UWORD Ystart, PAINT_TIME *pTime, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);

void Paint_DrawBitMap(const unsigned char* image_buffer);
void Paint_DrawImage(const unsigned char *image, UWORD xStart, UWORD yStart, UWORD W_Image, UWORD H_Image);

#ifdef __cplusplus
}
#endif

#endif
