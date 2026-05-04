# ExcellenceV2 ESP32 Network

GitHub repository: https://github.com/TommyTurboo/ExcellenceV2

## Doel

ExcellenceV2 is een ESP32-project voor een eigen lokaal netwerk van modules op plaatsen waar niet overal wifi-dekking is. Het doel is om met een vaste firmware te werken die op meerdere ESP32-modules kan draaien, waarbij elke module via configuratie een eigen rol en functies krijgt.

Voorbeelden:

- een node in de tuin meet het niveau van een waterput via een analoge ingang;
- een node in de garage stuurt een relais of klep;
- een vaste relay-node geeft berichten door tussen modules;
- een gateway-node verzamelt status, routes en meetgegevens en koppelt eventueel met wifi, MQTT, Home Assistant of een eigen dashboard.

## Voorkeursarchitectuur

De aanbevolen startarchitectuur is:

```text
ESP-NOW data plane
+ eigen lichte control plane
+ gateway voor observability en beheer
```

ESP-NOW wordt gebruikt voor de communicatie tussen ESP32-modules. Daarboven komt een eigen netwerklaag die zorgt voor:

- node discovery;
- neighbor tables;
- RSSI- en linkkwaliteitsmeting;
- message IDs;
- deduplicatie;
- TTL;
- forwarding via relay-nodes;
- end-to-end ACKs;
- retries via alternatieve routes;
- failsafe-gedrag voor outputs.

Deze aanpak geeft meer controle dan een volledig automatische mesh en maakt zichtbaar hoe het netwerk werkelijk in elkaar zit.

## Topologie

Een typische installatie ziet er zo uit:

```text
Sensor node -- ESP-NOW -- Relay node -- ESP-NOW -- Gateway -- WiFi/MQTT/Home Assistant
                    |
Actuator node ------+
```

Belangrijke uitgangspunten:

- relay- en gateway-nodes hangen bij voorkeur op vaste voeding;
- batterijgevoede nodes doen zo weinig mogelijk relaywerk;
- belangrijke actuators hebben meer dan een mogelijke route;
- de gateway bewaakt de topologie en kiest later eventueel de beste route;
- commands naar relais zijn idempotent, bijvoorbeeld `SET_OUTPUT true`, niet `TOGGLE`.

## Universele Firmware

Alle ESP32-modules krijgen dezelfde firmware. Het gedrag wordt bepaald door configuratie.

Voorbeeldconfiguratie voor een waterputnode:

```json
{
  "node_id": "garden_well",
  "role": "sensor",
  "components": [
    {
      "id": "well_level",
      "type": "analog_input",
      "pin": 34,
      "interval_ms": 10000,
      "scale": {
        "raw_min": 600,
        "raw_max": 3200,
        "value_min": 0,
        "value_max": 100,
        "unit": "%"
      }
    }
  ],
  "network": {
    "transport": "esp_now",
    "relay_enabled": false
  }
}
```

Voorbeeldconfiguratie voor een relay- en actuatornode:

```json
{
  "node_id": "garage_relay_box",
  "role": "relay_actuator",
  "components": [
    {
      "id": "pump_relay",
      "type": "relay_output",
      "pin": 26,
      "default": false,
      "failsafe_state": false,
      "failsafe_timeout_ms": 30000
    }
  ],
  "network": {
    "transport": "esp_now",
    "relay_enabled": true
  }
}
```

## Eerste Proof of Concept

De eerste milestone is bewust klein:

```text
ESP32 A: gateway
ESP32 B: relay
ESP32 C: sensor node met knop of analoge input
ESP32 D: actuator node met LED of relais
```

Doel van deze PoC:

- nodes kunnen elkaar zien;
- nodes sturen periodiek `HELLO`;
- gateway toont wie gezien wordt;
- sensor stuurt een command naar actuator;
- relay forwardt het bericht;
- actuator voert command uit;
- actuator stuurt end-to-end ACK terug;
- dubbele berichten worden genegeerd.

Voorbeeld van gewenste serial output:

```text
[gateway] seen relay_01 rssi=-55
[gateway] seen sensor_01 via relay_01 rssi=-71
[sensor_01] sent msg 42 target=actuator_01
[relay_01] forwarded msg 42 to actuator_01
[actuator_01] set pump_relay=true msg=42
[sensor_01] received ACK msg=42
```

## Berichtmodel

Het project gebruikt een gedeeld semantisch berichtcontract, maar niet overal dezelfde encoding:

