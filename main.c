#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#define MIN(a, b) ((a)<(b)?(a):(b))

typedef struct {
    uint16_t bfType; /**< BMP signature (usually 0x424D in big-endian or 0x4D42 in little-endian which means BM). */
    uint32_t bfSize; /**< BMP file size in bytes. */
    uint16_t bfReserved1, bfReserved2; /**< Reserved fields. Usually should be 0. */
    uint32_t bfOffBits; /**< Bitmap data offset in bytes. */
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize; /**< Size of header in bytes (should be 40). */
    int32_t biWidth; /**< Width of bitmap. */
    int32_t biHeight; /**< Height of bitmap. */
    uint16_t biPlanes; /**< Color planes. The value of this field must be 1 in BMP files. */
    uint16_t biBitCount; /**< Bits-per-pixel count. Allowed values are 1, 2, 4, 8, 16, 24, 32, 48, 64. */
    uint32_t biCompression; /**< Compression used for bitmap data. Allowed values are stored in BITMAP_COMPRESSION enum. */
    uint32_t biSizeImage; /**< Size of bitmap data. Can be 0 if BITMAP_COMPRESSION is 0, 3 or 6. */
    int32_t biXPelsPerMeter; /**< Pixels-per-meter (horizontal). */
    int32_t biYPelsPerMeter; /**< Pixels-per-meter (vertical). */
    uint32_t biClrUsed; /**< Number of colors used in palette. */
    uint32_t biClrImportant; /**< Number of colors used in bitmap data (from the start of palette). */
} BITMAPINFOHEADER;




typedef struct {
    uint8_t b; /**< A blue channel. */
    uint8_t g; /**< A green channel. */
    uint8_t r; /**< A red channel. */
    uint8_t reserved; /**< A reserved channel (sometimes it's used for alpha). */
} RGB_COLOR;

typedef union {
    RGB_COLOR rgb; /**< A BGRA representation of color. */
    uint32_t value; /**< An integer representation of color. */
} BITMAP_COLOR;

typedef struct {
    uint32_t width; /**< Bitmap width. */
    uint32_t height; /**< Bitmap height. */
    BITMAP_COLOR *data; /**< Raw pixel data. */
} BITMAP;



uint32_t ReadUint32(FILE *fileS){
    uint32_t valUint32;
    fread(&valUint32, sizeof(uint32_t), 1, fileS);
    return valUint32;
}

uint64_t ReadUint64(FILE *fileS){
    uint64_t valUint64;
    fread(&valUint64, sizeof(uint64_t), 1, fileS);
    return valUint64;
}

uint16_t ReadUint16(FILE *fileS){
    uint16_t valUint16;
    fread(&valUint16, sizeof(uint16_t), 1, fileS);
    return valUint16;
}

uint8_t ReadUint8(FILE *fileS){
    uint8_t valUint8;
    fread(&valUint8, sizeof(uint8_t), 1, fileS);
    return valUint8;
}

void WriteUint32(FILE *fileS, uint32_t value){    
    fwrite(&value, sizeof(uint32_t), 1, fileS);    
}

void WriteUint64(FILE *fileS, uint64_t value){    
    fwrite(&value, sizeof(uint64_t), 1, fileS);    
}

void WriteUint16(FILE *fileS, uint16_t value){    
    fwrite(&value, sizeof(uint16_t), 1, fileS);   
}

void WriteUint8(FILE *fileS, uint8_t value){
    fwrite(&value, sizeof(uint8_t), 1, fileS);    
}

uint8_t bmp_channel_blend(uint8_t a, uint8_t b, uint8_t alpha) {
    return (uint8_t) round(MIN(a, b) + abs(a - b) * (1.0 * alpha / 255.0));
}

BITMAP_COLOR bmp_color_blend(BITMAP_COLOR a, BITMAP_COLOR b, uint8_t alpha) {
    BITMAP_COLOR dst;
    dst.rgb.r = bmp_channel_blend(a.rgb.r, b.rgb.r, alpha);
    dst.rgb.g = bmp_channel_blend(a.rgb.g, b.rgb.g, alpha);
    dst.rgb.b = bmp_channel_blend(a.rgb.b, b.rgb.b, alpha);
    return dst;
}

