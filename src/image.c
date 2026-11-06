/**
 * @file image.c
 * @brief Implementación de funciones de E/S y manipulación de imágenes (PPM y BMP de 24 bits).
 * 
 * Computación Avanzada - UADER (2026)
 */

#include "image.h"
#include <math.h>
#include <ctype.h>

#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;      /* 'BM' = 0x4D42 */
    unsigned int   bfSize;      /* Tamaño del archivo en bytes */
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;   /* Offset a los datos de píxeles */
} BMPFileHeader;

typedef struct {
    unsigned int   biSize;          /* Tamaño de cabecera Info (40 bytes) */
    int            biWidth;         /* Ancho en píxeles */
    int            biHeight;        /* Alto en píxeles (positivo = abajo hacia arriba) */
    unsigned short biPlanes;        /* 1 */
    unsigned short biBitCount;      /* 24 bits por píxel */
    unsigned int   biCompression;   /* 0 = BI_RGB sin compresión */
    unsigned int   biSizeImage;     /* Tamaño de la imagen en bytes */
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

RGBImage* create_rgb_image(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    RGBImage *img = (RGBImage*)malloc(sizeof(RGBImage));
    if (!img) return NULL;
    img->width = width;
    img->height = height;
    img->data = (unsigned char*)malloc((size_t)width * height * 3);
    if (!img->data) {
        free(img);
        return NULL;
    }
    return img;
}

void free_rgb_image(RGBImage *img) {
    if (img) {
        if (img->data) free(img->data);
        free(img);
    }
}

GrayImage* create_gray_image(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    GrayImage *img = (GrayImage*)malloc(sizeof(GrayImage));
    if (!img) return NULL;
    img->width = width;
    img->height = height;
    img->data = (unsigned char*)malloc((size_t)width * height);
    if (!img->data) {
        free(img);
        return NULL;
    }
    return img;
}

void free_gray_image(GrayImage *img) {
    if (img) {
        if (img->data) free(img->data);
        free(img);
    }
}

/* Helper para saltar espacios y comentarios en cabeceras Netpbm */
static void skip_pnm_comments(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (isspace(ch)) continue;
        if (ch == '#') {
            while ((ch = fgetc(fp)) != EOF && ch != '\n' && ch != '\r');
        } else {
            ungetc(ch, fp);
            break;
        }
    }
}

RGBImage* load_ppm_rgb(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "[Error] No se pudo abrir el archivo PPM: %s\n", filename);
        return NULL;
    }

    char magic[3] = {0};
    if (fscanf(fp, "%2s", magic) != 1 || (strcmp(magic, "P6") != 0 && strcmp(magic, "P3") != 0)) {
        fprintf(stderr, "[Error] Formato PPM no soportado o inválido: %s\n", filename);
        fclose(fp);
        return NULL;
    }

    skip_pnm_comments(fp);
    int width = 0, height = 0, maxval = 0;
    if (fscanf(fp, "%d", &width) != 1) { fclose(fp); return NULL; }
    skip_pnm_comments(fp);
    if (fscanf(fp, "%d", &height) != 1) { fclose(fp); return NULL; }
    skip_pnm_comments(fp);
    if (fscanf(fp, "%d", &maxval) != 1) { fclose(fp); return NULL; }
    fgetc(fp); /* Consumir un solo espacio/salto de línea final */

    RGBImage *img = create_rgb_image(width, height);
    if (!img) {
        fclose(fp);
        return NULL;
    }

    if (strcmp(magic, "P6") == 0) {
        size_t total_bytes = (size_t)width * height * 3;
        size_t read_bytes = fread(img->data, 1, total_bytes, fp);
        if (read_bytes != total_bytes) {
            fprintf(stderr, "[Aviso] Se leyeron %zu de %zu bytes esperados en %s\n", read_bytes, total_bytes, filename);
        }
    } else {
        /* P3 ASCII */
        for (int i = 0; i < width * height * 3; i++) {
            int v;
            if (fscanf(fp, "%d", &v) == 1) {
                img->data[i] = (unsigned char)v;
            }
        }
    }

    fclose(fp);
    return img;
}

