/**
 * @file mpi_openmp.c
 * @brief Implementación Paralela Híbrida (MPI + OpenMP) de la Pipeline de Cartoons.
 *        Combina pasaje de mensajes entre nodos (MPI) con multithreading en memoria
 *        compartida (OpenMP) en cada proceso para maximizar el uso de los núcleos.
 * 
 * Computación Avanzada - UADER (2026)
 */

#include "image.h"
#include "cartoon_ops.h"
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline int clamp_coord(int pos, int max_val) {
    if (pos < 0) return 0;
    if (pos >= max_val) return max_val - 1;
    return pos;
}

static void exchange_halos_1d(unsigned char *local_buf, int width, int local_h, int halo_radius,
                              int rank, int size, int is_top_boundary, int is_bottom_boundary) {
    int top_neighbor = (rank > 0) ? rank - 1 : MPI_PROC_NULL;
    int bot_neighbor = (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;

    unsigned char *send_top = local_buf + (size_t)halo_radius * width;
    unsigned char *recv_top = local_buf;
    unsigned char *send_bot = local_buf + (size_t)(halo_radius + local_h - halo_radius) * width;
    unsigned char *recv_bot = local_buf + (size_t)(halo_radius + local_h) * width;

    int halo_count = halo_radius * width;
    MPI_Request reqs[4];
    int req_count = 0;

    if (top_neighbor != MPI_PROC_NULL) {
        MPI_Irecv(recv_top, halo_count, MPI_UNSIGNED_CHAR, top_neighbor, 101, MPI_COMM_WORLD, &reqs[req_count++]);
        MPI_Isend(send_top, halo_count, MPI_UNSIGNED_CHAR, top_neighbor, 201, MPI_COMM_WORLD, &reqs[req_count++]);
    }

    if (bot_neighbor != MPI_PROC_NULL) {
        MPI_Irecv(recv_bot, halo_count, MPI_UNSIGNED_CHAR, bot_neighbor, 201, MPI_COMM_WORLD, &reqs[req_count++]);
        MPI_Isend(send_bot, halo_count, MPI_UNSIGNED_CHAR, bot_neighbor, 101, MPI_COMM_WORLD, &reqs[req_count++]);
    }

    if (req_count > 0) {
        MPI_Waitall(req_count, reqs, MPI_STATUSES_IGNORE);
    }

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

/* 1. Conversión Grises híbrida */
static void hybrid_rgb_to_gray(const unsigned char *rgb, unsigned char *gray, int width, int local_h) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < local_h; y++) {
        size_t row_off = (size_t)y * width;
        for (int x = 0; x < width; x++) {
            size_t idx = row_off + x;
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

/* 2. Suavizado 3x3 híbrido */
static void hybrid_filter_mean_3x3(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
    int halo = 1;
    #pragma omp parallel for schedule(static)
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

/* 2b. Suavizado 5x5 híbrido */
static void hybrid_filter_mean_5x5(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
    int halo = 2;
    #pragma omp parallel for schedule(static)
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

/* 3. Gradiente Sobel 3x3 híbrido */
static void hybrid_sobel_gradient_3x3(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
    int halo = 1;
    #pragma omp parallel for schedule(static)
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

/* 3b. Gradiente Sobel 5x5 híbrido */
static void hybrid_sobel_gradient_5x5(const unsigned char *src_ext, unsigned char *dst_local, int width, int local_h) {
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

    #pragma omp parallel for schedule(static)
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

/* 4. Umbralado híbrido */
static void hybrid_threshold_edges(const unsigned char *grad, unsigned char *edges, int width, int local_h, int threshold) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < local_h; y++) {
        size_t off = (size_t)y * width;
        for (int x = 0; x < width; x++) {
            edges[off + x] = (grad[off + x] >= threshold) ? 255 : 0;
        }
    }
}

/* 5. Posterizado híbrido */
static void hybrid_posterize_rgb(const unsigned char *src, unsigned char *dst, int width, int local_h, int levels) {
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
    for (int y = 0; y < local_h; y++) {
        size_t off = (size_t)y * width * 3;
        for (int x = 0; x < width * 3; x++) {
            dst[off + x] = lut[src[off + x]];
        }
    }
}

/* 6. Fusión Cartoon híbrida */
static void hybrid_combine_cartoon(const unsigned char *poster, const unsigned char *edges, unsigned char *cartoon, int width, int local_h) {
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < local_h; y++) {
        size_t p_off = (size_t)y * width;
        size_t c_off = p_off * 3;
        for (int x = 0; x < width; x++) {
            if (edges[p_off + x] == 255) {
                cartoon[c_off + x * 3 + 0] = 0;
                cartoon[c_off + x * 3 + 1] = 0;
                cartoon[c_off + x * 3 + 2] = 0;
            } else {
                cartoon[c_off + x * 3 + 0] = poster[c_off + x * 3 + 0];
                cartoon[c_off + x * 3 + 1] = poster[c_off + x * 3 + 1];
                cartoon[c_off + x * 3 + 2] = poster[c_off + x * 3 + 2];
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) {
            printf("Uso: mpirun -np <P> %s <imagen_entrada> <imagen_salida> [threads=2] [filtro=3|5] [poster=3|9] [umbral=70]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    int num_threads = 2;
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

    int width = 0, height = 0;
    RGBImage *full_input = NULL;

    if (rank == 0) {
        printf("============================================================\n");
        printf("  TPFI - Computacion Avanzada (UADER)\n");
        printf("  Version: HIBRIDA (MPI + OpenMP)\n");
        printf("  Procesos MPI:        %d\n", size);
        printf("  Threads por proceso: %d (Total hilos de computo: %d)\n", actual_threads, size * actual_threads);
        printf("  Entrada:             %s\n", input_path);
        printf("  Salida:              %s\n", output_path);
        printf("  Filtro:              %dx%d\n", params.filter_size, params.filter_size);
        printf("  Posterizado:         %d niveles\n", params.poster_levels);
        printf("  Umbral:              %d\n", params.threshold);
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

    /* Broadcast de metadata */
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

    /* Particionado de filas */
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

    /* Buffers locales */
    unsigned char *local_rgb = (unsigned char*)malloc((size_t)local_h * width * 3);
    unsigned char *local_gray = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_gray_ext = (unsigned char*)malloc((size_t)(local_h + 2 * halo_radius) * width);
    unsigned char *local_smooth = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_smooth_ext = (unsigned char*)malloc((size_t)(local_h + 2 * halo_radius) * width);
    unsigned char *local_grad = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_edges = (unsigned char*)malloc((size_t)local_h * width);
    unsigned char *local_poster = (unsigned char*)malloc((size_t)local_h * width * 3);
    unsigned char *local_cartoon = (unsigned char*)malloc((size_t)local_h * width * 3);

    MPI_Scatterv(rank == 0 ? full_input->data : NULL, sendcounts_rgb, displs_rgb, MPI_UNSIGNED_CHAR,
                 local_rgb, local_h * width * 3, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    /* 1. Grises híbrido */
    hybrid_rgb_to_gray(local_rgb, local_gray, width, local_h);

    /* Preparar halos y comunicar */
    memcpy(local_gray_ext + (size_t)halo_radius * width, local_gray, (size_t)local_h * width);
    exchange_halos_1d(local_gray_ext, width, local_h, halo_radius, rank, size, rank == 0, rank == size - 1);

    /* 2. Suavizado híbrido */
    if (params.filter_size == 5) {
        hybrid_filter_mean_5x5(local_gray_ext, local_smooth, width, local_h);
    } else {
        hybrid_filter_mean_3x3(local_gray_ext, local_smooth, width, local_h);
    }

    /* Preparar halos para gradiente y comunicar */
    memcpy(local_smooth_ext + (size_t)halo_radius * width, local_smooth, (size_t)local_h * width);
    exchange_halos_1d(local_smooth_ext, width, local_h, halo_radius, rank, size, rank == 0, rank == size - 1);

    /* 3. Gradiente híbrido */
    if (params.filter_size == 5) {
        hybrid_sobel_gradient_5x5(local_smooth_ext, local_grad, width, local_h);
    } else {
        hybrid_sobel_gradient_3x3(local_smooth_ext, local_grad, width, local_h);
    }

    /* 4. Umbralado híbrido */
    hybrid_threshold_edges(local_grad, local_edges, width, local_h, params.threshold);

    /* 5. Posterizado híbrido */
    hybrid_posterize_rgb(local_rgb, local_poster, width, local_h, params.poster_levels);

    /* 6. Fusión Cartoon híbrida */
    hybrid_combine_cartoon(local_poster, local_edges, local_cartoon, width, local_h);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_end = MPI_Wtime();
    double local_elapsed = t_end - t_start;
    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    RGBImage *final_cartoon = NULL;
    if (rank == 0) {
        final_cartoon = create_rgb_image(width, height);
    }

    MPI_Gatherv(local_cartoon, local_h * width * 3, MPI_UNSIGNED_CHAR,
                rank == 0 ? final_cartoon->data : NULL, sendcounts_rgb, displs_rgb, MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf(">>> TIEMPO DE COMPUTO HIBRIDO (MPI %d x OpenMP %d = %d cores): %.6f s (%.2f ms) <<<\n",
               size, actual_threads, size * actual_threads, max_elapsed, max_elapsed * 1000.0);

        if (save_rgb_image(output_path, final_cartoon) != 0) {
            fprintf(stderr, "[Error Rank 0] No se pudo guardar imagen: %s\n", output_path);
        } else {
            printf("Resultado guardado en %s\n", output_path);
        }

        free_rgb_image(full_input);
        free_rgb_image(final_cartoon);
    }

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

