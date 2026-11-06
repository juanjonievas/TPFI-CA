/**
 * @file cartoon_ops.c
 * @brief Implementación de las etapas del algoritmo de generación de Cartoons.
 * 
 * Computación Avanzada - UADER (2026)
 */

#include "cartoon_ops.h"
#include <math.h>

/* Helper para clamping de índices de borde (replicate border) */
static inline int clamp_coord(int pos, int max_val) {
    if (pos < 0) return 0;
    if (pos >= max_val) return max_val - 1;
    return pos;
}

/* 1. Conversión de RGB a Escala de Grises: Y = 0.30*R + 0.59*G + 0.11*B */
void rgb_to_gray(const unsigned char *rgb, unsigned char *gray, int width, int height) {
    size_t total = (size_t)width * height;
    for (size_t i = 0; i < total; i++) {
        float r = (float)rgb[i * 3 + 0];
        float g = (float)rgb[i * 3 + 1];
        float b = (float)rgb[i * 3 + 2];
        int y = (int)(0.30f * r + 0.59f * g + 0.11f * b + 0.5f);
        if (y > 255) y = 255;
        if (y < 0) y = 0;
        gray[i] = (unsigned char)y;
    }
}

/* 2. Suavizado / Borroneo 3x3 (Filtro de Media uniforme 1/9) */
void filter_mean_3x3(const unsigned char *src, unsigned char *dst, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++) {
                int py = clamp_coord(y + dy, height);
                const unsigned char *row = src + (size_t)py * width;
                for (int dx = -1; dx <= 1; dx++) {
                    int px = clamp_coord(x + dx, width);
                    sum += row[px];
                }
            }
            dst[(size_t)y * width + x] = (unsigned char)(sum / 9);
        }
    }
}

/* 2b. Suavizado / Borroneo 5x5 (Filtro de Media uniforme 1/25) */
void filter_mean_5x5(const unsigned char *src, unsigned char *dst, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int dy = -2; dy <= 2; dy++) {
                int py = clamp_coord(y + dy, height);
                const unsigned char *row = src + (size_t)py * width;
                for (int dx = -2; dx <= 2; dx++) {
                    int px = clamp_coord(x + dx, width);
                    sum += row[px];
                }
            }
            dst[(size_t)y * width + x] = (unsigned char)(sum / 25);
        }
    }
}

/* 3. Resaltado de Bordes mediante Gradiente Sobel 3x3 */
void sobel_gradient_3x3(const unsigned char *src, unsigned char *dst, int width, int height) {
    /* Máscaras de Sobel 3x3:
       Sx = [-1  0  1]       Sy = [ 1  2  1]
            [-2  0  2]            [ 0  0  0]
            [-1  0  1]            [-1 -2 -1]
    */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int ym1 = clamp_coord(y - 1, height);
            int y0  = y;
            int yp1 = clamp_coord(y + 1, height);

            int xm1 = clamp_coord(x - 1, width);
            int x0  = x;
            int xp1 = clamp_coord(x + 1, width);

            const unsigned char *r_top = src + (size_t)ym1 * width;
            const unsigned char *r_mid = src + (size_t)y0  * width;
            const unsigned char *r_bot = src + (size_t)yp1 * width;

            int p00 = r_top[xm1], p01 = r_top[x0], p02 = r_top[xp1];
            int p10 = r_mid[xm1],                  p12 = r_mid[xp1];
            int p20 = r_bot[xm1], p21 = r_bot[x0], p22 = r_bot[xp1];

            int gx = (-p00 + p02) + 2 * (-p10 + p12) + (-p20 + p22);
            int gy = (p00 + 2 * p01 + p02) - (p20 + 2 * p21 + p22);

            int mag = (int)(sqrtf((float)(gx * gx + gy * gy)) + 0.5f);
            if (mag > 255) mag = 255;
            dst[(size_t)y * width + x] = (unsigned char)mag;
        }
    }
}

/* 3b. Resaltado de Bordes mediante Gradiente Sobel extendido 5x5 */
void sobel_gradient_5x5(const unsigned char *src, unsigned char *dst, int width, int height) {
    static const int kx[5][5] = {
        { -1, -2, 0,  2, 1 },
        { -4, -8, 0,  8, 4 },
        { -6,-12, 0, 12, 6 },
        { -4, -8, 0,  8, 4 },
        { -1, -2, 0,  2, 1 }
    };
    static const int ky[5][5] = {
        {  1,  4,   6,  4,  1 },
        {  2,  8,  12,  8,  2 },
        {  0,  0,   0,  0,  0 },
        { -2, -8, -12, -8, -2 },
        { -1, -4,  -6, -4, -1 }
    };

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int gx = 0, gy = 0;
            for (int dy = -2; dy <= 2; dy++) {
                int py = clamp_coord(y + dy, height);
                const unsigned char *row = src + (size_t)py * width;
                for (int dx = -2; dx <= 2; dx++) {
                    int px = clamp_coord(x + dx, width);
                    int val = row[px];
                    gx += val * kx[dy + 2][dx + 2];
                    gy += val * ky[dy + 2][dx + 2];
                }
            }
            /* Normalizar por el factor de escala acumulado del filtro 5x5 (48) */
            float fgx = (float)gx / 48.0f;
            float fgy = (float)gy / 48.0f;
            int mag = (int)(sqrtf(fgx * fgx + fgy * fgy) + 0.5f);
            if (mag > 255) mag = 255;
            dst[(size_t)y * width + x] = (unsigned char)mag;
        }
    }
}