int save_ppm_rgb(const char *filename, const RGBImage *img) {
    if (!img || !img->data) return -1;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "[Error] No se pudo crear archivo PPM: %s\n", filename);
        return -1;
    }
    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    size_t total_bytes = (size_t)img->width * img->height * 3;
    fwrite(img->data, 1, total_bytes, fp);
    fclose(fp);
    return 0;
}

int save_pgm_gray(const char *filename, const GrayImage *img) {
    if (!img || !img->data) return -1;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "[Error] No se pudo crear archivo PGM: %s\n", filename);
        return -1;
    }
    fprintf(fp, "P5\n%d %d\n255\n", img->width, img->height);
    size_t total_bytes = (size_t)img->width * img->height;
    fwrite(img->data, 1, total_bytes, fp);
    fclose(fp);
    return 0;
}

RGBImage* load_bmp_rgb(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "[Error] No se pudo abrir BMP: %s\n", filename);
        return NULL;
    }

    BMPFileHeader file_hdr;
    BMPInfoHeader info_hdr;

    if (fread(&file_hdr, sizeof(BMPFileHeader), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (file_hdr.bfType != 0x4D42) { /* 'BM' */
        fprintf(stderr, "[Error] Archivo no es un BMP válido: %s\n", filename);
        fclose(fp);
        return NULL;
    }

    if (fread(&info_hdr, sizeof(BMPInfoHeader), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }

    if (info_hdr.biBitCount != 24 || info_hdr.biCompression != 0) {
        fprintf(stderr, "[Error] Solo se soportan BMPs de 24 bpp sin compresión (encontrado: %d bpp, compresión: %u)\n",
                info_hdr.biBitCount, info_hdr.biCompression);
        fclose(fp);
        return NULL;
    }

    int width = info_hdr.biWidth;
    int height = abs(info_hdr.biHeight);
    int is_top_down = (info_hdr.biHeight < 0);

    RGBImage *img = create_rgb_image(width, height);
    if (!img) {
        fclose(fp);
        return NULL;
    }

    fseek(fp, file_hdr.bfOffBits, SEEK_SET);

    int row_padding = (4 - ((width * 3) % 4)) % 4;
    unsigned char pad_buffer[4];

    for (int r = 0; r < height; r++) {
        int target_row = is_top_down ? r : (height - 1 - r);
        unsigned char *row_ptr = img->data + (size_t)target_row * width * 3;

        for (int c = 0; c < width; c++) {
            unsigned char bgr[3];
            if (fread(bgr, 1, 3, fp) != 3) {
                bgr[0] = bgr[1] = bgr[2] = 0;
            }
            row_ptr[c * 3 + 0] = bgr[2]; /* Red */
            row_ptr[c * 3 + 1] = bgr[1]; /* Green */
            row_ptr[c * 3 + 2] = bgr[0]; /* Blue */
        }
        if (row_padding > 0) {
            fread(pad_buffer, 1, row_padding, fp);
        }
    }

    fclose(fp);
    return img;
}

int save_bmp_rgb(const char *filename, const RGBImage *img) {
    if (!img || !img->data) return -1;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "[Error] No se pudo crear BMP: %s\n", filename);
        return -1;
    }

    int width = img->width;
    int height = img->height;
    int row_padding = (4 - ((width * 3) % 4)) % 4;
    int stride = width * 3 + row_padding;
    unsigned int image_size = (unsigned int)stride * height;
    unsigned int file_size = 54 + image_size;

    BMPFileHeader file_hdr;
    file_hdr.bfType = 0x4D42; /* 'BM' */
    file_hdr.bfSize = file_size;
    file_hdr.bfReserved1 = 0;
    file_hdr.bfReserved2 = 0;
    file_hdr.bfOffBits = 54;

    BMPInfoHeader info_hdr;
    memset(&info_hdr, 0, sizeof(BMPInfoHeader));
    info_hdr.biSize = 40;
    info_hdr.biWidth = width;
    info_hdr.biHeight = height; /* Bottom-up */
    info_hdr.biPlanes = 1;
    info_hdr.biBitCount = 24;
    info_hdr.biCompression = 0;
    info_hdr.biSizeImage = image_size;
    info_hdr.biXPelsPerMeter = 2835; /* 72 DPI */
    info_hdr.biYPelsPerMeter = 2835;

    fwrite(&file_hdr, sizeof(BMPFileHeader), 1, fp);
    fwrite(&info_hdr, sizeof(BMPInfoHeader), 1, fp);

    unsigned char pad[4] = {0, 0, 0, 0};

    /* BMP estándar almacena filas de abajo hacia arriba */
    for (int r = height - 1; r >= 0; r--) {
        const unsigned char *row_ptr = img->data + (size_t)r * width * 3;
        for (int c = 0; c < width; c++) {
            unsigned char bgr[3];
            bgr[0] = row_ptr[c * 3 + 2]; /* B */
            bgr[1] = row_ptr[c * 3 + 1]; /* G */
            bgr[2] = row_ptr[c * 3 + 0]; /* R */
            fwrite(bgr, 1, 3, fp);
        }
        if (row_padding > 0) {
            fwrite(pad, 1, row_padding, fp);
        }
    }

    fclose(fp);
    return 0;
}

