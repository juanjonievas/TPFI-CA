/**
 * @file mpi.c
 * @brief Implementación Paralela en Memoria Distribuida (MPI) de la Pipeline de Cartoons.
 *        Utiliza descomposición de dominio 1D por bloques de filas e intercambio
 *        de filas fantasma (halo / ghost rows) para las etapas de convolución espacial.
 * 
 * Computación Avanzada - UADER (2026)
 */

#include "image.h"
#include "cartoon_ops.h"
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline int clamp_coord(int pos, int max_val) {
    if (pos < 0) return 0;
    if (pos >= max_val) return max_val - 1;
    return pos;
}

/**
 * @brief Realiza el intercambio de filas halo (filas fantasma) entre procesos vecinos en MPI.
 * @param local_buf Buffer con espacio para (halo_radius + local_h + halo_radius) * width
 * @param width Ancho de la imagen
 * @param local_h Altura de la porción local
 * @param halo_radius Radio de la vecindad (1 para 3x3, 2 para 5x5)
 * @param rank Rango actual
 * @param size Cantidad de procesos
 * @param is_top_boundary 1 si este proceso tiene el borde superior global (rank == 0)
 * @param is_bottom_boundary 1 si este proceso tiene el borde inferior global (rank == size - 1)
 */
static void exchange_halos_1d(unsigned char *local_buf, int width, int local_h, int halo_radius,
                              int rank, int size, int is_top_boundary, int is_bottom_boundary) {
    int top_neighbor = (rank > 0) ? rank - 1 : MPI_PROC_NULL;
    int bot_neighbor = (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;

    /* Punteros dentro del buffer extendido:
       - Fila 0 .. halo_radius - 1: Halo superior
       - Fila halo_radius .. halo_radius + local_h - 1: Filas locales de cómputo
       - Fila halo_radius + local_h .. halo_radius + local_h + halo_radius - 1: Halo inferior
    */
    unsigned char *send_top = local_buf + (size_t)halo_radius * width;
    unsigned char *recv_top = local_buf;
    unsigned char *send_bot = local_buf + (size_t)(halo_radius + local_h - halo_radius) * width;
    unsigned char *recv_bot = local_buf + (size_t)(halo_radius + local_h) * width;

    int halo_count = halo_radius * width;
    MPI_Request reqs[4];
    int req_count = 0;

    /* Comunicación no bloqueante con el vecino superior */
    if (top_neighbor != MPI_PROC_NULL) {
        MPI_Irecv(recv_top, halo_count, MPI_UNSIGNED_CHAR, top_neighbor, 100, MPI_COMM_WORLD, &reqs[req_count++]);
        MPI_Isend(send_top, halo_count, MPI_UNSIGNED_CHAR, top_neighbor, 200, MPI_COMM_WORLD, &reqs[req_count++]);
    }

    /* Comunicación no bloqueante con el vecino inferior */
    if (bot_neighbor != MPI_PROC_NULL) {
        MPI_Irecv(recv_bot, halo_count, MPI_UNSIGNED_CHAR, bot_neighbor, 200, MPI_COMM_WORLD, &reqs[req_count++]);
        MPI_Isend(send_bot, halo_count, MPI_UNSIGNED_CHAR, bot_neighbor, 100, MPI_COMM_WORLD, &reqs[req_count++]);
    }

    if (req_count > 0) {
        MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);
    }

    /* Relleno de bordes globales físicos mediante replicación (clamping) */
    if (is_top_boundary) {
        for (int r = 0; r < halo_radius; r++) {
            memcpy(local_buf + (size_t)r * width, send_top, width);
        }
    }
    if (is_bottom_boundary) {
        unsigned char *last_local_row = local_buf + (size_t)(halo_radius + local_h - 1) * width;
        for (int r = 0; r < halo_radius; r++) {
            memcpy(recv_bot + (size_t)r * width, last_local_row, width);
        }
    }
}

/* Suavizado local 3x3 usando el buffer extendido con halos */
static void local_filter_mean_3x3(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
    int halo = 1;
    for (int y = 0; y < local_h; y++) {
        int buf_y = y + halo;
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++) {
                const unsigned char *row = src_ext + (size_t)(buf_y + dy) * width;
                for (int dx = -1; dx <= 1; dx++) {
                    int px = clamp_coord(x + dx, width);
                    sum += row[px];
                }
            }
            dst_local[(size_t)y * width + x] = (unsigned char)(sum / 9);
        }
    }
}

