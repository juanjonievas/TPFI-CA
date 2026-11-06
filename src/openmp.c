/**
 * @file openmp.c
 * @brief Implementación Paralela en Memoria Compartida (OpenMP) de la Pipeline de Cartoons.
 * 
 * Computación Avanzada - UADER (2026)
 */

#include "image.h"
#include "cartoon_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

static inline int clamp_coord(int pos, int max_val) {
    if (pos < 0) return 0;
    if (pos >= max_val) return max_val - 1;
    return pos;
}

/* 1. Conversión de RGB a Grises paralela con OpenMP */
static void omp_rgb_to_gray(const unsigned char *rgb, unsigned char *gray, int width, int height) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        size_t row_offset = (size_t)y * width;
        for (int x = 0; x < width; x++) {
            size_t idx = row_offset + x;
            float r = (float)rgb[idx * 3 + 0];
            float g = (float)rgb[idx * 3 + 1];
            float b = (float)rgb[idx * 3 + 2];
            int lum = (int)(0.30f * r + 0.59f * g + 0.11f * b + 0.5f);
            if (lum > 255) lum = 255;
            if (lum < 0) lum = 0;
            gray[idx] = (unsigned char)lum;
        }
    }
}

/* 2. Suavizado 3x3 paralelo */
static void omp_filter_mean_3x3(const unsigned char *src, unsigned char *dst, int width, int height) {
    #pragma omp parallel for schedule(static)
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

/* 2b. Suavizado 5x5 paralelo */
static void omp_filter_mean_5x5(const unsigned char *src, unsigned char *dst, int width, int height) {
    #pragma omp parallel for schedule(static)
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

/* 3. Gradiente Sobel 3x3 paralelo */
static void omp_sobel_gradient_3x3(const unsigned char *src, unsigned char *dst, int width, int height) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        int ym1 = clamp_coord(y - 1, height);
        int y0  = y;
        int yp1 = clamp_coord(y + 1, height);

        const unsigned char *r_top = src + (size_t)ym1 * width;
        const unsigned char *r_mid = src + (size_t)y0  * width;
        const unsigned char *r_bot = src + (size_t)yp1 * width;

        for (int x = 0; x < width; x++) {
            int xm1 = clamp_coord(x - 1, width);
            int x0  = x;
            int xp1 = clamp_coord(x + 1, width);

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

/* 3b. Gradiente Sobel 5x5 paralelo */
static void omp_sobel_gradient_5x5(const unsigned char *src, unsigned char *dst, int width, int height) {
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

    #pragma omp parallel for schedule(static)
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
            float fgx = (float)gx / 48.0f;
            float fgy = (float)gy / 48.0f;
            int mag = (int)(sqrtf(fgx * fgx + fgy * fgy) + 0.5f);
            if (mag > 255) mag = 255;
            dst[(size_t)y * width + x] = (unsigned char)mag;
        }
    }
}

/* 4. Umbralado paralelo */
static void omp_threshold_edges(const unsigned char *grad, unsigned char *edges, int width, int height, int threshold) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        size_t offset = (size_t)y * width;
        for (int x = 0; x < width; x++) {
            edges[offset + x] = (grad[offset + x] >= threshold) ? 255 : 0;
        }
    }
}

/* 5. Posterizado RGB paralelo */
static void omp_posterize_rgb(const unsigned char *src, unsigned char *dst, int width, int height, int levels) {
    if (levels < 2) levels = 2;
    float step = 256.0f / (float)levels;
    float scale = 255.0f / (float)(levels - 1);

    unsigned char lut[256];
    for (int v = 0; v < 256; v++) {
        int bucket = (int)((float)v / step);
        if (bucket >= levels) bucket = levels - 1;
        int q = (int)((float)bucket * scale + 0.5f);
        if (q > 255) q = 255;
        lut[v] = (unsigned char)q;
    }

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        size_t offset = (size_t)y * width * 3;
        for (int x = 0; x < width * 3; x++) {
            dst[offset + x] = lut[src[offset + x]];
        }
    }
}