RGBImage* load_rgb_image(const char *filename) {
    if (!filename) return NULL;
    size_t len = strlen(filename);
    if (len >= 4 && (strcmp(filename + len - 4, ".bmp") == 0 || strcmp(filename + len - 4, ".BMP") == 0)) {
        return load_bmp_rgb(filename);
    }
    if (len >= 4 && (strcmp(filename + len - 4, ".ppm") == 0 || strcmp(filename + len - 4, ".PPM") == 0)) {
        return load_ppm_rgb(filename);
    }
    /* Intento por omisión BMP, luego PPM */
    RGBImage *img = load_bmp_rgb(filename);
    if (!img) img = load_ppm_rgb(filename);
    return img;
}

int save_rgb_image(const char *filename, const RGBImage *img) {
    if (!filename || !img) return -1;
    size_t len = strlen(filename);
    if (len >= 4 && (strcmp(filename + len - 4, ".ppm") == 0 || strcmp(filename + len - 4, ".PPM") == 0)) {
        return save_ppm_rgb(filename, img);
    }
    return save_bmp_rgb(filename, img);
}

int save_gray_image(const char *filename, const GrayImage *img) {
    if (!filename || !img) return -1;
    /* Guarda como PGM si termina en .pgm, o como BMP 24bpp en escala de grises si termina en .bmp */
    size_t len = strlen(filename);
    if (len >= 4 && (strcmp(filename + len - 4, ".pgm") == 0 || strcmp(filename + len - 4, ".PGM") == 0)) {
        return save_pgm_gray(filename, img);
    }
    /* Convertir a RGB temporal para guardar como BMP */
    RGBImage *rgb = create_rgb_image(img->width, img->height);
    if (!rgb) return -1;
    size_t total = (size_t)img->width * img->height;
    for (size_t i = 0; i < total; i++) {
        unsigned char g = img->data[i];
        rgb->data[i * 3 + 0] = g;
        rgb->data[i * 3 + 1] = g;
        rgb->data[i * 3 + 2] = g;
    }
    int res = save_bmp_rgb(filename, rgb);
    free_rgb_image(rgb);
    return res;
}

int compare_rgb_images(const RGBImage *img1, const RGBImage *img2, int max_diff_allowed) {
    if (!img1 || !img2) return 0;
    if (img1->width != img2->width || img1->height != img2->height) {
        printf("[Comparación] Error: Dimensiones diferentes (%dx%d vs %dx%d)\n",
               img1->width, img1->height, img2->width, img2->height);
        return 0;
    }
    size_t total_bytes = (size_t)img1->width * img1->height * 3;
    long long diff_count = 0;
    int max_observed_diff = 0;

    for (size_t i = 0; i < total_bytes; i++) {
        int diff = abs((int)img1->data[i] - (int)img2->data[i]);
        if (diff > max_observed_diff) max_observed_diff = diff;
        if (diff > max_diff_allowed) {
            diff_count++;
        }
    }

    if (diff_count == 0) {
        printf("[Comparación] OK: Las imágenes son idénticas (diferencia máxima observada = %d).\n", max_observed_diff);
        return 1;
    } else {
        printf("[Comparación] DISCREPANCIA: %lld bytes difieren por encima de la tolerancia %d (diferencia máxima = %d).\n",
               diff_count, max_diff_allowed, max_observed_diff);
        return 0;
    }
}

