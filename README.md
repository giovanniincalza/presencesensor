# Controllo accessi RFID (2 varchi) con ESP32

Sistema autonomo con whitelist locale per due varchi (pedonale e carrabile),
un lettore RFID contactless **PN532** (13,56 MHz, ISO 14443A) per varco.

> **Stato: scritto ma non ancora compilato né provato su hardware.** Il primo
> passo è `pio run` e un test al banco con relè scollegati dalle serrature.

## Cosa fa
- Legge l'**UID** della tessera (parte contactless della CMD2) e lo confronta con la whitelist.
- Whitelist su flash (LittleFS), con autorizzazione per singolo varco.
- Impulso relè non bloccante (durata e cooldown per varco), ingresso di sicurezza
  (fotocellula/spira) che inibisce l'apertura, LED e buzzer.
- Pagina web di gestione (HTTP basic auth): elenco, aggiunta, rimozione, apertura
  manuale, registro eventi, "usa ultima tessera sconosciuta" per l'enrollment.
- Log eventi su flash: `data/ora;varco;UID;nome;esito` (ruota a 64 KB).
- Se il Wi‑Fi non è raggiungibile apre un access point di configurazione.

## Limiti da conoscere
- **UID, non codice CMD a 9 caratteri.** La specifica non dice dove il codice
  è memorizzato nella parte RFID. Questo firmware usa l'UID: le tessere vanno
  enrollate una per una. Il formato a 33 caratteri dei lettori commerciali non è
  implementato (serve il codice CMD).
- **L'UID è clonabile**: sicurezza bassa, adatta a un controllo di comodo, non a
  un varco critico. Alcune tessere usano UID casuale a ogni lettura e non
  funzionano con questo schema: verificate con una CMD2 reale.
- La pagina web usa HTTP in chiaro: tenerla in rete locale protetta, cambiare
  le password in `include/config.h`.
- **Sicurezza del cancello carrabile**: il relè dà solo l'impulso al quadro
  dell'automazione. Fotocellule, coste e arresto di emergenza restano quelle
  dell'impianto e non vanno bypassate. L'ingresso `pinBlock` è un aiuto
  aggiuntivo, non una sicurezza.

## Hardware (per varco)
PN532 (modulo con selettore SPI, impostato su SPI), relè optoisolato (5 V),
LED rosso/verde, buzzer. Alimentare l'ESP32 da 5 V stabilizzati.

Cablaggio di default (vedi `include/config.h`):

| Segnale | ESP32 |
|---|---|
| SCK / MISO / MOSI (comuni ai due PN532) | 18 / 19 / 23 |
| PN532 pedonale SS | 5 |
| PN532 carrabile SS | 4 |
| Relè pedonale / carrabile | 25 / 32 |
| Ingresso sicurezza carrabile (verso GND) | 34 (nota: GPIO34 non ha pull‑up interno, aggiungere 10 kΩ verso 3,3 V) |
| LED ok / ko pedonale | 26 / 27 |
| LED ok / ko carrabile | 33 / 14 |
| Buzzer pedonale / carrabile | 13 / 16 |

I PN532 sono alimentati a 3,3 V (o 5 V secondo modulo) con logica a 3,3 V.
Per distanze oltre pochi metri tra ESP32 e lettore usate un ESP32 per varco
(`GATES[]` in `config.h` con un solo elemento) invece di cavi SPI lunghi.

## Uso
1. Impostare Wi‑Fi e password in `include/config.h`.
2. `pio run -t upload` (il filesystem LittleFS si crea al primo avvio).
3. Monitor seriale a 115200 per vedere IP e log.
4. Aprire `http://<ip>/`, appoggiare la tessera al lettore, premere
   "Usa ultima sconosciuta", dare un nome e salvare.

## Prossimi passi consigliati
- Capire dove la CMD2 tiene il codice a 9 caratteri (lettura sul settore RFID, o sul chip a contatto con ATR/APDU) per usarlo al posto dell'UID.
- Gateway/endpoint per inviare gli eventi al sistema "Controllo Accessi AM" nel formato `AAAAMMGGhhmm` + ID lettore + codice CMD + causale (1001/1002).
- Pulsante di uscita e sensore di stato porta.
