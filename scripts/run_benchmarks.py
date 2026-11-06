#!/usr/bin/env python3
"""
run_benchmarks.py
Automatiza la suite experimental completa para el TPFI de Computación Avanzada (UADER)
usando las imágenes subidas por el usuario:
  - Modelos: Secuencial, OpenMP, MPI, Híbrido (MPI+OpenMP)
  - Resoluciones: 800x800, 2000x2000, 5000x5000
  - Filtros espaciales: 3x3 y 5x5
  - Niveles de posterizado: 3 y 9
  - Configuraciones de concurrencia: 1, 2, 4, 8 núcleos/hilos
Guarda además cada resultado visual Cartoon en formato .png en output/resultados_cartoon/
para que el usuario pueda ver todas las variantes directamente.
"""

import os
import subprocess
import re
import json
import csv
from PIL import Image

WORKSPACE_LINUX = "/mnt/host/c/Users/Zona/Desktop/TPFI - CA"

def run_wsl_cmd(cmd_str):
    full_cmd = f"cd '{WORKSPACE_LINUX}' && {cmd_str}"
    res = subprocess.run(
        ["wsl", "-d", "docker-desktop", "-e", "/bin/sh", "-c", full_cmd],
        capture_output=True,
        text=True
    )
    if res.returncode != 0:
        print(f"[Error ejecutando cmd]: {cmd_str}\nSTDERR: {res.stderr}")
        return None
    return res.stdout

def parse_time(output):
    if not output:
        return None
    match = re.search(r">>> TIEMPO DE COMPUTO[^:]*:\s*([0-9\.]+)\s*s", output)
    if match:
        return float(match.group(1))
    return None

