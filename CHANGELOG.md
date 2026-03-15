# Changelog

## [0.4.1] - 2026-03-15

### Ajouté
- Onglet "Diagnostic" en mode hotspot (AP) : accès aux infos système, capteur PM, modem, logs et redémarrage sans connexion WiFi
- Navigation par onglets WiFi / Diagnostic sur la page du portail captif

### Amélioré
- Page dashboard unifiée entre mode AP et mode connecté (une seule fonction `compose_dashboard`)
- Endpoints API (`/modem-test`, `/sim-test`, `/led-test`, `/logs`, `/rebooting`) accessibles dans les deux modes
- Routing HTTP simplifié : endpoints partagés factorisés en amont du check de mode
- Suppression de la duplication HTML/CSS/JS entre les templates AP et connecté (~200 lignes en moins)

## [0.4.0] - 2026-03-12

### Ajouté
- Capteur PM NextPM via Modbus RTU sur UART0 (GP0 TX, GP1 RX, 115200 8E1)
  - Lecture PM1.0, PM2.5, PM10, température et humidité internes
  - Implémentation Modbus RTU from scratch (CRC16, function 0x03)
  - Lecture automatique toutes les 30s en mode connecté
- Carte "Capteur PM" sur la page status avec les dernières mesures
- Système de logs web : stdio driver custom capturant tous les printf
  - Ring buffer 100 lignes avec suppression automatique des codes ANSI
  - Carte "Logs" sur la page status avec zone scrollable et bouton rafraîchir
  - Endpoint `/logs` retournant les logs en texte brut
  - Fonctionne même sans USB connecté
- Bouton rafraîchir (🔄) sur le titre de la page status (remplacement de l'auto-refresh)
- Documentation câblage NextPM dans le README

### Technique
- UART0 en mode 8E1 (parité paire) pour compatibilité Modbus RTU NextPM
- Formatage PM en arithmétique entière (pas de dépendance pico_printf_float)
- Buffers page status augmentés (body 8KB, page 9KB) pour les nouvelles cartes

## [0.3.0] - 2026-03-12

### Ajouté
- Support modem SARA-R500S via UART1 (GP4 TX, GP5 RX, 115200 baud)
- Commandes AT : envoi avec timeout, détection OK/ERROR
- Test modem au démarrage (ATI) avec affichage dans les logs série
- Page status : carte Modem avec boutons de diagnostic
  - Tester le modem (ATI) : affiche le modèle
  - Tester carte SIM (AT+CIMI) : affiche l'IMSI
  - Activer LED status (AT+UGPIOC=16,2) : active la LED de connexion
- Layout responsive du tableau de bord : 1 colonne (mobile), 2 colonnes (tablette), 3 colonnes (desktop)
- Documentation câblage modem dans le README (table des pins)

### Technique
- Endpoints JSON `/modem-test`, `/sim-test`, `/led-test` via fetch asynchrone
- CSS grid avec media queries (breakpoints 600px / 900px)

## [0.2.0] - 2026-03-12

### Ajouté
- Portail captif WiFi : hotspot "MobileAir" (WPA2, mot de passe "mobileair")
- Page web de configuration WiFi (dark theme, mobile-first) avec scan des réseaux
- Serveur DHCP minimal (pool 192.168.4.100-110)
- Serveur DNS captif (toutes requêtes → 192.168.4.1)
- Serveur HTTP via lwIP httpd avec fichiers custom et support POST
- Détection automatique du portail captif (iOS, Android, Windows, Firefox)
- Sauvegarde des credentials WiFi en flash (dernier secteur, CRC32)
- Reconnexion automatique au dernier réseau WiFi au démarrage
- Reset des credentials WiFi via touche 'r' dans les 3s au boot (serial)
- Page "Tableau de bord" en mode connecté : infos board, WiFi (SSID, signal, IP), bouton redémarrer
- mDNS : accès via `mobileair.local` en mode connecté
- Bouton redémarrer via interface web (watchdog reboot)
- State machine : INIT → CHECK_CREDS → TRY_CONNECT / START_PORTAL → CONNECTED

### Technique
- lwIP en mode poll (`pico_cyw43_arch_lwip_poll`) avec `MEM_LIBC_MALLOC=1`
- IGMP + mDNS responder lwIP
- Affichage IP en surbrillance verte dans les logs serial

## [0.1.0] - 2026-03-12

### Ajouté
- Projet initial avec build CMake + Ninja pour Pico 2W (RP2350)
- Sortie série USB avec message "Hello"
- Système de versioning firmware (version.cmake)