/* 6. Fusión Cartoon paralela */
static void omp_combine_cartoon(const unsigned char *poster, const unsigned char *edges, unsigned char *cartoon, int width, int height) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; y++) {
        size_t p_offset = (size_t)y * width;
        size_t c_offset = p_offset * 3;
        for (int x = 0; x < width; x++) {
            if (edges[p_offset + x] == 255) {
                cartoon[c_offset + x * 3 + 0] = 0;
                cartoon[c_offset + x * 3 + 1] = 0;
                cartoon[c_offset + x * 3 + 2] = 0;
            } else {
                cartoon[c_offset + x * 3 + 0] = poster[c_offset + x * 3 + 0];
                cartoon[c_offset + x * 3 + 1] = poster[c_offset + x * 3 + 1];
                cartoon[c_offset + x * 3 + 2] = poster[c_offset + x * 3 + 2];
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <imagen_entrada> <imagen_salida> [num_threads=4] [tamanio_filtro=3|5] [niveles_posterizado=3|9] [umbral=70]\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    int num_threads = 4;
    CartoonParams params = default_cartoon_params();

    if (argc >= 4) num_threads = atoi(argv[3]);
    if (argc >= 5) params.filter_size = atoi(argv[4]);
    if (argc >= 6) params.poster_levels = atoi(argv[5]);
    if (argc >= 7) params.threshold = atoi(argv[6]);

    if (num_threads > 0) {
        omp_set_num_threads(num_threads);
    }

    int actual_threads = 1;
    #pragma omp parallel
    {
        #pragma omp single
        actual_threads = omp_get_num_threads();
    }

    printf("============================================================\n");
    printf("  TPFI - Computacion Avanzada (UADER)\n");
    printf("  Version: MEMORIA COMPARTIDA (OpenMP)\n");
    printf("  Threads:     %d\n", actual_threads);
    printf("  Entrada:     %s\n", input_path);
    printf("  Salida:      %s\n", output_path);
    printf("  Filtro:      %dx%d\n", params.filter_size, params.filter_size);
    printf("  Posterizado: %d niveles\n", params.poster_levels);
    printf("  Umbral:      %d\n", params.threshold);
    printf("============================================================\n");

    /* Carga de la imagen */
    double t_load_start = omp_get_wtime();
    RGBImage *input = load_rgb_image(input_path);
    double t_load_end = omp_get_wtime();

    if (!input) {
        fprintf(stderr, "[Error] No se pudo cargar la imagen: %s\n", input_path);
        return 1;
    }
    printf("Imagen cargada: %dx%d pixeles en %.4f seg.\n", input->width, input->height, t_load_end - t_load_start);

    int w = input->width;
    int h = input->height;

    GrayImage *gray = create_gray_image(w, h);
    GrayImage *smooth = create_gray_image(w, h);
    GrayImage *grad = create_gray_image(w, h);
    GrayImage *edges = create_gray_image(w, h);
    RGBImage  *poster = create_rgb_image(w, h);
    RGBImage  *cartoon = create_rgb_image(w, h);

    if (!gray || !smooth || !grad || !edges || !poster || !cartoon) {
        fprintf(stderr, "[Error] Memoria insuficiente para buffers de procesamiento.\n");
        return 1;
    }

    /* Inicio de medición de cómputo paralelo */
    double t_start = omp_get_wtime();

    /* Paso 1: Conversión a Grises */
    omp_rgb_to_gray(input->data, gray->data, w, h);

    /* Paso 2: Suavizado */
    if (params.filter_size == 5) {
        omp_filter_mean_5x5(gray->data, smooth->data, w, h);
    } else {
        omp_filter_mean_3x3(gray->data, smooth->data, w, h);
    }

    /* Paso 3: Gradiente Sobel */
    if (params.filter_size == 5) {
        omp_sobel_gradient_5x5(smooth->data, grad->data, w, h);
    } else {
        omp_sobel_gradient_3x3(smooth->data, grad->data, w, h);
    }

    /* Paso 4: Umbralado */
    omp_threshold_edges(grad->data, edges->data, w, h, params.threshold);

    /* Paso 5: Posterizado */
    omp_posterize_rgb(input->data, poster->data, w, h, params.poster_levels);

    /* Paso 6: Fusión Cartoon */
    omp_combine_cartoon(poster->data, edges->data, cartoon->data, w, h);

    double t_end = omp_get_wtime();
    double elapsed = t_end - t_start;

    printf(">>> TIEMPO DE COMPUTO OPENMP (%d threads): %.6f segundos (%.2f ms) <<<\n",
           actual_threads, elapsed, elapsed * 1000.0);

    /* Guardar resultado */
    if (save_rgb_image(output_path, cartoon) != 0) {
        fprintf(stderr, "[Error] No se pudo guardar imagen de salida: %s\n", output_path);
    } else {
        printf("Resultado guardado en %s\n", output_path);
    }

    /* Liberación de memoria */
    free_rgb_image(input);
    free_gray_image(gray);
    free_gray_image(smooth);
    free_gray_image(grad);
    free_gray_image(edges);
    free_rgb_image(poster);
    free_rgb_image(cartoon);

    return 0;
}

