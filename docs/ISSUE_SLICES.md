# ExcellenceV2 Issue Slices

Deze slices zijn bedoeld als voorstel voor latere GitHub issues. Ze zijn verticaal geformuleerd: elke slice levert observeerbaar gedrag op dat apart gebouwd en getest kan worden.

## 1. ESP-IDF smoke test op huidige ESP32

Status: Afgerond

Type: AFK

Geblokkeerd door: Geen

User stories: 7, 22, 23, 24, 25

### Wat te bouwen

Vervang de huidige Arduino-smoke-test door een ESP-IDF firmware die op de huidige ESP32 boot, seriele bootinfo print en elke seconde een heartbeat logt.

### Acceptatiecriteria

- [x] `platformio.ini` gebruikt `framework = espidf`.
- [x] De firmware buildt voor `esp32dev`.
- [x] De firmware uploadt naar `COM5` met manuele BOOT-procedure.
- [x] De seriele monitor toont chipinfo, MAC-adres en heartbeatregels op `115200`.
- [x] De README documenteert dat COM5 en COM11 voorlopig manuele BOOT bij upload vereisen.

## 2. Hardcoded node identity en rolselectie

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 1

User stories: 1, 2, 3, 4, 5, 6, 18

### Wat te bouwen

Voeg een minimale identitylaag toe waarmee dezelfde firmware als `gateway`, `relay`, `sensor` of `actuator` kan draaien op basis van hardcoded buildconfiguratie.

### Acceptatiecriteria

- [x] Een node heeft een stabiele `node_id`.
- [x] Een node rapporteert zijn rol via Serial bij boot.
- [x] De rol kan zonder codewijziging in netwerklogica gewisseld worden via buildflags in `platformio.ini`.
- [x] Ongeldige rollen geven een duidelijke buildfout.

## 3. ESP-NOW HELLO tussen twee nodes

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 2

User stories: 3, 4, 8, 9, 22

### Wat te bouwen

Implementeer ESP-NOW initialisatie en een eerste `HELLO`-bericht waarmee twee ESP32-modules elkaar kunnen zien.

### Acceptatiecriteria

- [x] ESP-NOW start correct in ESP-IDF.
- [x] Een node zendt periodiek een `HELLO` met `node_id` en rol.
- [x] Een ontvangende node logt de afzender en rol via Serial.
- [x] De ontvangen RSSI of beschikbare linkmetadata wordt gelogd wanneer ESP-IDF die beschikbaar maakt.
- [x] Het systeem blijft draaien wanneer nog geen peers zichtbaar zijn.

## 4. Gateway topology view uit heartbeats

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 3

User stories: 3, 8, 9

### Wat te bouwen

Laat de gateway ontvangen `HELLO`-berichten omzetten naar een eenvoudige topology state met last-seen en linkkwaliteit, en log die periodiek via Serial.

### Acceptatiecriteria

- [x] De gateway houdt per node `node_id`, rol en last-seen bij.
- [x] De gateway toont periodiek een compacte topology dump.
- [x] Nodes die te lang niet gezien zijn worden als stale gemarkeerd.
- [x] De topology dump is bruikbaar om te zien welke nodes direct zichtbaar zijn.

## 5. Berichtcontract met message ID, target en TTL

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 3

User stories: 10, 11, 14

### Wat te bouwen

Introduceer een eerste versie van het netwerkberichtcontract met `message_id`, `source`, `target`, `type`, `ttl` en payload, inclusief validatie en seriele debugoutput.

### Acceptatiecriteria

- [x] Berichten bevatten de afgesproken minimale velden.
- [x] Ongeldige of te korte berichten worden geweigerd zonder crash.
- [x] Een node negeert berichten waarvan `target` niet overeenkomt, behalve wanneer forwarding later toegestaan is.
- [x] Een verlopen TTL wordt herkend en gelogd.
- [x] Het berichtformaat is versieerbaar of uitbreidbaar zonder bestaande velden te breken.


Implementatienotitie: ESP-NOW gebruikt nu `src/protocol/network_message.*` als compact binair formaat. MQTT gebruikt later JSON met dezelfde semantische velden.

## 6. Sensor stuurt actuator-command direct

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 5

User stories: 5, 6, 14

### Wat te bouwen

