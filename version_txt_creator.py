#!/usr/bin/env python3
"""
Utilitaire pour créer un fichier version.txt
Ce script vous aide à créer le fichier de version requis pour le système OTA amélioré
"""

import os
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description="Créer un fichier version.txt pour le système OTA ESP32")
    parser.add_argument("version", help="Numéro de version au format x.y.z (ex: 1.0.1)")
    parser.add_argument("--output", default="version.txt", help="Nom du fichier de sortie (défaut: version.txt)")
    
    args = parser.parse_args()
    
    # Vérifier le format de la version
    if not check_version_format(args.version):
        print("Erreur: Le format de version doit être x.y.z (ex: 1.0.1)")
        return 1
    
    # Créer le fichier de version
    try:
        with open(args.output, 'w') as f:
            f.write(args.version.strip())
        
        print(f"Fichier version.txt créé avec la version {args.version}")
        print(f"Placez ce fichier dans le même dossier que votre firmware.bin")
        
        filesize = os.path.getsize(args.output)
        print(f"Taille du fichier: {filesize} octets")
        
        return 0
    except Exception as e:
        print(f"Erreur lors de la création du fichier: {str(e)}")
        return 1

def check_version_format(version):
    """Vérifie si le format de version est correct (x.y.z)"""
    parts = version.split('.')
    if len(parts) != 3:
        return False
    
    for part in parts:
        if not part.isdigit():
            return False
    
    return True

if __name__ == "__main__":
    sys.exit(main())
