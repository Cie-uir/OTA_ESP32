import http.server
import socketserver
import socket
import os

# Configuration du port
PORT = 8080

# Obtenir l'adresse IP locale
def get_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP

# Créer le gestionnaire HTTP
Handler = http.server.SimpleHTTPRequestHandler

# Créer le serveur
with socketserver.TCPServer(("", PORT), Handler) as httpd:
    ip = get_ip()
    print(f"=== Serveur OTA ESP32 ===")
    print(f"Serveur démarré: http://{ip}:{PORT}")
    print(f"URL du firmware: http://{ip}:{PORT}/firmware.bin")
    
    # Vérifier si le firmware existe
    if os.path.exists("firmware.bin"):
        print(f"Taille du firmware: {os.path.getsize('firmware.bin')} octets")
    else:
        print("ATTENTION: firmware.bin non trouvé dans ce dossier!")
    
    print(f"Dans le code ESP32, utilisez cette adresse: {ip}")
    print("Appuyez sur Ctrl+C pour arrêter le serveur")
    print("=========================")
    
    try:
        # Démarrer le serveur
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nServeur arrêté.")