Laat een sensor-node een eenvoudig idempotent command naar een actuator-node sturen, waarbij de actuator een LED of relais-output zet via `SET_OUTPUT`.

### Acceptatiecriteria

- [x] De gateway fungeert voorlopig als test-zender en genereert periodiek een hardcoded `SET_OUTPUT` testevent.
- [x] `relay_01` ontvangt `SET_OUTPUT` en zet GPIO2 expliciet aan of uit.
- [x] `relay_01` logt endpoint, GPIO, state, source en `message_id`.
- [x] Een duplicate `SET_OUTPUT` met dezelfde source en `message_id` wordt herkend en genegeerd.
- [x] `TOGGLE` wordt niet gebruikt als basiscommand.


Implementatienotitie: Slice 6 is met twee beschikbare modules uitgevoerd. `gateway_01` simuleert voorlopig de sensor/test-zender en stuurt elke 15 heartbeats een `SET_OUTPUT` naar `relay_01/status_led`, gevolgd door dezelfde message als duplicate. `relay_01` gebruikt GPIO2 als test-output, houdt die buiten de heartbeat-toggle, past het eerste command toe en negeert duplicates op basis van `source_node_id + message_id`.

Verificatie: `platformio run -e esp32dev` en `platformio run -e relay_01` slagen. Upload naar COM5 en COM11 slaagt met manuele BOOT-procedure. Seriele logs tonen `Applied SET_OUTPUT id=1001 ... state=on`, `Duplicate SET_OUTPUT ignored id=1001`, `Applied SET_OUTPUT id=1002 ... state=off` en `Duplicate SET_OUTPUT ignored id=1002`.

## 7. End-to-end ACK voor actuator-command

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 6

User stories: 12, 14

### Wat te bouwen

Voeg end-to-end ACK toe zodat de originele afzender weet dat de actuator het command effectief uitgevoerd heeft.

### Acceptatiecriteria

- [x] `relay_01` stuurt een ACK met `correlation_id` gelijk aan het originele `message_id`.
- [x] `gateway_01` logt delivery success wanneer de ACK ontvangen wordt.
- [x] Een ACK zonder pending command wordt als orphan gelogd.
- [x] `relay_01` stuurt de ACK pas nadat GPIO2 expliciet gezet is.


Implementatienotitie: Slice 7 voegt `src/delivery/delivery_tracker.*` toe. `gateway_01` trackt het pending `SET_OUTPUT` command voor verzending. `relay_01` stuurt na effectieve verwerking een ACK terug naar `gateway_01` met een nieuw ACK-`message_id` en `correlation_id` gelijk aan het originele command. De gateway markeert delivery success wanneer de correlation overeenkomt met het pending command; onbekende ACKs worden als orphan gelogd.

Verificatie: `platformio run -e esp32dev` en `platformio run -e relay_01` slagen. Upload naar COM11 slaagde. De eerste upload naar COM5 faalde met `Wrong boot mode detected (0x13)` toen BOOT niet tijdig actief was; de tweede poging met manuele BOOT slaagde. Seriele logs tonen `Sending ACK ack_id=50000 correlation=1001`, `Accepted network message id=50000 correlation=1001 type=ack` en `Delivery success message_id=1001 ack_id=50000`. Dezelfde flow is bevestigd voor `message_id=1002` / `ack_id=50001`.

## 8. Relay forwarding met dedupe en TTL

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 5

User stories: 4, 10, 11, 13

### Wat te bouwen

Laat een relay-node berichten doorsturen naar een doelnode, met dedupe-cache en TTL-verlaging.

### Acceptatiecriteria

- [x] De relay forwardt alleen berichten die niet eerder gezien zijn.
- [x] De relay verlaagt TTL voor forwarding.
- [x] Berichten met TTL `0` worden niet geforward.
- [x] Duplicates worden gelogd en niet opnieuw doorgestuurd.
- [x] Een gateway-to-actuator flow via relay is zichtbaar in de seriele logs.

Implementatienotitie: Slice 8 voegt `src/routing/relay_forwarder.*` toe en registreert een non-local handler in `network_message`. Relay-nodes houden een dedupe-cache bij op `message_id`, `correlation_id`, `type` en `source_node_id`, verlagen TTL bij forwarding en sturen verlopen of dubbele berichten niet opnieuw door.