BITMAP *bmp_combine(BITMAP *background, BITMAP *foreground, uint8_t alpha) {
    if (background == NULL || foreground == NULL) {
        return NULL;
    }
    if (background->width != foreground->width || background->height != foreground->height) {
        return NULL;
    }
    BITMAP *result = calloc(1, sizeof(BITMAP));
    result->width = background->width;
    result->height = background->height;
    result->data = calloc(result->width * result->height, sizeof(BITMAP_COLOR));
    for (int32_t y = 0; y < background->height; ++y) {
        for (int32_t x = 0; x < background->width; ++x) {
            BITMAP_COLOR a = background->data[y * result->width + x];
            BITMAP_COLOR b = foreground->data[y * result->width + x];
            result->data[y * result->width + x] = bmp_color_blend(a, b, alpha);
        }
    }
    return result;
}


BITMAP *bmp_read(const char *file) {

    FILE *bmp_file;
    BITMAP *bitmap;

    BITMAPFILEHEADER file_header;
    uint32_t bitmapinfo_size;
    BITMAPINFOHEADER *bitmapinfo_v3 = NULL;


    if ((bmp_file = fopen(file, "rb")) == NULL) {
        return NULL;
    }

    file_header.bfType = ReadUint16(bmp_file);
    if (file_header.bfType != 0x4D42) {
        // BM support onl
        fclose(bmp_file);
        return NULL;
    }
    file_header.bfSize = ReadUint32(bmp_file);
    file_header.bfReserved1 = ReadUint16(bmp_file);
    file_header.bfReserved2 = ReadUint16(bmp_file);
    file_header.bfOffBits = ReadUint32(bmp_file);

    // Reading BITMAPINFO's size to detect its own version
    bitmapinfo_size = ReadUint32(bmp_file);
    if ((bitmap = calloc(1, sizeof(BITMAP))) == NULL) {
        // Memory hasn't been allocated
        fclose(bmp_file);
        return NULL;
    }

    //Reading

    if ((bitmapinfo_v3 = calloc(1, sizeof(BITMAPINFOHEADER))) == NULL) {
        free(bitmap);
        fclose(bmp_file);
        return NULL;
    }
    bitmapinfo_v3->biSize = bitmapinfo_size;
    bitmapinfo_v3->biWidth =  ReadUint32(bmp_file);
    bitmapinfo_v3->biHeight = ReadUint32(bmp_file);
    bitmapinfo_v3->biPlanes = ReadUint16(bmp_file);
    bitmapinfo_v3->biBitCount = ReadUint16(bmp_file);
    bitmapinfo_v3->biCompression = ReadUint32(bmp_file);
    bitmapinfo_v3->biSizeImage = ReadUint32(bmp_file);
    bitmapinfo_v3->biXPelsPerMeter = ReadUint32(bmp_file);
    bitmapinfo_v3->biYPelsPerMeter = ReadUint32(bmp_file);
    bitmapinfo_v3->biClrUsed = ReadUint32(bmp_file);
    bitmapinfo_v3->biClrImportant = ReadUint32(bmp_file);


    bitmap->width = bitmapinfo_v3->biWidth;
    bitmap->height = bitmapinfo_v3->biHeight;

    free(bitmapinfo_v3);

    fseek(bmp_file, file_header.bfOffBits, SEEK_SET);

    if ((bitmap->data = calloc(bitmap->width * bitmap->height, sizeof(BITMAP_COLOR))) == NULL) {
        free(bitmap);
        fclose(bmp_file);
        return NULL;
    }

    // Reading "scans" as they are, bottom-to-top, left-to-right
    // TODO Microsoft says that in case of negative height the data are being read top-to-bottom
    for (int32_t y = bitmap->height - 1; y >= 0; --y) {
        uint32_t bytes_read = 0;
        for (int32_t x = 0; x < bitmap->width; ++x) {
            uint8_t pixels;
            BITMAP_COLOR color;
            color.rgb.b = ReadUint8(bmp_file);
            color.rgb.g = ReadUint8(bmp_file);
            color.rgb.r = ReadUint8(bmp_file);
            bytes_read += 3;
            bitmap->data[y * bitmap->width + x] = color;
        }
        // Skipping "scan" padding.
        // BMP's image row size must be divisible by 4 so usually there are a few odd bytes.
        while (bytes_read % 4) {
            ReadUint8(bmp_file);
            ++bytes_read;
        }
    }
    fclose(bmp_file);
    return bitmap;
}

