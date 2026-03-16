# NextPM - Communication Modbus

Documentation du protocole Modbus pour le capteur de particules NextPM (TERA Sensor).

Le NextPM supporte le protocole Modbus conforme a la specification v1.1b.

## Informations generales

### Ordre des octets

Tous les registres sont codes sur **2 octets** (1 word). Certaines donnees occupent **2 registres** (4 octets / 2 words).

Pour les donnees sur 4 octets (ex. mesures PM), le **second word est le plus significatif** (big word = registre d'adresse superieure).

**Exemple** : reponse a une requete PM2.5

```
0x01 0x03 0x04 0x4E 0x10 0x00 0x03 0xAC 0xDF
```

Valeur brute : `(0x4E10) + (0x0003) * 65536 = 216 592`

Un facteur de conversion est ensuite applique pour obtenir la mesure dans l'unite correcte (voir table des registres).

### Adresse Modbus par defaut

L'adresse par defaut est **0x01**. Elle peut etre modifiee via une commande d'ecriture (protocole simple ou Modbus). Le changement est sauvegarde en memoire non volatile et persiste apres un redemarrage.

### Timings

- **Timeout inter-octets** : 50 ms d'inactivite entre 2 octets = timeout
- **Delai de reponse** : 50 ms entre la reception de la requete et le debut de la reponse

### Codes fonction supportes

| Code | Nom | Description |
|------|-----|-------------|
| `0x03` | Read Multiple Holding Registers | Lecture de registres |
| `0x10` | Write Multiple Registers | Ecriture de registres |
| `0x17` | Read/Write Multiple Registers | Lecture et ecriture combinee |

---

## Registres lisibles

Accessibles via les fonctions `0x03` (read) ou `0x17` (read/write).

### Registres systeme

| Registre | Adresse hex | Nom | Description |
|----------|-------------|-----|-------------|
| 1 | `0x01` | Firmware version | Version du firmware embarque |
| 19 | `0x13` | Status | Statut du NextPM (voir detail ci-dessous) |

### Registre Status (0x13) - Codes d'erreur

Le NextPM effectue un diagnostic interne continu. Le registre status est code sur 2 octets (16 bits) contenant des flags d'erreur.

**Note** : les 8 bits de poids fort (bits 8-15) ne sont disponibles que via le protocole Modbus.

| Bit | Nom | Description |
|-----|-----|-------------|
| 0 | Sleep State | Le capteur est en veille (commande sleep ou etat par defaut bit 8). Seule la lecture du status reste autorisee. |
| 1 | Degraded State | Erreur mineure detectee et confirmee. Le capteur peut encore mesurer mais avec une precision reduite. |
| 2 | Not Ready | Le capteur demarre (15 s apres mise sous tension ou reveil). Les mesures ne sont pas fiables tant que ce bit est actif. |
| 3 | Heat Error | L'humidite relative reste au-dessus de 60 % pendant plus de 10 minutes. *(erreur mineure)* |
| 4 | T/RH Error | Les lectures du capteur T/RH sont hors specification. *(erreur mineure)* |
| 5 | Fan Error | La vitesse du ventilateur est hors plage mais il tourne encore. *(erreur mineure)* |
| 6 | Memory Error | Le capteur ne peut pas acceder a sa memoire, certaines fonctions internes ne seront pas disponibles. *(erreur mineure)* |
| 7 | Laser Error | Aucune particule detectee pendant au moins 240 s, possible erreur laser. *(erreur mineure)* |
| 8 | Default State | Le ventilateur s'est arrete apres 3 tentatives de redemarrage. Le capteur passe en mode par defaut puis en veille. |
| 9-15 | - | Reserve |

**Logique** :
- Quand **bit 1** (Degraded) est actif, un ou plusieurs bits d'erreur mineure (3-7) indiquent la cause
- Quand **bit 8** (Default State) est actif, le bit 0 (Sleep) est egalement positionne et le bit 1 (Degraded) est efface

### Mesures PM - Moyenne 10 secondes (Nb/L)

Nombre de particules par litre, moyenne glissante sur 10 s.

| Registres | Adresse hex | Taille PM | Description |
|-----------|-------------|-----------|-------------|
| 50-51 | `0x32-0x33` | PM1 | Particules < 1 um |
| 52-53 | `0x34-0x35` | PM2.5 | Particules < 2.5 um |
| 54-55 | `0x36-0x37` | PM10 | Particules < 10 um |

### Mesures PM - Moyenne 10 secondes (ug/m3)

Concentration massique, moyenne glissante sur 10 s. **Diviser par 10** pour obtenir la valeur en ug/m3.

| Registres | Adresse hex | Taille PM | Description |
|-----------|-------------|-----------|-------------|
| 56-57 | `0x38-0x39` | PM1 | Masse < 1 um |
| 58-59 | `0x3A-0x3B` | PM2.5 | Masse < 2.5 um |
| 60-61 | `0x3C-0x3D` | PM10 | Masse < 10 um |

### Mesures PM - Moyenne 1 minute (Nb/L)

Nombre de particules par litre, moyenne glissante sur 1 min.

| Registres | Adresse hex | Taille PM | Description |
|-----------|-------------|-----------|-------------|
| 62-63 | `0x3E-0x3F` | PM1 | Particules < 1 um |
| 64-65 | `0x40-0x41` | PM2.5 | Particules < 2.5 um |
| 66-67 | `0x42-0x43` | PM10 | Particules < 10 um |

### Mesures PM - Moyenne 1 minute (ug/m3)

Concentration massique, moyenne glissante sur 1 min. **Diviser par 10** pour obtenir la valeur en ug/m3.

| Registres | Adresse hex | Taille PM | Description |
|-----------|-------------|-----------|-------------|
| 68-69 | `0x44-0x45` | PM1 | Masse < 1 um |
| 70-71 | `0x46-0x47` | PM2.5 | Masse < 2.5 um |
| 72-73 | `0x48-0x49` | PM10 | Masse < 10 um |

### Mesures PM - Moyenne 15 minutes (Nb/L)

Nombre de particules par litre, moyenne glissante sur 15 min.

| Registres | Adresse hex | Taille PM | Description |
|-----------|-------------|-----------|-------------|
| 74-75 | `0x4A-0x4B` | PM1 | Particules < 1 um |
| 76-77 | `0x4C-0x4D` | PM2.5 | Particules < 2.5 um |
| 78-79 | `0x4E-0x4F` | PM10 | Particules < 10 um |

### Mesures PM - Moyenne 15 minutes (ug/m3)

Concentration massique, moyenne glissante sur 15 min. **Diviser par 10** pour obtenir la valeur en ug/m3.

| Registres | Adresse hex | Taille PM | Description |
|-----------|-------------|-----------|-------------|
| 80-81 | `0x50-0x51` | PM1 | Masse < 1 um |
| 82-83 | `0x52-0x53` | PM2.5 | Masse < 2.5 um |
| 84-85 | `0x54-0x55` | PM10 | Masse < 10 um |

### Distribution par taille - Moyenne 10 secondes (Nb/L)

Nombre de particules par litre par tranche de taille, moyenne glissante sur 10 s.

| Registres | Adresse hex | Tranche | Description |
|-----------|-------------|---------|-------------|
| 128-129 | `0x80-0x81` | 0.2 - 0.5 um | Particules entre 0.2 et 0.5 um |
| 130-131 | `0x82-0x83` | 0.5 - 1.0 um | Particules entre 0.5 et 1.0 um |
| 132-133 | `0x84-0x85` | 1.0 - 2.5 um | Particules entre 1.0 et 2.5 um |
| 134-135 | `0x86-0x87` | 2.5 - 5.0 um | Particules entre 2.5 et 5.0 um |
| 136-137 | `0x88-0x89` | 5.0 - 10.0 um | Particules entre 5.0 et 10.0 um |

### Registres internes / diagnostic

| Registre | Adresse hex | Nom | Description | Conversion |
|----------|-------------|-----|-------------|------------|
| 100 | `0x64` | Fan command ratio | Ratio de commande ventilateur (%) | Diviser par 100 |
| 101 | `0x65` | Heater command ratio | Ratio de commande chauffage (%) | Diviser par 100 |
| 102 | `0x66` | Fan speed | Vitesse de rotation du ventilateur | Hz |
| 103 | `0x67` | Laser status | Etat du laser | 1 = ON, 0 = OFF |
| 106 | `0x6A` | Relative humidity | Humidite relative interne | Multiplier par 100 (%) |
| 107 | `0x6B` | Temperature | Temperature interne | Multiplier par 100 (degC) |
| 145 | `0x91` | External temperature | Temperature externe calculee | Multiplier par 100 (degC) |

### Colmatage (clogging)

| Registres | Adresse hex | Nom | Description |
|-----------|-------------|-----|-------------|
| 109-110-111 | `0x6D-0x6E-0x6F` | PM10 clogging | Masse de PM10 cumulee par le capteur. Diviser par 10 pour obtenir la masse en mg. Seuil max fixe a 12 mg. |

---

## Utilisation par mode dans MobileAir

Le firmware selectionne automatiquement les registres PM en fonction du mode :

| Mode | Intervalle | Registres PM (ug/m3) | Moyenne |
|------|-----------|----------------------|---------|
| **Mobile** | 10 s | `0x38`, `0x3A`, `0x3C` | 10 sec |
| **Stationnaire** | 60 s | `0x44`, `0x46`, `0x48` | 1 min |

Les registres de temperature, humidite et status sont lus de maniere identique dans les deux modes.

---

## Notes

1. Les mesures en Nb/L et ug/m3 sur 2 registres utilisent l'ordre **low word first** : le premier registre contient le word de poids faible, le second le word de poids fort.
2. Les mesures en ug/m3 doivent etre **divisees par 10** pour obtenir la valeur reelle.
3. Les registres d'humidite et de temperature doivent etre **multiplies par 100** pour obtenir la valeur reelle.

---

*Source : TERA Sensor NextPM datasheet v4.0 - Aout 2024*
