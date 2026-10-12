# Specifiche di produzione: sistema rilevazione transiti (4 varchi)

Documento di passaggio per la fase di sviluppo. Raccoglie decisioni già prese,
fatti verificati sull'hardware e cosa costruire. Dove una scelta è lasciata
all'implementazione lo dice esplicitamente. **Il codice presente nel repo
(`src/`, `include/`, `platformio.ini`, `README.md`) è una bozza di una versione
precedente del progetto (ESP32 con whitelist locale e relè) ed è da sostituire.**
Si può riusare solo la parte di lettura PN532 e l'antirimbalzo.

Lingua: interfaccia, messaggi e documentazione in **italiano**; identificatori
nel codice in inglese.

---

## 1. Obiettivo

Registrare i transiti di persone e veicoli ai varchi di un sito, mostrarli in
tempo reale agli operatori della garitta e produrre registri stampabili.

**Non è un controllo accessi**: nessuna serratura, nessun relè, nessuna
decisione di apertura. L'apertura è sempre manuale da parte dell'operatore. Il
sistema legge, registra, mostra.

Fuori ambito (per ora): gestione serrature/cancelli, targhe dei veicoli,
integrazione con i sistemi ufficiali dell'ente, codice CMD a 9 caratteri.
I dati restano esclusivamente in questo sistema.

## 2. Architettura

```
 [PED-ENTRATA] [PED-USCITA] [CAR-ENTRATA] [CAR-USCITA]   4 lettori: ESP32 + PN532, Wi-Fi
          \          |            |          /
           ————— rete LAN privata (Wi-Fi WPA2) —————
                          |
          NAS QNAP TS-873 (Container Station, Docker)
            ├─ container "app": API + pagine web (Python/FastAPI)
            └─ container "db":  PostgreSQL
                          |
        2 PC in garitta: browser (client live + registri)
```

- 4 lettori fissi, uno per punto di lettura. **La direzione è decisa
  dall'hardware** (ogni lettore ha un ID e sa se è entrata o uscita, pedonale o
  carrabile).
- Il lettore legge l'UID e lo invia al server; il server cerca la persona,
  registra il transito e lo spinge in tempo reale ai client.
- Il client degli operatori è una **pagina web** (nessuna installazione sui PC).
- Rete: LAN privata, Wi-Fi interno, indirizzi fissi per NAS e lettori.

### Stack deciso
| Parte | Scelta |
|---|---|
| Server | Python 3.12, FastAPI, SQLAlchemy, Alembic (migrazioni) |
| Database | PostgreSQL 16 (container separato, volume persistente) |
| Tempo reale | Server-Sent Events (SSE) con `Last-Event-ID` per recuperare gli eventi persi |
| Frontend | HTML + JavaScript semplice, nessun passo di build, **nessuna dipendenza da CDN o internet** (rete privata) |
| Stampa | CSS `@media print` e stampa del browser (PDF dal browser); esportazione CSV |
| Deploy | `docker-compose.yml` per Container Station (il TS-873 è x86_64: immagini `amd64`) |
| Firmware | PlatformIO, framework Arduino, ESP32, libreria Adafruit PN532 |

## 3. Fatti verificati sull'hardware

- Tessera: **CMD2**, parte contactless **ISO 14443-A, Mifare Classic 4K**
  (ATQA `0x0002`, SAK `0x18`), a **13,56 MHz**. Verificata con NFC Tools.
- UID di **4 byte**, esempio `50:CC:B2:BC`. Il primo byte non è `08`: **non è un UID
  casuale**, resta lo stesso a ogni lettura.
- Lettore per l'anagrafica: adattatore USB RFID che si presenta come tastiera
  (`VID_08FF&PID_0009`) e "digita" un numero di **10 cifre decimali**.
- **Conversione verificata**: il numero stampato dal lettore USB è l'UID letto
  **con i byte invertiti** (little-endian) in decimale.
  - UID `50 CC B2 BC` → little-endian `BC B2 CC 50` → **`3165834320`** (stampato dal lettore USB)
  - (big-endian darebbe `1355592380`: **non** è il formato del lettore USB)
- Il codice CMD a 9 caratteri alfanumerici **non** è raggiungibile con questo
  lettore e non serve a questo progetto.
- Limite noto, accettato: l'UID del Mifare Classic è copiabile con tessere
  "magiche". Adatto a una rilevazione, non a un controllo di sicurezza.

