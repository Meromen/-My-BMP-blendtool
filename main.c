#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#define MIN(a, b) ((a)<(b)?(a):(b))

typedef struct {
    uint16_t bfType; /**< BMP signature (usually 0x424D in big-endian or 0x4D42 in little-endian which means BM). */
    uint32_t bfSize; /**< BMP file size in bytes. */
    uint32_t bfOffBits; /**< Bitmap data offset in bytes. */
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize; /**< Size of header in bytes (should be 40). */
    int32_t biWidth; /**< Width of bitmap. */
    int32_t biHeight; /**< Height of bitmap. */
    uint16_t biBitCount; /**< Bits-per-pixel count. Allowed values are 1, 2, 4, 8, 16, 24, 32, 48, 64. */
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
        fclose(bmp_file);
        return NULL;
    }
    file_header.bfSize = ReadUint32(bmp_file);
    fseek(bmp_file, 4, SEEK_CUR);
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
    fseek(bmp_file, 2, SEEK_CUR);
    bitmapinfo_v3->biBitCount = ReadUint16(bmp_file);
    fseek(bmp_file, 24, SEEK_CUR);
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

    info_header.biSize = 40;
    info_header.biWidth = bitmap->width;
    info_header.biHeight = bitmap->height;
    info_header.biBitCount = 24;

    WriteUint16(bmp_file, file_header.bfType);
    WriteUint32(bmp_file, file_header.bfSize);
    fseek(bmp_file, 8 , SEEK_CUR);
    WriteUint32(bmp_file, info_header.biSize);
    WriteUint32(bmp_file, info_header.biWidth);
    WriteUint32(bmp_file, info_header.biHeight);
    fseek(bmp_file, 2 , SEEK_CUR);
    WriteUint16(bmp_file, info_header.biBitCount);
    fseek(bmp_file, 24 , SEEK_CUR);

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
    char *name1;
    char *name2;
    char tempc[30];
    BITMAP *BMP1;
    BITMAP *BMP2;
    float temp;
    int ops;


    if (argc > 1 ) {
        name1 = argv[1];
        BMP1 = bmp_read(name1);
    } else {
        printf("First bmp:  ");
        gets(tempc);
        BMP1 = bmp_read(tempc);
    }


    if (argc > 2 ) {
        name2 = argv[2];
        BMP2 = bmp_read(name2);
    } else {
        printf("Second bmp:  ");
        gets(tempc);
        BMP2 = bmp_read(tempc);
    }

    if (argc > 3) {
        temp = atof(argv[3]);
    } else {
        printf("coefficient of opacity from 0 to 1:  ");
        scanf("%f", &temp);
    }

    ops = ((int)(temp * 255));

    BMP2 = bmp_combine(BMP1, BMP2, ops);


    bmp_write(BMP2, "bmp2.bmp");



    return 0;
}