int bmp_write(BITMAP *bitmap, const char *file) {
    // Save to 24-bit BMP file with v3 header.
    if (bitmap == NULL) {
        return 1; //TODO error codes
    }
    FILE *bmp_file;
    if ((bmp_file = fopen(file, "wb")) == NULL) {
        return 2; //TODO error codes
    };
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    uint32_t bitmapdata_size = ((bitmap->width % 4) ?
                                (bitmap->width + (4 - (bitmap->width % 4))) :
                                (bitmap->width))
                               * bitmap->height * 3;


    file_header.bfType = 0x4D42;
    file_header.bfSize = 14 + 40 + bitmapdata_size;
    file_header.bfReserved1 = 0;
    file_header.bfReserved2 = 0;
    file_header.bfOffBits = 14 + 40;

    info_header.biSize = 40;
    info_header.biWidth = bitmap->width;
    info_header.biHeight = bitmap->height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = 0;
    info_header.biSizeImage = 0;
    info_header.biXPelsPerMeter = 0;
    info_header.biYPelsPerMeter = 0;
    info_header.biClrUsed = 0;
    info_header.biClrImportant = 0;

    WriteUint16(bmp_file, file_header.bfType);
    WriteUint32(bmp_file, file_header.bfSize);
    WriteUint16(bmp_file, file_header.bfReserved1);
    WriteUint16(bmp_file, file_header.bfReserved2);
    WriteUint32(bmp_file, file_header.bfOffBits);

    WriteUint32(bmp_file, info_header.biSize);
    WriteUint32(bmp_file, info_header.biWidth);
    WriteUint32(bmp_file, info_header.biHeight);
    WriteUint16(bmp_file, info_header.biPlanes);
    WriteUint16(bmp_file, info_header.biBitCount);
    WriteUint32(bmp_file, info_header.biCompression);
    WriteUint32(bmp_file, info_header.biSizeImage);
    WriteUint32(bmp_file, info_header.biXPelsPerMeter);
    WriteUint32(bmp_file, info_header.biYPelsPerMeter);
    WriteUint32(bmp_file, info_header.biClrUsed);
    WriteUint32(bmp_file, info_header.biClrImportant);


    for (int32_t y = bitmap->height - 1; y >= 0; --y) {
        uint32_t bytes_written = 0;
        for (int32_t x = 0; x < bitmap->width; ++x) {
            WriteUint8(bmp_file, bitmap->data[y * bitmap->width + x].rgb.b);
            WriteUint8(bmp_file, bitmap->data[y * bitmap->width + x].rgb.g);
            WriteUint8(bmp_file, bitmap->data[y * bitmap->width + x].rgb.r);
            bytes_written += 3;
        }
        // Skipping "scan" padding.
        // BMP's image row size must be divisible by 4 so usually there are a few odd bytes.
        while (bytes_written % 4) {
            WriteUint8(bmp_file, 0);
            ++bytes_written;
        }
    }
    fclose(bmp_file);
    return 0;
}




int main(int argc, char **argv) {

    BITMAP *BMP1;
    BITMAP *BMP2;
    BITMAP *blend;
    float temp;
    int ops;
    printf("coefficient of opacity from 0 to 1:  ");
    scanf("%f", &temp);
    temp *= 255;
    ops = (int)temp;

    BMP1 = bmp_read("bmp1.bmp");
    BMP2 = bmp_read("bmp2.bmp");

    blend = bmp_combine(BMP1, BMP2, ops);


    bmp_write(blend, "output.bmp");



    return 0;
}