- ESP-NOW gebruikt een compacte binaire payload voor radioverkeer tussen ESP32-modules.
- MQTT en externe integraties gebruiken later JSON met dezelfde velden.
- De gateway vertaalt tussen het binaire ESP-NOW-formaat en de leesbare MQTT-JSON-berichten.

Een extern MQTT-bericht kan er later zo uitzien:

```json
{
  "version": 1,
  "message_id": 42,
  "correlation_id": 0,
  "source_node_id": "sensor_01",
  "target_node_id": "actuator_01",
  "target_endpoint_id": "pump_relay",
  "type": "set_output",
  "ttl": 5,
  "payload": {
    "state": true
  }
}
```

Het interne ESP-NOW-bericht bevat dezelfde betekenis in vaste velden:

- `magic`: herkenning van ExcellenceV2 netwerkberichten;
- `version`: protocolversie;
- `message_id`: numeriek ID voor deduplicatie en ACKs;
- `correlation_id`: verwijzing naar het originele bericht bij ACKs;
- `source_node_id`: verzendende node;
- `target_node_id`: technische doelnode of `*` voor broadcast;
- `target_endpoint_id`: functioneel doel op de node, bijvoorbeeld `pump_relay`;
- `type`: `set_output`, `sensor_value` of `ack`;
- `ttl`: maximale forwardingdiepte;
- `payload`: compacte command- of meetdata.

Slice 5 implementeert validatie, target-checking en TTL-herkenning. Forwarding, deduplicatie en echte ACK-afhandeling volgen in latere slices.

## Discovery en Topology Reporting

Elke node stuurt periodiek een heartbeat:

```json
{
  "type": "HELLO",
  "node_id": "garage_relay",
  "role": "relay",
  "neighbors": [
    {
      "node_id": "gateway",
      "rssi": -58,
      "last_seen_ms": 2000
    },
    {
      "node_id": "pump_node",
      "rssi": -67,
      "last_seen_ms": 3500
    }
  ]
}
```

De gateway gebruikt deze informatie om een netwerkkaart op te bouwen:

```text
gateway
  +-- garage_relay
  |   +-- garden_sensor
  |   +-- pump_node
  +-- attic_relay
      +-- cellar_sensor
```

## Betrouwbaarheid

Voor betrouwbare delivery worden deze mechanismen voorzien:

- end-to-end ACK voor belangrijke berichten;
- retry bij uitblijvende ACK;
- alternatieve route proberen wanneer een relay wegvalt;
- dedupe-cache per node;
- TTL om forwardingloops te vermijden;
- command expiry;
- failsafe-state per relais/output;
- heartbeat timeouts om dode routes te detecteren.

Voor relaissturing geldt:

- gebruik `SET_OUTPUT state=true/false`;
- vermijd `TOGGLE`;
- voer een command alleen uit als het nog geldig is;
- bevestig pas na effectieve uitvoering;
- zet outputs naar veilige toestand bij langdurig communicatieverlies.

## Pin- en Hardwarevalidatie

De firmware moet configuratie valideren voordat pins geactiveerd worden.

Aandachtspunten voor ESP32:

- GPIO 34-39 zijn input-only;
- sommige bootstrapping-pinnen kunnen opstartproblemen veroorzaken;
- ADC2 kan conflicteren met WiFi-gebruik;
- niet elke pin is veilig voor relaissturing;
- hardwareprofielen per boardtype zijn nuttig.

Daarom configureert het systeem liefst functies in plaats van alleen pins:

```json
{
  "components": [
    {
      "id": "well_level",
      "type": "analog_level_sensor",
      "pin": 34
    },
    {
      "id": "pump_relay",
      "type": "relay_output",
      "pin": 26
    }
  ]
}
```

## Voorgestelde Projectstructuur

Start met PlatformIO en ESP-IDF:

```text
ExcellenceV2/
  README.md
  platformio.ini
  docs/
    PRD.md
    ISSUE_SLICES.md
  src/
    main.c
    CMakeLists.txt
    config/
    network/
    routing/
    topology/
    components/
```

ESP-IDF is gekozen omdat dit project controle nodig heeft over ESP-NOW, WiFi-events, NVS, FreeRTOS-taken, logging en later OTA.

## Roadmap

### Milestone 1: Radio en basisberichten

