/**
 * @file secuencial.c
 * @brief Implementación Secuencial de la Pipeline de Generación de Cartoons.
 *
 * Computación Avanzada - UADER (2026)
 */

#include "cartoon_ops.h"
#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double get_time_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Uso: %s <imagen_entrada> <imagen_salida> [tamanio_filtro=3|5] "
           "[niveles_posterizado=3|9] [umbral=70]\n",
           argv[0]);
    return 1;
  }

  const char *input_path = argv[1];
  const char *output_path = argv[2];

  CartoonParams params = default_cartoon_params();
  if (argc >= 4)
    params.filter_size = atoi(argv[3]);
  if (argc >= 5)
    params.poster_levels = atoi(argv[4]);
  if (argc >= 6)
    params.threshold = atoi(argv[5]);

  printf("============================================================\n");
  printf("  TPFI - Computacion Avanzada (UADER)\n");
  printf("  Version: SECUENCIAL\n");
  printf("  Entrada:     %s\n", input_path);
  printf("  Salida:      %s\n", output_path);
  printf("  Filtro:      %dx%d\n", params.filter_size, params.filter_size);
  printf("  Posterizado: %d niveles\n", params.poster_levels);
  printf("  Umbral:      %d\n", params.threshold);
  printf("============================================================\n");

  /* Carga de la imagen */
  double t_load_start = get_time_sec();
  RGBImage *input = load_rgb_image(input_path);
  double t_load_end = get_time_sec();

  if (!input) {
    fprintf(stderr, "[Error] No se pudo cargar la imagen de entrada: %s\n",
            input_path);
    return 1;
  }
  printf("Imagen cargada: %dx%d pixeles (%zu bytes) en %.4f seg.\n",
         input->width, input->height, (size_t)input->width * input->height * 3,
         t_load_end - t_load_start);

  /* Procesamiento secuencial del pipeline completo */
  double t_proc_start = get_time_sec();
  RGBImage *cartoon = process_cartoon_sequential(input, params);
  double t_proc_end = get_time_sec();

  if (!cartoon) {
    fprintf(stderr, "[Error] Fallo el procesamiento cartoon.\n");
    free_rgb_image(input);
    return 1;
  }

  double elapsed_proc = t_proc_end - t_proc_start;
  printf(">>> TIEMPO DE COMPUTO SECUENCIAL: %.6f segundos (%.2f ms) <<<\n",
         elapsed_proc, elapsed_proc * 1000.0);

  /* Guardado de la imagen resultante */
  double t_save_start = get_time_sec();
  if (save_rgb_image(output_path, cartoon) != 0) {
    fprintf(stderr, "[Error] No se pudo guardar la imagen cartoon en: %s\n",
            output_path);
  } else {
    printf("Resultado guardado en %s (%.4f seg).\n", output_path,
           get_time_sec() - t_save_start);
  }

  free_rgb_image(input);
  free_rgb_image(cartoon);
  return 0;
}