### Gestione dell'UID (da implementare in modo centralizzato e testato)
- Forma canonica nel database: **esadecimale maiuscolo, byte nell'ordine letto
  dalla tessera**, senza separatori (`50CCB2BC`). Lunghezze ammesse: 8, 14, 20
  cifre esadecimali (UID da 4, 7, 10 byte).
- Input accettati (admin, CSV): esadecimale con o senza `:`/`-`/spazi, oppure
  **decimale a 10 cifre come stampato dal lettore USB** (valido solo per UID da
  4 byte; si interpreta come little-endian, con zeri iniziali).
- Mostrare sempre entrambe le forme (hex e decimale).
- Test obbligatori: `50CCB2BC` ⇄ `3165834320`, zeri iniziali, valori non validi.

## 4. Modello dati (indicativo)

- `persone`: id, grado, nome, cognome, foto (percorso file), attiva, note,
  creato/modificato.
- `tessere`: uid (hex canonico, **univoco**), persona_id, attiva, date.
  Separata da `persone` per poter sostituire una tessera senza perdere lo storico.
- `lettori`: id, codice (`PED-IN`, `PED-OUT`, `CAR-IN`, `CAR-OUT`), nome, varco
  (`PEDONALE`|`CARRABILE`), direzione (`ENTRATA`|`USCITA`), hash del token,
  ultimo contatto.
- `transiti`: id, lettore_id, uid, persona_id (nullo se sconosciuta), esito
  (`NOTO`|`SCONOSCIUTO`), ts_evento, ts_ricezione, ts_affidabile (bool),
  event_id (univoco per lettore, per l'idempotenza dei rinvii),
  presa_visione_da/ora (per le sconosciute).
  Il transito conserva **anche i dati della persona al momento** (grado, nome,
  cognome) o è legato in modo che modifiche successive non alterino lo storico
  stampato: decidere e documentare la scelta.
- `utenti`: username, hash password, ruolo (`OPERATORE`|`ADMIN`), attivo.
- `audit`: azioni dell'admin (creazione/modifica/cancellazione persone, import,
  reset presenze, purge) e stampe dei registri, con utente e ora.

Volume atteso: ~150 persone, qualche migliaio di transiti al mese. Indici su
`transiti(ts_evento)`, `(persona_id, ts_evento)`, `(lettore_id, ts_evento)`.

## 5. Funzioni del server e del client

### Ruoli
- **Operatore** (account standard, più account): vista live, "ancora presenti",
  registri (sola lettura e stampa), presa visione delle tessere sconosciute.
  Non può modificare anagrafica.
- **Amministratore** (account separato, funzioni avanzate): tutto quanto sopra
  più anagrafica (CRUD persone e tessere, foto), import CSV, gestione utenti e
  lettori (token), reset/chiusura manuale delle presenze, eliminazione dati
  oltre la conservazione, audit.
- Password con hash robusto (argon2 o bcrypt), sessione con cookie `HttpOnly`,
  blocco temporaneo dopo tentativi falliti, nessuna password predefinita nota:
  l'admin iniziale si crea al primo avvio con variabile d'ambiente o procedura guidata.

### Vista live (operatori)
- Per ogni transito: **FOTO, grado, nome, cognome**, varco (pedonale/carrabile),
  direzione (entrata/uscita), **data e ora**.
- I 4 varchi distinguibili a colpo d'occhio (colore/etichetta per entrata/uscita).
- Ultimo transito in evidenza (scheda grande) e storico recente scorrevole.
- Indicatore dello stato dei 4 lettori (in linea / non risponde da N minuti).
- Aggiornamento in tempo reale via SSE, con ricollegamento automatico.

### Tessera sconosciuta
- **Allarme rosso ben visibile** con messaggio:
  *«Tessera non registrata. Rivolgersi al Nucleo Telematico.»*
  più UID (hex e decimale), varco, ora.