- PlatformIO-project opzetten.
- ESP-NOW initialiseren.
- Node identity hardcoded instellen.
- `HELLO` broadcast implementeren.
- Berichten ontvangen en loggen via Serial.

### Milestone 2: Forwarding

- `message_id`, `source`, `target` en `ttl` toevoegen.
- Relay-node berichten laten doorsturen.
- Dedupe-cache toevoegen.
- Basisroute testen via een relay.

### Milestone 3: ACK en retry

- End-to-end ACK implementeren.
- Retry bij timeout.
- Delivery status loggen.
- Commands idempotent maken.

### Milestone 4: Componentruntime

- `digital_input` toevoegen.
- `digital_output` of `relay_output` toevoegen.
- Sensor-event naar actuator-command vertalen.
- Failsafe-state voor outputs toevoegen.

### Milestone 5: Configuratie

- Configschema vastleggen.
- Hardcoded config vervangen door JSON.
- Config opslaan in LittleFS of NVS.
- Pinvalidatie toevoegen.

### Milestone 6: Gateway en dashboard

- Gateway-topologie opbouwen uit heartbeats.
- Routes en linkkwaliteit tonen.
- MQTT of seriele bridge toevoegen.
- Later integratie met Home Assistant of eigen dashboard.

## Eerste Concrete Stap

De eerste implementatiestap is een minimale PlatformIO/ESP-IDF PoC met drie rollen:

- `gateway`;
- `relay`;
- `node`.

Voor de eerste test mag de rol hardcoded in `config.h` staan. Pas wanneer de radio, forwarding en ACKs stabiel werken, wordt configuratie dynamisch gemaakt.

De eerste ESP-IDF smoke test is bedoeld om te bewijzen dat de huidige hardware kan booten, seriele ESP-IDF logs toont en een heartbeat via GPIO2 laat lopen.

## Node Identity

De firmware heeft een compile-time node identity. De standaardconfiguratie staat in `platformio.ini`:

```ini
build_flags =
  -D EXCELLENCE_NODE_ID=\"gateway_01\"
  -D EXCELLENCE_NODE_ROLE=EXCELLENCE_ROLE_GATEWAY
```

Ondersteunde rollen:

- `EXCELLENCE_ROLE_GATEWAY`
- `EXCELLENCE_ROLE_RELAY`
- `EXCELLENCE_ROLE_SENSOR`
- `EXCELLENCE_ROLE_ACTUATOR`

De firmware valideert de rol tijdens compile-time. Een ongeldige rol veroorzaakt bewust een buildfout. Later kunnen extra PlatformIO environments worden toegevoegd voor meerdere fysieke modules, bijvoorbeeld `gateway_01`, `relay_01`, `sensor_01` en `actuator_01`, zonder de netwerklogica zelf te wijzigen.

## ESP-NOW HELLO

De eerste ESP-NOW transportlaag staat in `src/network/espnow_transport.*`.

Huidig gedrag:

- initialiseert NVS, WiFi STA mode en ESP-NOW;
- gebruikt vast ESP-NOW kanaal `1`;
- registreert send- en receive-callbacks;
- voegt de broadcast peer `FF:FF:FF:FF:FF:FF` toe;
- verstuurt elke vijf heartbeats een binair `HELLO`-bericht met node-id, rol, sequence en uptime;
- logt ontvangen `HELLO`-berichten inclusief RSSI wanneer `rx_ctrl` beschikbaar is.

Met een enkele aangesloten ESP32 kan alleen radio-init en broadcast-send bevestigd worden. Ontvangstlogging tussen nodes wordt volledig verifieerbaar zodra een tweede ESP32 dezelfde firmware draait op hetzelfde ESP-NOW kanaal.

## Topology Store

De gateway houdt ontvangen `HELLO`-berichten bij in `src/topology/topology_store.*`.

Per remote node wordt bijgehouden:

- MAC-adres;
- node-id;
- rol;
- laatste sequence;
- last-seen timestamp;
- RSSI wanneer beschikbaar;
- aantal ontvangen HELLO-berichten;
- status `online` of `stale`.

Gateway-nodes dumpen elke 10 heartbeats een compacte topology view via Serial. Een node wordt voorlopig `stale` wanneer er 15 seconden geen HELLO meer ontvangen is.

## Huidige Testhardware

De eerste aangesloten module is zichtbaar op Windows als:

```text
COM5
Silicon Labs CP210x USB to UART Bridge
USB VID:PID=10C4:EA60
```

