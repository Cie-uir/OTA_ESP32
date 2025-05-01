/*
  Programme ESP32 OTA via GitHub - Multi-réseaux
  Ce programme permet la connexion à différents réseaux WiFi
  et la mise à jour OTA via GitHub
*/

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Version du firmware - À MODIFIER pour chaque nouvelle version
const String FIRMWARE_VERSION = "1.0.0";  // Format: majeur.mineur.patch
const String FIRMWARE_NAME = "coucou";    // Nom du firmware pour l'identification

// Liens GitHub - À MODIFIER avec vos liens "raw"
const String VERSION_JSON_URL = "https://raw.githubusercontent.com/Cie-uir/OTA_ESP32/main/version.json";
const String FIRMWARE_BIN_URL = "https://raw.githubusercontent.com/Cie-uir/OTA_ESP32/main/firmware.bin";

// Structure pour stocker les configurations WiFi
struct WiFiConfig {
  char ssid[32];
  char password[64];
};

// Nombre maximum de réseaux WiFi à stocker
const int MAX_WIFI_NETWORKS = 5;

// Configuration des réseaux WiFi
WiFiMulti wifiMulti;
WiFiConfig wifiNetworks[MAX_WIFI_NETWORKS];
int numNetworks = 0;

// Préférences pour stocker les configurations
Preferences preferences;

// Intervalle d'affichage du message
const long messageInterval = 5000;         // Afficher message toutes les 5 secondes
unsigned long previousMessageMillis = 0;

// Intervalle de vérification des mises à jour
const long updateCheckInterval = 3600000;  // Vérifier les mises à jour toutes les heures
unsigned long previousUpdateCheckMillis = 0;
const long initialUpdateDelay = 30000;     // Délai initial pour la première vérification
bool initialUpdateCheckDone = false;

// Variables pour la mise à jour OTA
bool otaInProgress = false;
int updateCheckRetries = 0;
const int maxUpdateCheckRetries = 3;

// Adresses EEPROM
const int EEPROM_SIZE = 16;
const int REBOOT_COUNT_ADDR = 0;
const int UPDATE_FLAG_ADDR = 4;

// Fonctions de gestion des configurations WiFi
void loadWifiConfigurations() {
  preferences.begin("wifi-config", false);
  
  // Charger le nombre de réseaux
  numNetworks = preferences.getInt("numNetworks", 0);
  if (numNetworks > MAX_WIFI_NETWORKS) numNetworks = MAX_WIFI_NETWORKS;
  
  // Charger chaque configuration WiFi
  for (int i = 0; i < numNetworks; i++) {
    char keySSID[16];
    char keyPass[16];
    sprintf(keySSID, "ssid%d", i);
    sprintf(keyPass, "pass%d", i);
    
    String ssid = preferences.getString(keySSID, "");
    String pass = preferences.getString(keyPass, "");
    
    if (ssid.length() > 0) {
      strncpy(wifiNetworks[i].ssid, ssid.c_str(), sizeof(wifiNetworks[i].ssid) - 1);
      strncpy(wifiNetworks[i].password, pass.c_str(), sizeof(wifiNetworks[i].password) - 1);
      
      // Ajouter à WiFiMulti
      wifiMulti.addAP(wifiNetworks[i].ssid, wifiNetworks[i].password);
      
      Serial.print("Réseau WiFi chargé: ");
      Serial.println(wifiNetworks[i].ssid);
    }
  }
  
  preferences.end();
}

void saveWifiConfiguration(const char* ssid, const char* password) {
  // Vérifier si ce réseau existe déjà
  for (int i = 0; i < numNetworks; i++) {
    if (strcmp(wifiNetworks[i].ssid, ssid) == 0) {
      // Mettre à jour le mot de passe
      strncpy(wifiNetworks[i].password, password, sizeof(wifiNetworks[i].password) - 1);
      
      // Sauvegarder dans les préférences
      preferences.begin("wifi-config", false);
      char keyPass[16];
      sprintf(keyPass, "pass%d", i);
      preferences.putString(keyPass, password);
      preferences.end();
      
      Serial.print("Réseau WiFi mis à jour: ");
      Serial.println(ssid);
      return;
    }
  }
  
  // Si on a atteint le nombre max de réseaux, remplacer le plus ancien
  if (numNetworks >= MAX_WIFI_NETWORKS) {
    // Décaler tous les réseaux
    for (int i = 0; i < MAX_WIFI_NETWORKS - 1; i++) {
      memcpy(&wifiNetworks[i], &wifiNetworks[i+1], sizeof(WiFiConfig));
    }
    numNetworks = MAX_WIFI_NETWORKS - 1;
  }
  
  // Ajouter le nouveau réseau
  strncpy(wifiNetworks[numNetworks].ssid, ssid, sizeof(wifiNetworks[numNetworks].ssid) - 1);
  strncpy(wifiNetworks[numNetworks].password, password, sizeof(wifiNetworks[numNetworks].password) - 1);
  
  // Ajouter à WiFiMulti
  wifiMulti.addAP(ssid, password);
  
  // Sauvegarder dans les préférences
  preferences.begin("wifi-config", false);
  
  numNetworks++;
  preferences.putInt("numNetworks", numNetworks);
  
  char keySSID[16];
  char keyPass[16];
  sprintf(keySSID, "ssid%d", numNetworks - 1);
  sprintf(keyPass, "pass%d", numNetworks - 1);
  
  preferences.putString(keySSID, ssid);
  preferences.putString(keyPass, password);
  
  preferences.end();
  
  Serial.print("Nouveau réseau WiFi ajouté: ");
  Serial.println(ssid);
}

