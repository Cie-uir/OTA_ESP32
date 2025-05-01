#!/usr/bin/env python3
"""
Serveur HTTP simple pour les mises à jour OTA de l'ESP32
Ce script crée un serveur web local qui permet à l'ESP32 de télécharger le firmware
"""

import http.server
import socketserver
import os
import socket
import argparse
import threading
import time
import webbrowser
from urllib.parse import urlparse, parse_qs

# Configuration par défaut
PORT = 8080
firmware_path = None
update_in_progress = False
last_progress = 0
total_bytes = 0
transferred_bytes = 0

# Obtenir l'adresse IP locale
def get_local_ip():
    try:
        # Méthode simple pour obtenir l'IP locale
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        # Fallback si la méthode ci-dessus échoue
        hostname = socket.gethostname()
        return socket.gethostbyname(hostname)

# Gestionnaire HTTP personnalisé
class OTARequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        global firmware_path, update_in_progress, last_progress, total_bytes, transferred_bytes
        
        # Analyser l'URL
        parsed_url = urlparse(self.path)
        
        # Page d'accueil
        if parsed_url.path == "/" or parsed_url.path == "/index.html":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            
            # Lire la taille du firmware
            firmware_size = os.path.getsize(firmware_path) if firmware_path else 0
            
            # Créer une page HTML simple
            html = """
            <!DOCTYPE html>
            <html>
            <head>
                <title>ESP32 OTA Update Server</title>
                <style>
                    body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
                    h1 { color: #333; }
                    .info { background-color: #f0f0f0; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
                    pre { background-color: #f5f5f5; padding: 10px; border-radius: 3px; }
                    .button { display: inline-block; background-color: #4CAF50; color: white; padding: 10px 20px; 
                             text-align: center; text-decoration: none; font-size: 16px; border-radius: 5px; 
                             cursor: pointer; margin-top: 10px; }
                    .progress { width: 100%; background-color: #ddd; border-radius: 3px; margin-top: 20px; }
                    .progress-bar { height: 20px; background-color: #4CAF50; width: 0%; border-radius: 3px; text-align: center; color: white; }
                </style>
                <script>
                    function checkProgress() {
                        fetch('/progress')
                            .then(response => response.json())
                            .then(data => {
                                let percent = data.progress;
                                document.getElementById('progress-bar').style.width = percent + '%';
                                document.getElementById('progress-text').innerText = percent + '%';
                                
                                if (data.in_progress) {
                                    setTimeout(checkProgress, 1000);
                                }
                            });
                    }
                </script>
            </head>
            <body>
                <h1>ESP32 OTA Update Server</h1>
                
                <div class="info">
                    <h2>Informations</h2>
                    <p><strong>Serveur HTTP:</strong> http://{0}:{1}</p>
                    <p><strong>Firmware:</strong> {2}</p>
                    <p><strong>Taille:</strong> {3} octets ({4:.1f} KB)</p>
                </div>
                
                <h2>Instructions</h2>
                <p>Pour mettre à jour votre ESP32, utilisez l'URL suivante dans votre code:</p>
                <pre>http://{0}:{1}/firmware.bin</pre>
                
                <h2>Test</h2>
                <p>Vous pouvez tester le téléchargement du firmware en cliquant sur le bouton ci-dessous:</p>
                <a href="/firmware.bin" class="button">Télécharger le firmware</a>
                
                <div class="progress">
                    <div id="progress-bar" class="progress-bar">
                        <span id="progress-text">0%</span>
                    </div>
                </div>
                
                <script>
                    // Vérifier la progression au chargement
                    checkProgress();
                </script>
            </body>
            </html>
            """.format(
                get_local_ip(),
                PORT,
                os.path.basename(firmware_path) if firmware_path else "Non spécifié",
                firmware_size,
                firmware_size/1024.0
            )
            
            self.wfile.write(html.encode())
            return
        
        # Suivi de la progression
        elif parsed_url.path == "/progress":
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            
            progress = 0
            if total_bytes > 0:
                progress = int((transferred_bytes / total_bytes) * 100)
            
            json_data = '{"in_progress": %s, "progress": %d}' % ("true" if update_in_progress else "false", progress)
            self.wfile.write(json_data.encode())
            return
        
        # Téléchargement du firmware
        elif parsed_url.path == "/firmware.bin":
            if not firmware_path:
                self.send_error(404, "Firmware not found")
                return
            
            try:
                # Ouvrir le fichier en mode binaire
                with open(firmware_path, 'rb') as file:
                    # Obtenir la taille du fichier
                    file_size = os.path.getsize(firmware_path)
                    
                    # Envoyer les en-têtes HTTP
                    self.send_response(200)
                    self.send_header("Content-type", "application/octet-stream")
                    self.send_header("Content-Disposition", f"attachment; filename=firmware.bin")
                    self.send_header("Content-Length", str(file_size))
                    self.end_headers()
                    
                    # Initialiser les variables de suivi
                    global update_in_progress, total_bytes, transferred_bytes, last_progress
                    update_in_progress = True
                    total_bytes = file_size
                    transferred_bytes = 0
                    last_progress = 0
                    
                    print(f"Envoi du firmware ({file_size} octets)...")
                    
                    # Envoyer le fichier par morceaux
                    chunk_size = 8192
                    while True:
                        chunk = file.read(chunk_size)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        
                        # Mettre à jour le suivi de progression
                        transferred_bytes += len(chunk)
                        progress = int((transferred_bytes / total_bytes) * 100)
                        
                        # Afficher la progression sur la console tous les 10%
                        if progress >= last_progress + 10:
                            last_progress = progress - (progress % 10)
                            print(f"Progression: {progress}%")
                    
                    print("Firmware envoyé avec succès!")
                    update_in_progress = False
                    return
                
            except Exception as e:
                self.send_error(500, f"Internal Server Error: {str(e)}")
                update_in_progress = False
                return
        
        # Gestion des autres requêtes
        else:
            super().do_GET()
    
    def log_message(self, format, *args):
        # Supprimer les logs pour les requêtes de progression
        if "progress" not in args[0]:
            super().log_message(format, *args)