/* Suavizado local 5x5 usando el buffer extendido con halos */
static void local_filter_mean_5x5(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
    int halo = 2;
    for (int y = 0; y < local_h; y++) {
        int buf_y = y + halo;
        for (int x = 0; x < width; x++) {
            int sum = 0;
            for (int dy = -2; dy <= 2; dy++) {
                const unsigned char *row = src_ext + (size_t)(buf_y + dy) * width;
                for (int dx = -2; dx <= 2; dx++) {
                    int px = clamp_coord(x + dx, width);
                    sum += row[px];
                }
            }
            dst_local[(size_t)y * width + x] = (unsigned char)(sum / 25);
        }
    }
}

/* Gradiente Sobel local 3x3 usando buffer extendido con halos */
static void local_sobel_gradient_3x3(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
    int halo = 1;
    for (int y = 0; y < local_h; y++) {
        int buf_y = y + halo;
        const unsigned char *r_top = src_ext + (size_t)(buf_y - 1) * width;
        const unsigned char *r_mid = src_ext + (size_t)(buf_y)     * width;
        const unsigned char *r_bot = src_ext + (size_t)(buf_y + 1) * width;

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
            dst_local[(size_t)y * width + x] = (unsigned char)mag;
        }
    }
}

/* Gradiente Sobel local 5x5 usando buffer extendido con halos */
static void local_sobel_gradient_5x5(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
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
    int halo = 2;

    for (int y = 0; y < local_h; y++) {
        int buf_y = y + halo;
        for (int x = 0; x < width; x++) {
            int gx = 0, gy = 0;
            for (int dy = -2; dy <= 2; dy++) {
                const unsigned char *row = src_ext + (size_t)(buf_y + dy) * width;
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
            dst_local[(size_t)y * width + x] = (unsigned char)mag;
        }
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            printf("Uso: mpirun -np <N> %s <imagen_entrada> <imagen_salida> [filtro=3|5] [poster=3|9] [umbral=70]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    CartoonParams params = default_cartoon_params();

    if (argc >= 4) params.filter_size = atoi(argv[3]);
    if (argc >= 5) params.poster_levels = atoi(argv[4]);
    if (argc >= 6) params.threshold = atoi(argv[5]);

    int width = 0, height = 0;
    RGBImage *full_input = NULL;

    if (rank == 0) {
        printf("============================================================\n");
        printf("  TPFI - Computacion Avanzada (UADER)\n");
        printf("  Version: MEMORIA DISTRIBUIDA (MPI)\n");
        printf("  Procesos:    %d\n", size);
        printf("  Entrada:     %s\n", input_path);
        printf("  Salida:      %s\n", output_path);
        printf("  Filtro:      %dx%d\n", params.filter_size, params.filter_size);
        printf("  Posterizado: %d niveles\n", params.poster_levels);
        printf("  Umbral:      %d\n", params.threshold);
        printf("============================================================\n");

        full_input = load_rgb_image(input_path);
        if (!full_input) {
            fprintf(stderr, "[Error Rank 0] No se pudo cargar la imagen: %s\n", input_path);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        width = full_input->width;
        height = full_input->height;
        printf("Imagen cargada: %dx%d (%zu bytes)\n", width, height, (size_t)width * height * 3);
    }

    /* Difusión de dimensiones y parámetros a todos los procesos */
    int dims[5];
    if (rank == 0) {
        dims[0] = width;
        dims[1] = height;
        dims[2] = params.filter_size;
        dims[3] = params.poster_levels;
        dims[4] = params.threshold;
    }
    MPI_Bcast(dims, 5, MPI_INT, 0, MPI_COMM_WORLD);
    width = dims[0];
    height = dims[1];
    params.filter_size = dims[2];
    params.poster_levels = dims[3];
    params.threshold = dims[4];

    /* Cálculo de partición 1D por bloques de filas */
    int *sendcounts = (int*)malloc(size * sizeof(int));
    int *displs = (int*)malloc(size * sizeof(int));
    int *sendcounts_rgb = (int*)malloc(size * sizeof(int));
    int *displs_rgb = (int*)malloc(size * sizeof(int));

    int base_rows = height / size;
    int extra_rows = height % size;
    int current_disp = 0;

    for (int i = 0; i < size; i++) {
        int r_count = (i < extra_rows) ? (base_rows + 1) : base_rows;
        sendcounts[i] = r_count * width;
        displs[i] = current_disp * width;
        sendcounts_rgb[i] = sendcounts[i] * 3;
        displs_rgb[i] = displs[i] * 3;
        current_disp += r_count;
    }

    int local_h = (rank < extra_rows) ? (base_rows + 1) : base_rows;
    int halo_radius = (params.filter_size == 5) ? 2 : 1;

    /* Reserva de memoria local */
    unsigned char *local_rgb = (unsigned char*)malloc((size_t)local_h * width * 3);
    unsigned char *local_gray = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_gray_ext = (unsigned char*)malloc((size_t)(local_h + 2 * halo_radius) * width);
    unsigned char *local_smooth = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_smooth_ext = (unsigned char*)malloc((size_t)(local_h + 2 * halo_radius) * width);
    unsigned char *local_grad = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_edges = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_poster = (unsigned char*)malloc((size_t)local_h * width * 3);
    unsigned char *local_cartoon = (unsigned char*)malloc((size_t)local_h * width * 3);

    /* Distribución de la porción RGB original */
    MPI_Scatterv(rank == 0 ? full_input->data : NULL, sendcounts_rgb, displs_rgb, MPI_UNSIGNED_CHAR,
                 local_rgb, local_h * width * 3, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    /* Sincronización antes de medir tiempo de cómputo y comunicación */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    /* Paso 1: Conversión a Grises de la porción local */
    rgb_to_gray(local_rgb, local_gray, width, local_h);

    /* Copiar datos locales al centro del buffer extendido para halos */
    memcpy(local_gray_ext + (size_t)halo_radius * width, local_gray, (size_t)local_h * width);

    /* Intercambio de halos para el suavizado */
    exchange_halos_1d(local_gray_ext, width, local_h, halo_radius, rank, size, rank == 0, rank == size - 1);

    /* Paso 2: Suavizado local */
    if (params.filter_size == 5) {
        local_filter_mean_5x5(local_gray_ext, local_smooth, width, local_h);
    } else {
        local_filter_mean_3x3(local_gray_ext, local_smooth, width, local_h);
    }

    /* Copiar datos suavizados al centro del buffer extendido para gradiente */
    memcpy(local_smooth_ext + (size_t)halo_radius * width, local_smooth, (size_t)local_h * width);

    /* Intercambio de halos para el gradiente */
    exchange_halos_1d(local_smooth_ext, width, local_h, halo_radius, rank, size, rank == 0, rank == size - 1);

    /* Paso 3: Gradiente Sobel local */
    if (params.filter_size == 5) {
        local_sobel_gradient_5x5(local_smooth_ext, local_grad, width, local_h);
    } else {
        local_sobel_gradient_3x3(local_smooth_ext, local_grad, width, local_h);
    }

    /* Paso 4: Umbralado de bordes */
    threshold_edges(local_grad, local_edges, width, local_h, params.threshold);

    /* Paso 5: Posterizado de canales RGB */
    posterize_rgb(local_rgb, local_poster, width, local_h, params.poster_levels);

    /* Paso 6: Fusión Cartoon */
    combine_cartoon(local_poster, local_edges, local_cartoon, width, local_h);

    /* Sincronización y medición de tiempo de ejecución */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;
    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    /* Recolección del resultado final en Rank 0 */
    RGBImage *final_cartoon = NULL;
    if (rank == 0) {
        final_cartoon = create_rgb_image(width, height);
    }

    MPI_Gatherv(local_cartoon, local_h * width * 3, MPI_UNSIGNED_CHAR,
                rank == 0 ? final_cartoon->data : NULL, sendcounts_rgb, displs_rgb, MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf(">>> TIEMPO DE COMPUTO MPI (%d procesos): %.6f segundos (%.2f ms) <<<\n",
               size, max_elapsed, max_elapsed * 1000.0);

        if (save_rgb_image(output_path, final_cartoon) != 0) {
            fprintf(stderr, "[Error Rank 0] No se pudo guardar imagen: %s\n", output_path);
        } else {
            printf("Resultado guardado en %s\n", output_path);
        }

        free_rgb_image(full_input);
        free_rgb_image(final_cartoon);
    }

    /* Liberación de buffers locales */
    free(local_rgb);
    free(local_gray);
    free(local_gray_ext);
    free(local_smooth);
    free(local_smooth_ext);
    free(local_grad);
    free(local_edges);
    free(local_poster);
    free(local_cartoon);
    free(sendcounts);
    free(displs);
    free(sendcounts_rgb);
    free(displs_rgb);

    MPI_Finalize();
    return 0;
}