Verificatie: `platformio run -e esp32dev -e relay_01 -e relay_02 -e actuator_01` slaagt. Fieldtests met gateway, relay en actuator toonden forwarding en ACK-terugkeer, maar ook dat broadcast-forwarding niet hard afdwingt welk pad gebruikt wordt. Daarom is slice 9 aangescherpt naar route-aware forwarding.

## 9. ACK-retourpad via relay

Status: Afgerond

Type: AFK

Geblokkeerd door: Slice 7, Slice 8

User stories: 12, 13

### Wat te bouwen

Maak ACKs bruikbaar wanneer een command via een relay loopt, zodat de originele afzender end-to-end delivery kan bevestigen.

### Acceptatiecriteria

- [x] Een actuator-ACK kan via de relay terug naar de originele afzender.
- [x] De originele afzender logt delivery success voor een via-relay command.
- [x] Relay-logs tonen zowel command-forwarding als ACK-forwarding.
- [x] Duplicates van ACKs worden veilig genegeerd.

Implementatienotitie: De firmware bevat nu route-aware protocolvelden `next_hop_node_id` en `reply_next_hop_node_id`. De gateway stuurt de testflow als `gateway_01 -> relay_01 -> actuator_01`; de actuator accepteert het originele broadcastbericht niet rechtstreeks wanneer `next_hop_node_id=relay_01`. De relay herschrijft `next_hop_node_id` naar de eindbestemming bij command-forwarding en naar `gateway_01` bij ACK-forwarding. De actuator gebruikt `reply_next_hop_node_id=relay_01` om zijn ACK eerst terug naar de relay te sturen.

Verificatie: `platformio run -e esp32dev -e relay_01 -e actuator_01` bouwt succesvol. Upload naar `COM5`, `COM11` en `COM12` slaagt met manuele BOOT-procedure. Gatewaylogs tonen `Delivery success message_id=1001 ack_id=50000 source=actuator_01 source_mac=D8:13:2A:7D:DB:A0`. Relaylogs tonen `Forwarding routed message id=1001 ... type=set_output ... next_hop=actuator_01` en `Forwarding routed message id=50000 ... type=ack ... next_hop=gateway_01`. Actuatorlogs tonen eerst `Ignored network message ... next_hop=relay_01`, daarna `Accepted network message ... next_hop=actuator_01` en `Sending ACK ... next_hop=relay_01`.

## 10. Retry bij ontbrekende ACK

Status: Gedeeltelijk geverifieerd

Type: AFK

Geblokkeerd door: Slice 9

User stories: 12, 13

### Wat te bouwen

Voeg een delivery tracker toe die wacht op ACKs, timeouts detecteert en een command beperkt opnieuw probeert.

### Acceptatiecriteria

- [x] Een command zonder ACK krijgt een timeoutstatus in de delivery tracker.
- [x] Een command wordt maximaal een configureerbaar aantal keer opnieuw verzonden.
- [x] Delivery failure wordt duidelijk gelogd wanneer de retries opgebruikt zijn.
- [x] Een late ACK wordt behandeld als orphan ACK zonder dubbele successmelding.
- [x] Retries blijven idempotent voor actuatorcommands.

Implementatienotitie: Slice 10 bewaart het volledige pending command in `src/delivery/delivery_tracker.*`. Als de ACK na 5 seconden ontbreekt, wordt hetzelfde `message_id` opnieuw verzonden met een verhoogde `attempt`. Na 3 attempts wordt `Delivery failure` gelogd. `gateway_01` stuurt geen nieuw periodiek testcommand zolang een eerder command nog pending is. De relay-dedupe gebruikt nu ook `attempt`, zodat retries worden doorgelaten. De actuator-dedupe blijft idempotent op `source_node_id + message_id`: een duplicate wordt niet opnieuw uitgevoerd, maar stuurt wel opnieuw een ACK.