// Fonction pour se connecter au WiFi en utilisant WiFiMulti
bool connectWiFi() {
  Serial.println("Tentative de connexion WiFi...");
  
  // Tenter de se connecter avec timeout
  unsigned long startAttemptTime = millis();
  
  while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connecté");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("Adresse IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("");
    Serial.println("Échec de connexion WiFi. Fonctionnement en mode hors ligne.");
    return false;
  }
}

// Vérifier et appliquer les mises à jour OTA
void checkForUpdates() {
  // Ne vérifier que si le WiFi est connecté
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Pas de connexion WiFi. Impossible de vérifier les mises à jour.");
    return;
  }

  Serial.println("Vérification des mises à jour...");
  
  WiFiClientSecure client;
  client.setInsecure(); // Désactiver la vérification des certificats pour simplifier
  
  HTTPClient http;
  Serial.print("URL de vérification de version: ");
  Serial.println(VERSION_JSON_URL);
  
  // Première étape: vérifier si une nouvelle version est disponible
  http.begin(client, VERSION_JSON_URL);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Analyser le JSON de version
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String newVersion = doc["version"].as<String>();
      
      Serial.print("Version actuelle: ");
      Serial.println(FIRMWARE_VERSION);
      Serial.print("Version disponible: ");
      Serial.println(newVersion);
      
      // Comparer les versions (si version distante est différente et non vide)
      if (newVersion.length() > 0 && newVersion != FIRMWARE_VERSION) {
        Serial.println("Nouvelle version disponible. Téléchargement du firmware...");
        http.end();
        
        // Télécharger et installer le nouveau firmware
        downloadAndInstallUpdate();
      } else {
        Serial.println("Déjà à jour avec la dernière version.");
        http.end();
        updateCheckRetries = 0;  // Réinitialiser le compteur d'essais
      }
    } else {
      Serial.print("Erreur d'analyse JSON: ");
      Serial.println(error.c_str());
      http.end();
    }
  } else {
    Serial.print("Erreur lors de la vérification de la version, code: ");
    Serial.println(httpCode);
    http.end();
    
    // Incrémenter le compteur d'essais
    updateCheckRetries++;
    if (updateCheckRetries >= maxUpdateCheckRetries) {
      Serial.println("Nombre maximum d'essais atteint. Abandon des vérifications de mise à jour.");
      updateCheckRetries = 0;
    }
  }
}

