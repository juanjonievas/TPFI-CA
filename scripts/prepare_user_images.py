#!/usr/bin/env python3
"""
prepare_user_images.py
Verifica las dimensiones de las imágenes subidas por el usuario en data/:
  - '800 x 800.png' -> 800x800
  - '2000 x 2000.png' -> 2000x2000
  - '5000x5000.jpg' -> redimensiona a 5000x5000 (Lanczos)
Y genera las versiones .ppm / .bmp sin compresión para que los binarios en C
(secuencial, openmp, mpi, mpi_openmp) las procesen directamente.
"""

import os
from PIL import Image

def process_image(src_path, target_w, target_h, base_out):
    if not os.path.exists(src_path):
        print(f"[Error] No se encontró: {src_path}")
        return
    
    img = Image.open(src_path).convert("RGB")
    orig_w, orig_h = img.size
    print(f"Archivo: '{src_path}' | Resolución original: {orig_w}x{orig_h}")
    
    if (orig_w, orig_h) != (target_w, target_h):
        print(f"  -> Redimensionando de {orig_w}x{orig_h} a {target_w}x{target_h} (LANCZOS)...")
        img = img.resize((target_w, target_h), Image.Resampling.LANCZOS)
        # Actualizar también el archivo original si era el de 5000x5000
        if "5000" in src_path:
            img.save(src_path, quality=95)
            print(f"  -> Archivo '{src_path}' actualizado a {target_w}x{target_h}.")
    else:
        print(f"  -> Resolución exacta verificada ({target_w}x{target_h}).")
    
    ppm_path = f"data/{base_out}.ppm"
    bmp_path = f"data/{base_out}.bmp"
    img.save(ppm_path)
    img.save(bmp_path)
    print(f"  -> Matrices preparadas para C: '{ppm_path}' y '{bmp_path}'\n")

def main():
    process_image("data/800 x 800.png",   800,  800,  "test_800")
    process_image("data/2000 x 2000.png", 2000, 2000, "test_2000")
    process_image("data/5000x5000.jpg",   5000, 5000, "test_5000")
    print("¡Todas las imágenes del usuario fueron verificadas y preparadas con éxito!")

if __name__ == "__main__":
    main()