Verificatie: `platformio run -e esp32dev -e relay_01 -e actuator_01` slaagt. Upload naar `COM5` (`gateway_01`) en `COM11` (`relay_01`) slaagt met manuele BOOT. `actuator_01` was niet langer zichtbaar op de eerdere `COM12`; uitlezen van `COM9` toonde dezelfde actuator-MAC `D0:EF:76:15:86:98`, waarna upload naar `COM9` slaagde. Seriele logs tonen normale delivery met `attempt=0`: `Tracking delivery message_id=1014 attempt=0`, `Applied SET_OUTPUT id=1014 attempt=0`, `Sending ACK ack_id=50003 correlation=1014` en `Delivery success message_id=1014 ack_id=50003 source=actuator_01 source_mac=D8:13:2A:7D:DB:A0`. Een echte ontbrekende-ACK veldtest is nog niet uitgevoerd; daarvoor moet de actuator of relay tijdens een pending command bewust buiten bereik of zonder voeding gezet worden.

## 11. Failsafe voor relay-output

Status: Build geverifieerd

Type: AFK

Geblokkeerd door: Slice 6

User stories: 15

### Wat te bouwen

Voeg failsafe-gedrag toe aan een actuator-output, zodat die naar veilige toestand gaat wanneer communicatie te lang uitblijft.

### Acceptatiecriteria

- [x] Een output heeft een configureerbare failsafe-state.
- [x] Een output heeft een configureerbare communicatie-timeout.
- [x] De output schakelt naar failsafe-state wanneer de timeout verstrijkt.
- [x] Een geldig nieuw command herstelt normale werking.
- [x] De seriele logs tonen wanneer failsafe geactiveerd wordt.

Implementatienotitie: `src/components/digital_output.*` bevat nu een `excellence_digital_output_tick()`. Relay- en actuatorrollen starten GPIO2 veilig als `off`. Na elk geldig `SET_OUTPUT` command wordt `last_valid_command_ms` bijgewerkt en wordt een eventuele actieve failsafe gewist. Als 20 seconden lang geen geldig command meer binnenkomt, zet de runtime GPIO2 naar de failsafe-state `off` en logt `Failsafe activated ...`. De log wordt maar een keer per commandstilte geschreven.

Verificatie: `platformio run -e esp32dev -e relay_01 -e actuator_01` slaagt. Upload naar `COM5` (`gateway_01`, MAC `D0:EF:76:15:54:80`) en `COM9` (`actuator_01`, MAC `D0:EF:76:15:86:98`) is bevestigd met manuele BOOT. Actuatorlogs tonen geldige `SET_OUTPUT` commands en ACKs via de relay, bijvoorbeeld `Applied SET_OUTPUT id=1533 ... state=on`, `Sending ACK ack_id=50001 correlation=1533`, `Applied SET_OUTPUT id=1534 ... state=off` en `Sending ACK ack_id=50002 correlation=1534`. Fysieke failsafe-acceptatie blijft: gateway/relay-commandflow langer dan 20 seconden onderbreken en controleren dat GPIO2/relais naar `off` gaat met de `Failsafe activated` log.

## 12. Eerste componentruntime voor digital output en analog input

Type: AFK

Geblokkeerd door: Slice 2

User stories: 5, 6, 16, 17

### Wat te bouwen

Introduceer een minimale componentruntime die een analoge input periodiek kan lezen en een digitale output expliciet kan zetten.

### Acceptatiecriteria

- [ ] Een `analog_input` component publiceert periodiek een waarde via Serial of netwerkbericht.
- [ ] Een `digital_output` of `relay_output` component kan via command gezet worden.
- [ ] Componenten hebben een stabiel component-id.
- [ ] De runtime weigert een input-only pin als output.
- [ ] De runtime logt configuratiefouten duidelijk.

## 13. Eerste JSON-config laden en valideren

Type: AFK

Geblokkeerd door: Slice 12

User stories: 1, 2, 16, 17, 19

### Wat te bouwen

Vervang hardcoded componentinstellingen door een eerste JSON-config die bij boot geladen en gevalideerd wordt.

### Acceptatiecriteria

- [ ] De node leest een JSON-config bij boot.
- [ ] De config bevat `node_id`, rol, netwerkopties en componenten.
- [ ] Ongeldige JSON stopt componentactivatie met duidelijke logs.
- [ ] Ongeldige pinconfiguratie wordt geweigerd.
- [ ] Een geldige config activeert dezelfde flow als de hardcoded variant.

## 14. Persistent config in NVS of filesystem

Type: AFK

Geblokkeerd door: Slice 13

User stories: 19

### Wat te bouwen

Sla de nodeconfig persistent op en laad die opnieuw na reboot.