// Télécharger et installer une mise à jour
void downloadAndInstallUpdate() {
  Serial.print("URL de téléchargement du firmware: ");
  Serial.println(FIRMWARE_BIN_URL);
  
  WiFiClientSecure client;
  client.setInsecure(); // Désactiver la vérification des certificats pour simplifier
  
  HTTPClient http;
  http.begin(client, FIRMWARE_BIN_URL);
  
  // Obtenir la taille du fichier d'abord avec une requête HEAD
  int httpCode = http.sendRequest("HEAD");
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Erreur lors de la vérification du firmware, code: ");
    Serial.println(httpCode);
    http.end();
    return;
  }
  
  // Obtenir la taille du firmware
  int contentLength = http.getSize();
  Serial.print("Taille du firmware: ");
  Serial.print(contentLength);
  Serial.println(" octets");
  
  // Vérifier si le téléchargement est possible
  if (contentLength <= 0) {
    Serial.println("Taille de firmware invalide");
    http.end();
    return;
  }
  
  // Définir le flag de mise à jour dans l'EEPROM
  EEPROM.write(UPDATE_FLAG_ADDR, 1);
  EEPROM.commit();
  
  // Préparer la mise à jour
  http.end();
  http.begin(client, FIRMWARE_BIN_URL);
  
  // Envoyer une requête GET pour télécharger le firmware
  httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Erreur lors du téléchargement du firmware, code: ");
    Serial.println(httpCode);
    http.end();
    
    // Réinitialiser le flag de mise à jour
    EEPROM.write(UPDATE_FLAG_ADDR, 0);
    EEPROM.commit();
    return;
  }
  
  // Obtenir le flux de données
  WiFiClient* stream = http.getStreamPtr();
  
  // Préparer la mise à jour
  if (!Update.begin(contentLength)) {
    Serial.println("Pas assez d'espace pour la mise à jour");
    Serial.print("Erreur: ");
    Serial.println(Update.errorString());
    http.end();
    
    // Réinitialiser le flag de mise à jour
    EEPROM.write(UPDATE_FLAG_ADDR, 0);
    EEPROM.commit();
    return;
  }
  
  Serial.println("Début de la mise à jour OTA...");
  otaInProgress = true;
  
  // Variables pour la progression
  size_t written = 0;
  size_t prevProgress = 0;
  const size_t bufSize = 1024;
  uint8_t buf[bufSize];
  
  // Télécharger et écrire le firmware par morceaux
  while (http.connected() && (written < contentLength)) {
    // Lire un morceau
    size_t available = stream->available();
    if (available) {
      size_t toRead = min(available, bufSize);
      size_t bytesRead = stream->readBytes(buf, toRead);
      
      // Écrire dans le flash
      if (Update.write(buf, bytesRead) != bytesRead) {
        Serial.println("Erreur d'écriture");
        Serial.print("Erreur: ");
        Serial.println(Update.errorString());
        otaInProgress = false;
        
        // Réinitialiser le flag de mise à jour
        EEPROM.write(UPDATE_FLAG_ADDR, 0);
        EEPROM.commit();
        
        http.end();
        return;
      }
      
      written += bytesRead;
      
      // Afficher la progression à intervalles de 10%
      size_t progress = (written * 100) / contentLength;
      if (progress >= prevProgress + 10) {
        prevProgress = progress - (progress % 10);
        Serial.print("Progression: ");
        Serial.print(progress);
        Serial.println("%");
      }
    }
    // Petite pause pour éviter de surcharger l'ESP32
    delay(1);
  }
  
  // Finaliser la mise à jour
  if (written == contentLength) {
    Serial.println("Tous les octets reçus, finalisation de la mise à jour...");
    if (Update.end(true)) {
      Serial.println("Mise à jour OTA réussie");
      Serial.println("Redémarrage dans 3 secondes...");
      
      // Incrémenter le compteur de redémarrage
      int rebootCount = EEPROM.read(REBOOT_COUNT_ADDR);
      EEPROM.write(REBOOT_COUNT_ADDR, rebootCount + 1);
      EEPROM.commit();
      
      delay(3000);
      ESP.restart();
    } else {
      Serial.println("Erreur de mise à jour OTA lors de la finalisation");
      Serial.print("Erreur: ");
      Serial.println(Update.errorString());
      
      // Réinitialiser le flag de mise à jour
      EEPROM.write(UPDATE_FLAG_ADDR, 0);
      EEPROM.commit();
    }
  } else {
    Serial.print("Octets reçus incomplets: ");
    Serial.print(written);
    Serial.print("/");
    Serial.println(contentLength);
    Update.abort();
    
    // Réinitialiser le flag de mise à jour
    EEPROM.write(UPDATE_FLAG_ADDR, 0);
    EEPROM.commit();
  }
  
  otaInProgress = false;
  http.end();
}

// Vérification de la stabilité du système après mise à jour
void checkUpdateStability() {
  // Lire le flag de mise à jour
  byte updateFlag = EEPROM.read(UPDATE_FLAG_ADDR);
  
  // Si nous venons de faire une mise à jour
  if (updateFlag == 1) {
    Serial.println("Première exécution après mise à jour.");
    
    // Réinitialiser le flag de mise à jour
    EEPROM.write(UPDATE_FLAG_ADDR, 0);
    EEPROM.commit();
    
    // On ne vérifiera pas de mise à jour pendant un certain temps
    // pour s'assurer que le système est stable
    initialUpdateCheckDone = false;
    previousUpdateCheckMillis = millis();
  }
  
  // Lire et afficher le compteur de redémarrages
  int rebootCount = EEPROM.read(REBOOT_COUNT_ADDR);
  Serial.print("Nombre de redémarrages: ");
  Serial.println(rebootCount);
}

// Mise en place du mode AP si aucun réseau WiFi n'est configuré
void setupAPMode() {
  const char* ap_ssid = "ESP32_Config";
  const char* ap_password = "12345678";  // Au moins 8 caractères
  
  Serial.println("Démarrage du point d'accès WiFi...");
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.print("Point d'accès démarré: ");
  Serial.println(ap_ssid);
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Ici, vous pourriez démarrer un serveur web pour permettre la configuration
  // des réseaux WiFi via une interface web
}