def main():
    os.makedirs("output", exist_ok=True)
    os.makedirs("output/resultados_cartoon", exist_ok=True)

    images = [
        {"name": "800x800",   "path": "data/test_800.ppm"},
        {"name": "2000x2000", "path": "data/test_2000.ppm"},
        {"name": "5000x5000", "path": "data/test_5000.ppm"}
    ]

    filters = [3, 5]
    posters = [3, 9]
    reps = 3

    results = []

    print("================================================================================")
    print(" INICIANDO BATERÍA DE BENCHMARKS CON IMÁGENES DEL USUARIO - TPFI UADER 2026")
    print("================================================================================")

    for img in images:
        img_name = img["name"]
        img_path = img["path"]

        for f_size in filters:
            for p_lvl in posters:
                print(f"\n>>> ESCENARIO: Imagen {img_name} | Filtro {f_size}x{f_size} | Posterizado {p_lvl} niveles <<<")

                # 1. BASELINE SECUENCIAL
                times_sec = []
                for r in range(reps):
                    cmd = f"./bin/secuencial {img_path} output/bench_sec.ppm {f_size} {p_lvl} 70"
                    out = run_wsl_cmd(cmd)
                    t = parse_time(out)
                    if t is not None:
                        times_sec.append(t)
                t_sec_avg = sum(times_sec) / len(times_sec) if times_sec else 0.0
                print(f"  [Secuencial] T_s = {t_sec_avg:.6f} s (corridas: {times_sec})")

                # Guardar el resultado visual en PNG para que el usuario pueda inspeccionarlo
                if os.path.exists("output/bench_sec.ppm"):
                    png_out = f"output/resultados_cartoon/cartoon_{img_name}_filtro{f_size}x{f_size}_poster{p_lvl}.png"
                    Image.open("output/bench_sec.ppm").save(png_out)
                    print(f"  [Imagen Guardada] -> {png_out}")

                results.append({
                    "image": img_name,
                    "filter": f_size,
                    "poster": p_lvl,
                    "paradigm": "Secuencial",
                    "p": 1,
                    "mpi_procs": 1,
                    "omp_threads": 1,
                    "time_sec": t_sec_avg,
                    "speedup": 1.0,
                    "efficiency": 1.0
                })

                # 2. OPENMP (1, 2, 4, 8 threads)
                for threads in [1, 2, 4, 8]:
                    times_omp = []
                    for r in range(reps):
                        cmd = f"./bin/openmp {img_path} output/bench_omp.ppm {threads} {f_size} {p_lvl} 70"
                        out = run_wsl_cmd(cmd)
                        t = parse_time(out)
                        if t is not None:
                            times_omp.append(t)
                    t_omp_avg = sum(times_omp) / len(times_omp) if times_omp else 0.0
                    s_omp = (t_sec_avg / t_omp_avg) if t_omp_avg > 0 else 0.0
                    e_omp = (s_omp / threads) if threads > 0 else 0.0
                    print(f"  [OpenMP {threads}T] T_p = {t_omp_avg:.6f} s | Speedup = {s_omp:.2f}x | Eficiencia = {e_omp*100:.1f}%")

                    results.append({
                        "image": img_name,
                        "filter": f_size,
                        "poster": p_lvl,
                        "paradigm": "OpenMP",
                        "p": threads,
                        "mpi_procs": 1,
                        "omp_threads": threads,
                        "time_sec": t_omp_avg,
                        "speedup": s_omp,
                        "efficiency": e_omp
                    })

                # 3. MPI (1, 2, 4 procesos)
                for procs in [1, 2, 4]:
                    times_mpi = []
                    for r in range(reps):
                        cmd = f"mpirun --allow-run-as-root --mca plm_rsh_agent /bin/true -np {procs} ./bin/mpi {img_path} output/bench_mpi.ppm {f_size} {p_lvl} 70"
                        out = run_wsl_cmd(cmd)
                        t = parse_time(out)
                        if t is not None:
                            times_mpi.append(t)
                    t_mpi_avg = sum(times_mpi) / len(times_mpi) if times_mpi else 0.0
                    s_mpi = (t_sec_avg / t_mpi_avg) if t_mpi_avg > 0 else 0.0
                    e_mpi = (s_mpi / procs) if procs > 0 else 0.0
                    print(f"  [MPI {procs}P] T_p = {t_mpi_avg:.6f} s | Speedup = {s_mpi:.2f}x | Eficiencia = {e_mpi*100:.1f}%")

                    results.append({
                        "image": img_name,
                        "filter": f_size,
                        "poster": p_lvl,
                        "paradigm": "MPI",
                        "p": procs,
                        "mpi_procs": procs,
                        "omp_threads": 1,
                        "time_sec": t_mpi_avg,
                        "speedup": s_mpi,
                        "efficiency": e_mpi
                    })

                # 4. HÍBRIDO MPI + OPENMP
                hybrid_configs = [(1, 2), (2, 2), (2, 4), (4, 2)]
                for p_mpi, t_omp in hybrid_configs:
                    total_cores = p_mpi * t_omp
                    times_hyb = []
                    for r in range(reps):
                        cmd = f"mpirun --allow-run-as-root --mca plm_rsh_agent /bin/true -np {p_mpi} ./bin/mpi_openmp {img_path} output/bench_hyb.ppm {t_omp} {f_size} {p_lvl} 70"
                        out = run_wsl_cmd(cmd)
                        t = parse_time(out)
                        if t is not None:
                            times_hyb.append(t)
                    t_hyb_avg = sum(times_hyb) / len(times_hyb) if times_hyb else 0.0
                    s_hyb = (t_sec_avg / t_hyb_avg) if t_hyb_avg > 0 else 0.0
                    e_hyb = (s_hyb / total_cores) if total_cores > 0 else 0.0
                    print(f"  [Híbrido {p_mpi}P x {t_omp}T ({total_cores} cores)] T_p = {t_hyb_avg:.6f} s | Speedup = {s_hyb:.2f}x | Eficiencia = {e_hyb*100:.1f}%")

                    results.append({
                        "image": img_name,
                        "filter": f_size,
                        "poster": p_lvl,
                        "paradigm": f"Híbrido ({p_mpi}Px{t_omp}T)",
                        "p": total_cores,
                        "mpi_procs": p_mpi,
                        "omp_threads": t_omp,
                        "time_sec": t_hyb_avg,
                        "speedup": s_hyb,
                        "efficiency": e_hyb
                    })

    # Guardar en JSON
    json_path = "output/benchmark_results.json"
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2, ensure_ascii=False)
    print(f"\n[OK] Resultados exportados en formato JSON a {json_path}")

    # Guardar en CSV
    csv_path = "output/benchmark_results.csv"
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["image", "filter", "poster", "paradigm", "p_cores", "mpi_procs", "omp_threads", "time_sec", "speedup", "efficiency"])
        for r in results:
            writer.writerow([
                r["image"], r["filter"], r["poster"], r["paradigm"],
                r["p"], r["mpi_procs"], r["omp_threads"],
                f"{r['time_sec']:.6f}", f"{r['speedup']:.4f}", f"{r['efficiency']:.4f}"
            ])
    print(f"[OK] Resultados exportados en formato CSV a {csv_path}")

if __name__ == "__main__":
    main()