- Resta in evidenza finché l'operatore non fa "presa visione" (registrata).
- Segnale acustico nel browser (richiede un primo click dell'utente per
  abilitare l'audio: prevederlo in pagina, es. pulsante "Attiva audio").
- Dall'allarme l'admin può passare direttamente a "registra questa tessera" con
  l'UID già compilato.

### "Ancora presenti in sede"
- Una persona è presente se il suo **ultimo transito valido, su qualsiasi varco,
  è un'ENTRATA**.
- Elenco con foto, grado, nome, cognome, varco e ora di ingresso, da quanto
  tempo è dentro; totale in alto; stampabile.
- Uscite mancate (passaggi in due con una sola lettura) sono inevitabili:
  l'admin può **chiudere manualmente** una presenza (con audit) e deve essere
  possibile una regola opzionale configurabile di chiusura automatica a fine
  giornata, **disattivata di default**. Chiedere conferma al cliente prima di attivarla.

### Registri e stampe
- Intervalli: ora, giorno, settimana, mese, intervallo libero.
- Filtri: varco, direzione, persona, grado, esito (noti/sconosciuti).
- Pagina A4 pronta per la stampa: intestazione (nome ente e logo configurabili),
  intervallo, filtri applicati, utente e data di generazione, numeri di pagina,
  totali. Intestazione ripetuta a ogni pagina.
- Esportazione CSV (separatore `;`, codifica UTF-8 con BOM, aperto bene da Excel).
- Stampa e esportazione registrate nell'audit.

### Anagrafica (admin)
- Elenco con ricerca, scheda persona, caricamento foto (ridimensionata lato
  server, es. 400 px JPEG; accettare JPEG/PNG), associazione di una o più tessere.
- **Import CSV**: colonne `uid;grado;nome;cognome;foto` dove `uid` può essere
  decimale a 10 cifre (formato lettore USB) o esadecimale; `foto` è il nome file
  facoltativo, caricabile insieme in uno ZIP. Anteprima con errori riga per riga
  prima di confermare; importazione transazionale; segnalazione dei duplicati.
- Esempio di riga: `3165834320;Mar.;Mario;Rossi;rossi.jpg`

### Conservazione
- Conservare i transiti **almeno 5 anni**. Nessuna cancellazione automatica prima.
- Eliminazione dei dati più vecchi solo manuale, da admin, con soglia
  configurabile (default 5 anni e 1 giorno), conferma esplicita e audit.
- Backup giornaliero del database (container di servizio con `pg_dump` su una
  cartella condivisa del NAS, con rotazione) e backup delle foto. Documentare il
  ripristino e provarlo.
- I dati sono personali: accesso limitato ai ruoli, nessun dato in log applicativi
  non necessari, password mai in chiaro. L'ente deve decidere responsabilità e
  base giuridica del trattamento; non è una scelta tecnica, ma documentarla nel README.

## 6. API tra lettori e server

Autenticazione dei lettori con **token per dispositivo** nell'intestazione
`X-Device-Token` (memorizzato nel server solo come hash). Rete privata, HTTP in
prima versione; HTTPS opzionale tramite reverse proxy del NAS.

`POST /api/v1/events`
```json
{
  "device": "PED-IN",
  "event_id": "a3f9-000123",
  "uid": "50CCB2BC",
  "ts": "2026-10-12T15:05:03+02:00",
  "time_synced": true
}
```
Risposta (usata dal lettore per LED/buzzer):
```json
{ "ok": true, "known": true, "duplicate": false }
```
- **Idempotente su `event_id`**: i rinvii dalla coda non creano duplicati.
- Se `time_synced` è falso, il server usa l'ora di ricezione e imposta
  `ts_affidabile = false` (visibile nel registro).
- Duplicati ravvicinati (stessa tessera, stesso lettore, entro N secondi,
  configurabile, default 5) scartati come `duplicate`.
- Endpoint batch per svuotare la coda dopo una disconnessione.
- `POST /api/v1/heartbeat` ogni 30–60 s (stato, versione firmware, RSSI, lunghezza coda).
- Dal browser: `GET /api/v1/stream` (SSE, protetto da sessione).
- Specificare tutto con OpenAPI (FastAPI lo genera) e testarlo.

## 7. Firmware del lettore

Un firmware unico per i 4 lettori, identità e parametri in configurazione
(non nel codice).

- ESP32 + PN532 via I²C (SDA 21, SCL 22) o SPI, a scelta dell'implementazione,
  documentando i selettori della scheda PN532 V3 (rossa) per la modalità scelta.
  Un lettore per ESP32.
- Legge l'UID ISO 14443-A (4/7/10 byte), lo invia al server, riceve la risposta.
- **Antirimbalzo**: stessa tessera ignorata per ~3–5 s.
- **Feedback**: LED verde/rosso e buzzer in base alla risposta (noto/sconosciuto);
  stati distinti per "connessione assente" e "server non raggiungibile".
- **Coda offline** su LittleFS (con limite, rotazione e priorità ai più vecchi):
  se il NAS non risponde le letture vengono salvate e reinviate con backoff, con
  `event_id` stabile e timestamp originale.
- **Orario**: NTP verso il NAS (indirizzo configurabile) o altro server interno.
  Senza sincronizzazione `time_synced = false`.
- Configurazione (SSID, password, URL server, codice lettore, token, server NTP)
  in memoria non volatile, impostabile da un portale di configurazione al primo
  avvio o al tenere premuto un pulsante; **niente credenziali nel repository**.
- Aggiornamento OTA protetto da password. Watchdog e riavvio automatico se il
  Wi-Fi manca a lungo.
- Isolare la logica pura (formattazione UID, coda, backoff) in codice testabile
  su PC senza hardware.
- **Limite dell'ambiente di sviluppo cloud**: il registro di PlatformIO è bloccato
  dalla rete, quindi **il firmware non si può compilare lì**. Tenerlo semplice e
  indicare chiaramente nel README cosa non è stato compilato/provato su hardware.

## 8. Distribuzione sul NAS (QNAP TS-873, Container Station)

- `docker-compose.yml` con servizi `db`, `app` e (opzionale) `backup`, volumi
  persistenti per database, foto e backup, **riavvio automatico**.
- Variabili d'ambiente per segreti e configurazione (`.env.example` senza valori reali).
- Porta dell'app configurabile; istruzioni per l'indirizzo fisso e per l'NTP
  del NAS verso i lettori.
- README con installazione passo-passo in Container Station, aggiornamento,
  backup/ripristino e creazione dell'admin iniziale.
- Fuso orario `Europe/Rome` ovunque, timestamp salvati in UTC o con offset.

## 9. Struttura del repository proposta

```
server/            applicazione FastAPI, migrazioni, test
  app/ static/ templates/ tests/
firmware/          progetto PlatformIO del lettore
docker-compose.yml
.env.example
docs/              installazione, backup, privacy, cablaggio lettore
SPECIFICHE.md      questo documento
```
Rimuovere dal repo la bozza precedente (`src/`, `include/`, `platformio.ini`,
`README.md` della vecchia versione) quando il nuovo firmware la sostituisce.

## 10. Qualità e prove

- Test automatici del server: conversione UID, API eventi (idempotenza,
  duplicati, tessera sconosciuta, ora non affidabile), presenti, filtri dei
  registri, import CSV, permessi per ruolo.
- Prova della vista live e della stampa con **Chromium/Playwright** (disponibile
  nell'ambiente cloud, non scaricare altri browser) con dati simulati,
  controllando layout, schermata di allarme e stampa A4.
- Script di **simulazione dei 4 lettori** che inviano eventi all'API, per
  provare il sistema senza hardware.
- Dati di prova con persone e foto fittizie; **nessun dato reale nel repo**.

## 11. Ordine di lavoro suggerito

1. Server: modello dati, migrazioni, conversione UID, API eventi, test.
2. Autenticazione e ruoli, anagrafica, import CSV, foto.
3. Vista live (SSE), allarme tessera sconosciuta, "ancora presenti".
4. Registri, stampa A4, esportazione CSV, audit.
5. Docker Compose, backup, documentazione di installazione.
6. Firmware del lettore (coda offline, OTA, configurazione), con prova su banco.
7. Prova su due lettori reali, poi i quattro varchi.

## 12. Punti aperti, da chiedere al cliente quando serve

- Nome dell'ente e logo per le intestazioni dei registri.
- Chiusura automatica delle presenze a fine giornata: sì o no, a che ora.
- Se le fotografie esistono già e in che formato.
- Orari di servizio della garitta (per mostrare lettori "non risponde" solo
  quando ha senso).
- Backup: dove si trova la cartella condivisa di destinazione sul NAS.
- Backup esterno al NAS (un NAS non è un backup di sé stesso).
- Eventuale HTTPS sulla rete interna.
- Politica sui dati personali e informativa agli interessati, a carico dell'ente.

## 13. Hardware per lettore (riferimento acquisto, circa 40 €)

ESP32 DevKit, modulo PN532 V3 (scheda rossa, con convertitore di livello),
LED verde/rosso, buzzer, alimentatore 5 V 2 A, **custodia in plastica IP65**
(il metallo indebolisce Wi-Fi e lettura), il PN532 dietro una parete di plastica a
pochi centimetri dalla tessera (portata reale 2–5 cm). Per un Wi-Fi debole, ESP32
con antenna esterna. Prima ordinare 1 PN532 + 1 ESP32 per il banco di prova.