// Traiter les commandes série (pour ajouter des réseaux WiFi)
void processSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // Commande: add_wifi:SSID:PASSWORD
    if (command.startsWith("add_wifi:")) {
      int firstColon = command.indexOf(':', 9);
      if (firstColon > 9) {
        String ssid = command.substring(9, firstColon);
        String password = command.substring(firstColon + 1);
        
        saveWifiConfiguration(ssid.c_str(), password.c_str());
        Serial.println("Réseau WiFi ajouté. Redémarrage pour appliquer les changements...");
        delay(1000);
        ESP.restart();
      } else {
        Serial.println("Format incorrect. Utilisez: add_wifi:SSID:PASSWORD");
      }
    }
    // Commande: list_wifi
    else if (command == "list_wifi") {
      Serial.println("Réseaux WiFi configurés:");
      for (int i = 0; i < numNetworks; i++) {
        Serial.print(i+1);
        Serial.print(". ");
        Serial.println(wifiNetworks[i].ssid);
      }
    }
    // Commande: clear_wifi
    else if (command == "clear_wifi") {
      preferences.begin("wifi-config", false);
      preferences.clear();
      preferences.end();
      
      Serial.println("Configurations WiFi effacées. Redémarrage...");
      delay(1000);
      ESP.restart();
    }
    // Commande: check_update
    else if (command == "check_update") {
      Serial.println("Vérification manuelle des mises à jour...");
      checkForUpdates();
    }
    // Commande: help
    else if (command == "help") {
      Serial.println("Commandes disponibles:");
      Serial.println("  add_wifi:SSID:PASSWORD - Ajouter un réseau WiFi");
      Serial.println("  list_wifi - Lister les réseaux WiFi configurés");
      Serial.println("  clear_wifi - Effacer toutes les configurations WiFi");
      Serial.println("  check_update - Vérifier les mises à jour maintenant");
      Serial.println("  help - Afficher cette aide");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("-------------------------------------");
  Serial.println("Programme ESP32 OTA via GitHub - Multi WiFi");
  Serial.println("Firmware: " + FIRMWARE_NAME + " v" + FIRMWARE_VERSION);
  Serial.println("-------------------------------------");
  
  // Initialiser l'EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Afficher des informations sur le système
  Serial.print("ESP32 SDK version: ");
  Serial.println(ESP.getSdkVersion());
  Serial.print("ESP32 free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("ESP32 sketch size: ");
  Serial.print(ESP.getSketchSize());
  Serial.println(" bytes");
  Serial.print("ESP32 free sketch space: ");
  Serial.print(ESP.getFreeSketchSpace());
  Serial.println(" bytes");
  Serial.println("-------------------------------------");
  
  // Vérifier la stabilité après mise à jour
  checkUpdateStability();
  
  // Charger les configurations WiFi
  loadWifiConfigurations();
  
  // Tentative de connexion WiFi
  bool connected = false;
  if (numNetworks > 0) {
    connected = connectWiFi();
  } else {
    Serial.println("Aucun réseau WiFi configuré.");
  }
  
  // Si pas connecté ou pas de réseau configuré, démarrer le mode AP
  if (!connected) {
    setupAPMode();
  }
  
  Serial.println("Prêt pour la mise à jour OTA via GitHub");
  Serial.println("-------------------------------------");
  Serial.println("Tapez 'help' pour afficher les commandes disponibles");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Traiter les commandes série
  processSerialCommands();
  
  // Reconnecter le WiFi si nécessaire
  if (WiFi.status() != WL_CONNECTED && numNetworks > 0) {
    static unsigned long lastWiFiAttempt = 0;
    if (currentMillis - lastWiFiAttempt > 60000) {  // Tentative toutes les 60 secondes
      lastWiFiAttempt = currentMillis;
      connectWiFi();
    }
  }
  
  // Si une mise à jour OTA n'est pas en cours, afficher le message à intervalles réguliers
  if (!otaInProgress) {
    // Afficher le message principal
    if (currentMillis - previousMessageMillis >= messageInterval) {
      previousMessageMillis = currentMillis;
      Serial.println(FIRMWARE_NAME);  // "coucou" ou autre selon la version
    }
    
    // Vérifier les mises à jour après le délai initial, puis selon l'intervalle défini
    if (WiFi.status() == WL_CONNECTED) {
      if (!initialUpdateCheckDone) {
        if (currentMillis - previousUpdateCheckMillis >= initialUpdateDelay) {
          initialUpdateCheckDone = true;
          previousUpdateCheckMillis = currentMillis;
          checkForUpdates();
        }
      } else if (currentMillis - previousUpdateCheckMillis >= updateCheckInterval) {
        previousUpdateCheckMillis = currentMillis;
        checkForUpdates();
      }
    }
  }
}