### Acceptatiecriteria

- [ ] Een geldige config blijft beschikbaar na reset.
- [ ] Een ontbrekende config valt terug op een veilige default of provisioning-state.
- [ ] Een corrupte config wordt gedetecteerd en niet toegepast.
- [ ] De node logt welke configbron gebruikt werd.

## 15. Gateway bridge richting MQTT of externe host

Type: HITL

Geblokkeerd door: Slice 4, Slice 9

User stories: 21

### Wat te bouwen

Kies en implementeer de eerste externe koppeling voor de gateway: MQTT, Home Assistant discovery, Node-RED of een eenvoudige seriele bridge.

### Acceptatiecriteria

- [ ] De gekozen integratie is expliciet vastgelegd.
- [ ] Gateway-topologie of node-events worden buiten de ESP32 zichtbaar.
- [ ] Een externe commandflow naar een actuator is aantoonbaar of bewust buiten deze slice gehouden.
- [ ] Verbindingsverlies naar de externe host wordt gelogd zonder ESP-NOW flow te breken.

## 16. Automatische download mode hardwarebeslissing

Type: HITL

Geblokkeerd door: Geen

User stories: 25

### Wat te bouwen

Maak een hardwarebeslissing voor de ontwikkelworkflow: huidig board met manuele BOOT blijven gebruiken, een devboard met werkende auto-program schakeling gebruiken, of een eigen DTR/RTS-naar-EN/GPIO0 schakeling voorzien.

### Acceptatiecriteria

- [ ] De gekozen ontwikkelhardware is gedocumenteerd.
- [ ] De uploadprocedure is reproduceerbaar.
- [ ] Als auto-download vereist is, is bevestigd dat DTR/RTS correct naar EN/GPIO0 gaan.
- [ ] Als manuele BOOT geaccepteerd wordt, staat dat expliciet in de ontwikkelnotities.

## Statusonderhoud

Deze file is de bron voor de voorgestelde GitHub issues en moet na elke uitgevoerde slice bijgewerkt worden.

Per afgeronde slice houden we minimaal bij:

- `Status: Afgerond` onder de slice-titel;
- alle werkelijk gehaalde acceptatiecriteria op `[x]`;
- een korte implementatienotitie wanneer de uitvoering afwijkt van de oorspronkelijke formulering;
- eventuele restpunten blijven zichtbaar als open checkbox.
- een git-commit waarin code en documentatie van die slice samen bewaard worden.

Zo blijven de eerdere issues bruikbaar wanneer we ze later als GitHub issues of PR-checklist overnemen.

Commitafspraak: afgeronde slices worden gecommit met een duidelijke slice-prefix, bijvoorbeeld `slice-09 route-aware relay ack forwarding`. De documentatie beschrijft wat functioneel bewezen is; git bewaart de exacte codeversie.

## Voorgestelde volgorde

1. Slice 1: ESP-IDF smoke test op huidige ESP32
2. Slice 2: Hardcoded node identity en rolselectie
3. Slice 3: ESP-NOW HELLO tussen twee nodes
4. Slice 4: Gateway topology view uit heartbeats
5. Slice 5: Berichtcontract met message ID, target en TTL
6. Slice 6: Sensor stuurt actuator-command direct
7. Slice 7: End-to-end ACK voor actuator-command
8. Slice 8: Relay forwarding met dedupe en TTL
9. Slice 9: ACK-retourpad via relay
10. Slice 10: Retry bij ontbrekende ACK
11. Slice 11: Failsafe voor relay-output
12. Slice 12: Eerste componentruntime voor digital output en analog input
13. Slice 13: Eerste JSON-config laden en valideren
14. Slice 14: Persistent config in NVS of filesystem
15. Slice 15: Gateway bridge richting MQTT of externe host
16. Slice 16: Automatische download mode hardwarebeslissing

## Open reviewvragen voor issue-aanmaak

- Is deze granulariteit klein genoeg, of moeten ESP-NOW discovery en RSSI/topology nog verder gesplitst worden?
- Moet de eerste externe bridge MQTT zijn, of wil je eerst alleen Serial behouden?
- Moet hardwarebeslissing Slice 16 eerst gebeuren, of accepteren we voorlopig manuele BOOT tijdens ontwikkeling?







