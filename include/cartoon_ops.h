/**
 * @file cartoon_ops.h
 * @brief Algoritmos y operadores para la generación de Cartoons a partir de imágenes:
 *        - Conversión a Escala de Grises
 *        - Filtrado de Suavizado / Borroneo (3x3 y 5x5)
 *        - Resaltado de Gradiente (Sobel 3x3 y 5x5)
 *        - Detección de Bordes por Umbralado
 *        - Posterizado de Canales RGB (3 y 9 niveles)
 *        - Combinación final para estilo Cartoon
 * 
 * Computación Avanzada - UADER (2026)
 */

#ifndef CARTOON_OPS_H
#define CARTOON_OPS_H

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuración de parámetros de la pipeline */
typedef struct {
    int filter_size;   /* 3 para 3x3, 5 para 5x5 */
    int poster_levels; /* 3 o 9 niveles de posterización */
    int threshold;     /* Umbral de corte para detección de bordes (ej: 70) */
} CartoonParams;

/* Parámetros por defecto */
static inline CartoonParams default_cartoon_params(void) {
    CartoonParams p;
    p.filter_size = 3;
    p.poster_levels = 9;
    p.threshold = 70;
    return p;
}

/* 1. Conversión de RGB a Escala de Grises (Y = 0.30*R + 0.59*G + 0.11*B) */
void rgb_to_gray(const unsigned char *rgb, unsigned char *gray, int width, int height);

/* 2. Suavizado / Borroneo (Box filter de media 3x3 y 5x5) */
void filter_mean_3x3(const unsigned char *src, unsigned char *dst, int width, int height);
void filter_mean_5x5(const unsigned char *src, unsigned char *dst, int width, int height);

/* 3. Resaltado de Bordes mediante Gradiente Sobel */
void sobel_gradient_3x3(const unsigned char *src, unsigned char *dst, int width, int height);
void sobel_gradient_5x5(const unsigned char *src, unsigned char *dst, int width, int height);

/* 4. Detección de Bordes por Umbralado */
void threshold_edges(const unsigned char *grad, unsigned char *edges, int width, int height, int threshold);

/* 5. Posterizado de un arreglo RGB (canales R, G, B independientes) */
void posterize_rgb(const unsigned char *src, unsigned char *dst, int width, int height, int levels);

/* 6. Combinación final: Superposición de contornos de bordes sobre la imagen posterizada */
void combine_cartoon(const unsigned char *poster, const unsigned char *edges, unsigned char *cartoon, int width, int height);

/* Pipeline secuencial completa */
RGBImage* process_cartoon_sequential(const RGBImage *input, CartoonParams params);

#ifdef __cplusplus
}
#endif

#endif /* CARTOON_OPS_H */

