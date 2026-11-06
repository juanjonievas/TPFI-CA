/**
 * @file image.h
 * @brief Manejo y estructuras de datos para imágenes digitales (RGB y Escala de Grises).
 * Soporta formatos BMP de 24 bits y PPM (P6/P5).
 * 
 * Computación Avanzada - UADER (2026)
 */

#ifndef IMAGE_H
#define IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Estructura para imagen en color RGB (24 bits: 3 bytes por píxel).
 */
typedef struct {
    int width;
    int height;
    unsigned char *data; /* Arreglo plano de tamaño width * height * 3 (orden R, G, B) */
} RGBImage;

/**
 * @brief Estructura para imagen en escala de grises (8 bits: 1 byte por píxel).
 */
typedef struct {
    int width;
    int height;
    unsigned char *data; /* Arreglo plano de tamaño width * height */
} GrayImage;

/* Funciones de creación y liberación de memoria */
RGBImage* create_rgb_image(int width, int height);
void free_rgb_image(RGBImage *img);

GrayImage* create_gray_image(int width, int height);
void free_gray_image(GrayImage *img);

/* Carga y guardado genérico según extensión (.bmp o .ppm) */
RGBImage* load_rgb_image(const char *filename);
int save_rgb_image(const char *filename, const RGBImage *img);
int save_gray_image(const char *filename, const GrayImage *img);

/* Carga y guardado específico PPM */
RGBImage* load_ppm_rgb(const char *filename);
int save_ppm_rgb(const char *filename, const RGBImage *img);
int save_pgm_gray(const char *filename, const GrayImage *img);

/* Carga y guardado específico BMP (24-bit uncompressed) */
RGBImage* load_bmp_rgb(const char *filename);
int save_bmp_rgb(const char *filename, const RGBImage *img);

/* Verificación de equivalencia numérica entre dos imágenes */
int compare_rgb_images(const RGBImage *img1, const RGBImage *img2, int max_diff_allowed);

#ifdef __cplusplus
}
#endif

#endif /* IMAGE_H */