# Fonction principale
def main():
    global firmware_path, PORT
    
    # Parser pour les arguments de ligne de commande
    parser = argparse.ArgumentParser(description="Serveur HTTP pour les mises à jour OTA de l'ESP32")
    parser.add_argument("firmware", help="Chemin vers le fichier binaire du firmware (.bin)")
    parser.add_argument("--port", type=int, default=PORT, help=f"Port du serveur HTTP (défaut: {PORT})")
    
    args = parser.parse_args()
    
    # Mettre à jour les variables globales
    firmware_path = args.firmware
    PORT = args.port
    
    # Vérifier si le fichier existe
    if not os.path.exists(args.firmware):
        print(f"Erreur: Le fichier {args.firmware} n'existe pas!")
        return
    
    # Créer un serveur HTTP
    handler = OTARequestHandler
    httpd = socketserver.TCPServer(("", args.port), handler)
    
    # Obtenir l'adresse IP locale
    local_ip = get_local_ip()
    
    print(f"=== Serveur HTTP OTA pour ESP32 ===")
    print(f"Serveur démarré à l'adresse http://{local_ip}:{args.port}")
    print(f"Firmware: {os.path.basename(args.firmware)} ({os.path.getsize(args.firmware)} octets)")
    print(f"Dans votre code ESP32, utilisez: http://{local_ip}:{args.port}/firmware.bin")
    print(f"Pour la documentation, visitez: http://{local_ip}:{args.port}/")
    print("Appuyez sur Ctrl+C pour arrêter le serveur")
    print("===================================")
    
    # Ouvrir le navigateur avec la page d'accueil
    try:
        webbrowser.open(f"http://{local_ip}:{args.port}/")
    except:
        print("Impossible d'ouvrir automatiquement le navigateur.")
        print(f"Veuillez visiter manuellement http://{local_ip}:{args.port}/")
    
    try:
        # Démarrer le serveur
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nServeur arrêté.")
    finally:
        httpd.server_close()

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Erreur: {str(e)}")