/* 4. Detección de Bordes por Umbralado */
void threshold_edges(const unsigned char *grad, unsigned char *edges, int width, int height, int threshold) {
    size_t total = (size_t)width * height;
    for (size_t i = 0; i < total; i++) {
        edges[i] = (grad[i] >= threshold) ? 255 : 0;
    }
}

/* 5. Posterizado de Canales RGB (3 y 9 niveles) */
void posterize_rgb(const unsigned char *src, unsigned char *dst, int width, int height, int levels) {
    if (levels < 2) levels = 2;
    float step = 256.0f / (float)levels;
    float scale = 255.0f / (float)(levels - 1);

    /* Precalcular tabla de cuantización para los 256 valores posibles [0..255] */
    unsigned char lut[256];
    for (int v = 0; v < 256; v++) {
        int bucket = (int)((float)v / step);
        if (bucket >= levels) bucket = levels - 1;
        int q = (int)((float)bucket * scale + 0.5f);
        if (q > 255) q = 255;
        lut[v] = (unsigned char)q;
    }

    size_t total_bytes = (size_t)width * height * 3;
    for (size_t i = 0; i < total_bytes; i++) {
        dst[i] = lut[src[i]];
    }
}

/* 6. Combinación final: Superposición de contornos (bordes negros) sobre imagen posterizada */
void combine_cartoon(const unsigned char *poster, const unsigned char *edges, unsigned char *cartoon, int width, int height) {
    size_t total_pixels = (size_t)width * height;
    for (size_t i = 0; i < total_pixels; i++) {
        /* Si edges[i] == 255 es un borde: asignamos trazo negro (0, 0, 0)
           Si edges[i] == 0: dejamos el color original posterizado */
        if (edges[i] == 255) {
            cartoon[i * 3 + 0] = 0;
            cartoon[i * 3 + 1] = 0;
            cartoon[i * 3 + 2] = 0;
        } else {
            cartoon[i * 3 + 0] = poster[i * 3 + 0];
            cartoon[i * 3 + 1] = poster[i * 3 + 1];
            cartoon[i * 3 + 2] = poster[i * 3 + 2];
        }
    }
}

/* Pipeline secuencial completa */
RGBImage* process_cartoon_sequential(const RGBImage *input, CartoonParams params) {
    if (!input || !input->data) return NULL;
    int w = input->width;
    int h = input->height;

    GrayImage *gray = create_gray_image(w, h);
    GrayImage *smooth = create_gray_image(w, h);
    GrayImage *grad = create_gray_image(w, h);
    GrayImage *edges = create_gray_image(w, h);
    RGBImage  *poster = create_rgb_image(w, h);
    RGBImage  *cartoon = create_rgb_image(w, h);

    if (!gray || !smooth || !grad || !edges || !poster || !cartoon) {
        if (gray) free_gray_image(gray);
        if (smooth) free_gray_image(smooth);
        if (grad) free_gray_image(grad);
        if (edges) free_gray_image(edges);
        if (poster) free_rgb_image(poster);
        if (cartoon) free_rgb_image(cartoon);
        return NULL;
    }

    /* Paso 1: Conversión a Grises */
    rgb_to_gray(input->data, gray->data, w, h);

    /* Paso 2: Suavizado (3x3 o 5x5) */
    if (params.filter_size == 5) {
        filter_mean_5x5(gray->data, smooth->data, w, h);
    } else {
        filter_mean_3x3(gray->data, smooth->data, w, h);
    }

    /* Paso 3: Gradiente Sobel */
    if (params.filter_size == 5) {
        sobel_gradient_5x5(smooth->data, grad->data, w, h);
    } else {
        sobel_gradient_3x3(smooth->data, grad->data, w, h);
    }

    /* Paso 4: Umbralado de Bordes */
    threshold_edges(grad->data, edges->data, w, h, params.threshold);

    /* Paso 5: Posterizado de Canales RGB */
    posterize_rgb(input->data, poster->data, w, h, params.poster_levels);

    /* Paso 6: Fusión Cartoon */
    combine_cartoon(poster->data, edges->data, cartoon->data, w, h);

    /* Liberar buffers intermedios */
    free_gray_image(gray);
    free_gray_image(smooth);
    free_gray_image(grad);
    free_gray_image(edges);
    free_rgb_image(poster);

    return cartoon;
}

