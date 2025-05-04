Enter file contents here# ESP32_MQTT_OTA
# ESP32 OTA via MQTT

Ce projet permet de mettre à jour un ESP32 à distance via MQTT (Over-The-Air). Il est conçu de manière modulaire pour faciliter la réutilisation de ses composants dans d'autres projets.

## Fonctionnalités

- Connexion WiFi configurable et persistante
- Communication via protocole MQTT
- Vérification et mise à jour OTA du firmware
- Stockage des configurations dans la mémoire non volatile
- Architecture modulaire pour une meilleure maintenance

## Structure du projet

- `main.ino` - Fichier principal qui inclut les autres modules
- `config.h` - Configuration de base (WiFi, MQTT, versions, etc.)
- `wifi_manager.h/cpp` - Gestion du WiFi
- `mqtt_client.h/cpp` - Gestion du client MQTT
- `ota_updater.h/cpp` - Fonctionnalités de mise à jour OTA
- `storage.h/cpp` - Gestion du stockage des préférences

## Prérequis

### Matériel
- ESP32

### Logiciels
- Arduino IDE
- Bibliothèques Arduino requises :
  - WiFi.h (incluse dans l'ESP32 Core)
  - PubSubClient (pour MQTT)
  - HTTPClient & HTTPUpdate (incluses dans l'ESP32 Core)
  - ArduinoJson
  - Preferences (incluse dans l'ESP32 Core)

## Installation

1. Clonez ce dépôt
2. Ouvrez le projet dans l'Arduino IDE
3. Installez les bibliothèques requises via le gestionnaire de bibliothèques
4. Modifiez les paramètres dans `config.h` selon vos besoins
5. Téléversez le code sur votre ESP32

## Interface web

Une interface web HTML est également disponible pour contrôler l'ESP32 à distance. Elle permet de :
- Se connecter au broker MQTT
- Vérifier et forcer les mises à jour
- Configurer les URLs GitHub pour les fichiers de version et de firmware
- Gérer les réseaux WiFi

## Utilisation

### Commandes MQTT supportées

- `check_update` : Vérifie si une mise à jour est disponible
- `force_update:URL_VERSION:URL_FIRMWARE` : Force une mise à jour avec les URL spécifiées
- `restart` : Redémarre l'ESP32
- `add_wifi:SSID:PASSWORD` : Ajoute ou met à jour un réseau WiFi
- `list_wifi` : Liste les réseaux WiFi enregistrés
- `clear_wifi` : Efface tous les réseaux WiFi enregistrés

## Licence

Ce projet est sous licence MIT

## Auteur

Ali GOGO
