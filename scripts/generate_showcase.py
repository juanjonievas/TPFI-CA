#!/usr/bin/env python3
"""
generate_showcase.py
Genera láminas comparativas para cada una de las 3 imágenes subidas por el usuario
(800x800, 2000x2000 y 5000x5000) mostrando la imagen original junto con las
variaciones de filtro (3x3 y 5x5) y posterizado (3 y 9 niveles).
"""

import os
from PIL import Image
import matplotlib.pyplot as plt
import numpy as np

def create_showcase_for_res(res_name, orig_path):
    orig = Image.open(orig_path)
    arr = np.array(orig)
    r, g, b = arr[:,:,0].astype(float), arr[:,:,1].astype(float), arr[:,:,2].astype(float)
    gray = np.clip(0.30*r + 0.59*g + 0.11*b, 0, 255).astype(np.uint8)

    c_f3_p3 = Image.open(f"output/resultados_cartoon/cartoon_{res_name}_filtro3x3_poster3.png")
    c_f3_p9 = Image.open(f"output/resultados_cartoon/cartoon_{res_name}_filtro3x3_poster9.png")
    c_f5_p3 = Image.open(f"output/resultados_cartoon/cartoon_{res_name}_filtro5x5_poster3.png")
    c_f5_p9 = Image.open(f"output/resultados_cartoon/cartoon_{res_name}_filtro5x5_poster9.png")

    fig, axes = plt.subplots(2, 3, figsize=(15, 10), dpi=220)
    fig.suptitle(f"Resultados de Generación de Cartoon — Imagen {res_name} (Usuario)", fontsize=15, fontweight='bold')

    axes[0, 0].imshow(orig)
    axes[0, 0].set_title(f"(a) Original RGB ({res_name})", fontweight='bold')
    axes[0, 0].axis('off')

    axes[0, 1].imshow(c_f3_p3)
    axes[0, 1].set_title("(b) Cartoon: Filtro 3x3 | Posterizado 3 Niveles", fontweight='bold')
    axes[0, 1].axis('off')

    axes[0, 2].imshow(c_f3_p9)
    axes[0, 2].set_title("(c) Cartoon: Filtro 3x3 | Posterizado 9 Niveles", fontweight='bold')
    axes[0, 2].axis('off')

    axes[1, 0].imshow(gray, cmap='gray')
    axes[1, 0].set_title("(d) Canal de Luminancia (Escala de Grises)", fontweight='bold')
    axes[1, 0].axis('off')

    axes[1, 1].imshow(c_f5_p3)
    axes[1, 1].set_title("(e) Cartoon: Filtro 5x5 | Posterizado 3 Niveles", fontweight='bold')
    axes[1, 1].axis('off')

    axes[1, 2].imshow(c_f5_p9)
    axes[1, 2].set_title("(f) Cartoon: Filtro 5x5 | Posterizado 9 Niveles", fontweight='bold')
    axes[1, 2].axis('off')

    plt.tight_layout()
    out_file = f"output/plots/comparativa_{res_name}.png"
    plt.savefig(out_file)
    plt.close()
    print(f"[OK] Lámina comparativa guardada en: {out_file}")

def main():
    os.makedirs("output/plots", exist_ok=True)
    create_showcase_for_res("800x800",   "data/test_800.ppm")
    create_showcase_for_res("2000x2000", "data/test_2000.ppm")
    create_showcase_for_res("5000x5000", "data/test_5000.ppm")

    # También guardar pipeline_etapas.png con la imagen del usuario para el informe
    orig = Image.open("data/test_800.ppm")
    arr = np.array(orig)
    r, g, b = arr[:,:,0].astype(float), arr[:,:,1].astype(float), arr[:,:,2].astype(float)
    gray = np.clip(0.30*r + 0.59*g + 0.11*b, 0, 255).astype(np.uint8)
    c_f3_p3 = Image.open("output/resultados_cartoon/cartoon_800x800_filtro3x3_poster3.png")
    c_f3_p9 = Image.open("output/resultados_cartoon/cartoon_800x800_filtro3x3_poster9.png")

    fig, axes = plt.subplots(1, 4, figsize=(16, 4.5), dpi=250)
    axes[0].imshow(orig)
    axes[0].set_title("(a) Imagen Original RGB", fontsize=11, fontweight='bold')
    axes[0].axis('off')

    axes[1].imshow(gray, cmap='gray')
    axes[1].set_title("(b) Escala de Grises", fontsize=11, fontweight='bold')
    axes[1].axis('off')

    axes[2].imshow(c_f3_p3)
    axes[2].set_title("(c) Cartoon (Filtro 3x3, Poster 3)", fontsize=11, fontweight='bold')
    axes[2].axis('off')

    axes[3].imshow(c_f3_p9)
    axes[3].set_title("(d) Cartoon (Filtro 3x3, Poster 9)", fontsize=11, fontweight='bold')
    axes[3].axis('off')

    plt.tight_layout()
    plt.savefig("output/plots/pipeline_etapas.png")
    plt.close()
    print("[OK] Showcase principal actualizado en output/plots/pipeline_etapas.png")

if __name__ == "__main__":
    main()