De ESP32-chip werd succesvol uitgelezen wanneer de module manueel in download mode werd gezet:

```text
Chip type: ESP32
Model: ESP32-D0WD-V3
Revision: v3.1
Features: WiFi, BT, Dual Core, 240MHz
Crystal: 40MHz
MAC: d0:ef:76:15:54:80
```

Historische bevinding: automatische download mode via `esptool --before default_reset` faalde initieel op COM5. De module reset wel, maar GPIO0 stond toen niet laag tijdens reset:

```text
Wrong boot mode detected (0x13)
The chip needs to be in download mode.
```

Geteste resetmodi:

- `default_reset`: faalt met wrong boot mode;
- `usb_reset`: faalt met no serial data received;
- manueel BOOT vasthouden tijdens connecteren: werkt.

De actuele drie-module testopstelling is:

- `COM5`: `gateway_01`, MAC `D0:EF:76:15:54:80`;
- `COM11`: `relay_01`, MAC `D8:13:2A:7D:DB:A0`;
- `COM12`: `actuator_01`, MAC `D0:EF:76:15:86:98`.

Voor deze boards moet tijdens upload voorlopig nog manueel de BOOT-knop gebruikt worden. Hardwarematig wijst dat erop dat de USB-UART of devboardschakeling DTR/RTS niet correct naar GPIO0 en EN stuurt volgens de klassieke ESP32 auto-program schakeling, of dat die schakeling op deze boards ontbreekt.

## Uitgevoerde Slices

De actuele slice-status staat in `docs/ISSUE_SLICES.md`. Op dit moment zijn verwerkt en gedocumenteerd:

- Slice 1: ESP-IDF smoke test op huidige ESP32.
- Slice 2: hardcoded node identity en rolselectie.
- Slice 3: ESP-NOW HELLO tussen twee nodes.
- Slice 4: gateway topology view uit heartbeats.
- Slice 5: berichtcontract met message ID, target, endpoint, correlation ID en TTL.
- Slice 6: gateway-testsender stuurt `SET_OUTPUT` naar `relay_01/status_led`; relay past GPIO2 toe en negeert duplicates.
- Slice 7: end-to-end ACK van `relay_01` naar `gateway_01` met delivery success logging.
- Slice 8: relay forwarding met dedupe-cache en TTL-verlaging.
- Slice 9: route-aware forwarding en ACK-retourpad via relay is bevestigd op `COM5`, `COM11` en `COM12`.

Afspraak: na elke uitgevoerde slice werken we `docs/ISSUE_SLICES.md` bij, inclusief checkboxen, status en implementatienotities. Daardoor blijven de latere GitHub issues en vertical slices afgestemd op wat al werkelijk gebouwd is.

## Versiebeheer per Slice

De bedoeling is dat elke afgeronde slice ook als aparte git-commit bewaard wordt. De documentatie blijft de functionele bron van waarheid, maar git bewaart de exacte codeversie waarmee een slice bewezen is.

Werkwijze vanaf nu:

- elke slice eindigt met bijgewerkte `README.md` en `docs/ISSUE_SLICES.md`;
- de code en documentatie worden samen gecommit;
- commitberichten gebruiken bij voorkeur `slice-XX korte-beschrijving`;
- `.pio/` en andere lokale buildbestanden worden niet gecommit;
- de lokale repo is gekoppeld aan `https://github.com/TommyTurboo/ExcellenceV2`.

## Direct SET_OUTPUT Test

Slice 6 gebruikt de twee huidige modules als minimale commandflow:

```text
gateway_01 -- ESP-NOW SET_OUTPUT --> relay_01/status_led
```

Huidig gedrag:

- `gateway_01` stuurt elke 15 heartbeats een `SET_OUTPUT` naar `relay_01` endpoint `status_led`;
- dezelfde message wordt direct opnieuw verzonden als duplicate-test;
- `relay_01` gebruikt GPIO2 als test-output en laat de heartbeat die pin niet meer toggelen;
- `relay_01` voert het eerste command uit en negeert duplicates op basis van `source_node_id + message_id`;
- ACK-afhandeling is toegevoegd in slice 7; deze directe flow logt nu delivery success op de gateway.

Bevestigde serial logs:

```text
Applied SET_OUTPUT id=1001 source=gateway_01 endpoint=status_led gpio=2 state=on
Duplicate SET_OUTPUT ignored id=1001 source=gateway_01 endpoint=status_led state=on
Applied SET_OUTPUT id=1002 source=gateway_01 endpoint=status_led gpio=2 state=off
Duplicate SET_OUTPUT ignored id=1002 source=gateway_01 endpoint=status_led state=off
```


## End-to-End ACK Test

Slice 7 breidt de directe `SET_OUTPUT` flow uit met een ACK terug naar de afzender:

```text
gateway_01 -- SET_OUTPUT id=1001 --> relay_01/status_led
relay_01 -- ACK correlation=1001 --> gateway_01
```

Huidig gedrag:

- `gateway_01` registreert een pending command voordat het `SET_OUTPUT` wordt verzonden;
- `relay_01` zet GPIO2 en stuurt daarna pas een ACK;
- de ACK heeft een eigen `message_id` en gebruikt `correlation_id` om naar het originele command te verwijzen;
- `gateway_01` logt delivery success wanneer de ACK overeenkomt met het pending command;
- een onbekende ACK wordt als orphan gelogd.

Bevestigde serial logs:

```text
Sending ACK ack_id=50000 correlation=1001 target_node=gateway_01 endpoint=status_led state=on
Accepted network message id=50000 correlation=1001 type=ack source=relay_01 target_node=gateway_01 endpoint=status_led ttl=3 payload_len=1
Delivery success message_id=1001 ack_id=50000 source=relay_01 endpoint=status_led
```

## Route-Aware Relay Test

De volgende testflow forceert een relay-hop:

```text
gateway_01 -- SET_OUTPUT target=actuator_01 next_hop=relay_01 --> relay_01
relay_01 -- SET_OUTPUT target=actuator_01 next_hop=actuator_01 --> actuator_01
actuator_01 -- ACK target=gateway_01 next_hop=relay_01 --> relay_01
relay_01 -- ACK target=gateway_01 next_hop=gateway_01 --> gateway_01
```

Huidig gedrag in code:

- `target_node_id` blijft de eindbestemming;
- `next_hop_node_id` bepaalt welke node het bericht als volgende mag verwerken;
- `reply_next_hop_node_id` bepaalt via welke relay de eindbestemming zijn ACK terugstuurt;
- een actuator negeert een direct ontvangen broadcastcommand wanneer de next-hop een relay is;
- een relay forwardt alleen als hij zelf de next-hop is;
- ACKs worden teruggestuurd via dezelfde relay-hop.

Bevestigde serial logs:

```text
Sending routed test SET_OUTPUT id=1001 target_node=actuator_01 next_hop=relay_01 reply_next_hop=relay_01
Forwarding routed message id=1001 type=set_output target_node=actuator_01 next_hop=actuator_01 ttl=3->2
Ignored network message id=1001 ... next_hop=relay_01 local_node=actuator_01
Accepted network message id=1001 ... next_hop=actuator_01 reply_next_hop=relay_01
Sending ACK ack_id=50000 correlation=1001 target_node=gateway_01 next_hop=relay_01
Forwarding routed message id=50000 type=ack target_node=gateway_01 next_hop=gateway_01 ttl=3->2
Delivery success message_id=1001 ack_id=50000 source=actuator_01 source_mac=D8:13:2A:7D:DB:A0
```

Build- en uploadstatus: `esp32dev`, `relay_01` en `actuator_01` bouwen succesvol. Upload naar de drie huidige boards is bevestigd met manuele BOOT tijdens `Connecting...`.

## Network Message Contract

De eerste protocolmodule staat in `src/protocol/network_message.*`.

Huidig gedrag:

- definieert een versioned binair ESP-NOW message contract;
- ondersteunt node-adressering via `target_node_id` en functie-adressering via `target_endpoint_id`;
- ondersteunt route-adressering via `next_hop_node_id`;
- ondersteunt ACK-retourroutering via `reply_next_hop_node_id`;
- reserveert `correlation_id` en `ack` type voor latere end-to-end acknowledgements;
- valideert magic, versie, headerlengte, payloadlengte en TTL;
- weigert te korte of verlopen berichten zonder crash;
- negeert berichten voor andere nodes zolang forwarding nog niet actief is;
- logt accept/reject/ignore-beslissingen via Serial;
- houdt MQTT/JSON als externe gatewayrepresentatie, niet als ESP-NOW radioencoding.

De boot-selftest toont op Serial minstens vier beslissingen: accepted, rejected `too_short`, ignored wrong target en rejected `expired_ttl`.








