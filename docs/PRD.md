# PRD: ExcellenceV2 ESP32 Network

## Probleemstelling

De gebruiker wil een betrouwbaar lokaal ESP32-netwerk bouwen op een huis en eigendom waar niet overal wifi-dekking is. Losse ESP32-modules moeten met elkaar kunnen communiceren, meetwaarden doorgeven, relais of andere outputs aansturen, en zichtbaar maken hoe het netwerk fysiek en logisch in elkaar zit.

Vandaag is er nog geen vaste firmwarebasis, geen gedeeld protocol, geen topologie-overzicht en geen mechanisme om berichten betrouwbaar via relay-nodes af te leveren. Zonder die basis wordt elke ESP32-module een aparte sketch of firmwarevariant, wat beheer, debugging en uitbreiden snel foutgevoelig maakt.

De verandering is nu relevant omdat de eerste hardware is aangesloten, de ESP32-module via `COM5` bevestigd is, en het project klaarstaat als PlatformIO-werkmap. De volgende stap moet het project richting een onderhoudbare ESP-IDF firmware brengen die later als basis voor meerdere modules kan dienen.

## Oplossing

ExcellenceV2 wordt een universele ESP32-firmware op basis van ESP-IDF. Elke module draait dezelfde firmware en krijgt een configuratie die bepaalt welke rol en componenten actief zijn. Communicatie tussen modules gebeurt via ESP-NOW, aangevuld met een eigen lichte netwerklaag voor discovery, topology reporting, forwarding, ACKs, retries en failsafe-gedrag.

De gebruiker moet uiteindelijk kunnen zien welke nodes bestaan, wie elkaar ziet, via welke relay berichten lopen, welke routes betrouwbaar zijn, en welke actuators veilig staan bij communicatiefouten. De eerste versie bewijst dat end-to-end met een kleine hardware-opstelling: gateway, relay, sensor-node en actuator-node.

## User Stories

1. Als eigenaar van het ESP32-netwerk, wil ik dezelfde firmware op meerdere ESP32-modules kunnen zetten, zodat ik niet per module aparte software hoef te onderhouden.
2. Als eigenaar van het ESP32-netwerk, wil ik per module een configuratie kunnen meegeven, zodat dezelfde firmware verschillende rollen kan opnemen.
3. Als eigenaar van het ESP32-netwerk, wil ik een module als gateway kunnen instellen, zodat ik netwerkstatus en berichten centraal kan volgen.
4. Als eigenaar van het ESP32-netwerk, wil ik een module als relay kunnen instellen, zodat berichten ook gebieden zonder wifi-dekking kunnen bereiken.
5. Als eigenaar van het ESP32-netwerk, wil ik een module als sensor kunnen instellen, zodat analoge of digitale ingangen meetdata kunnen publiceren.
6. Als eigenaar van het ESP32-netwerk, wil ik een module als actuator kunnen instellen, zodat relais of digitale outputs op commando kunnen schakelen.
7. Als eigenaar van het ESP32-netwerk, wil ik via seriele logs kunnen zien dat een node opstart, zodat hardware en firmware snel te controleren zijn.
8. Als eigenaar van het ESP32-netwerk, wil ik kunnen zien welke nodes elkaar ontvangen, zodat ik de dekking en relay-posities kan beoordelen.
9. Als eigenaar van het ESP32-netwerk, wil ik RSSI en laatste-zien-tijd per neighbor kennen, zodat ik zwakke of weggevallen links kan herkennen.
10. Als eigenaar van het ESP32-netwerk, wil ik dat berichten een uniek ID hebben, zodat dubbele berichten niet dubbel uitgevoerd worden.
11. Als eigenaar van het ESP32-netwerk, wil ik dat berichten een TTL hebben, zodat forwardingloops vanzelf stoppen.
12. Als eigenaar van het ESP32-netwerk, wil ik dat belangrijke commands end-to-end bevestigd worden, zodat ik weet dat de doelnode het command effectief heeft uitgevoerd.
13. Als eigenaar van het ESP32-netwerk, wil ik dat een bericht via een alternatieve route opnieuw geprobeerd kan worden, zodat het netwerk blijft werken wanneer een relay wegvalt.
14. Als eigenaar van het ESP32-netwerk, wil ik dat relay-commands idempotent zijn, zodat een duplicate `SET_OUTPUT` geen gevaarlijke tweede actie veroorzaakt.
15. Als eigenaar van het ESP32-netwerk, wil ik dat outputs een failsafe-state hebben, zodat relais veilig schakelen wanneer communicatie te lang wegvalt.
16. Als eigenaar van het ESP32-netwerk, wil ik dat configuraties gevalideerd worden tegen pinmogelijkheden, zodat een input-only pin niet als output gebruikt wordt.
17. Als eigenaar van het ESP32-netwerk, wil ik hardwareprofielen kunnen gebruiken, zodat pinregels per boardtype beheersbaar blijven.
18. Als eigenaar van het ESP32-netwerk, wil ik eerst hardcoded rollen kunnen testen, zodat radio, protocol en routing bewezen worden voor dynamische configuratie erbij komt.
19. Als eigenaar van het ESP32-netwerk, wil ik later configuratie persistent kunnen opslaan, zodat een node na reboot zijn rol behoudt.
20. Als eigenaar van het ESP32-netwerk, wil ik later configuratie via de gateway kunnen bijwerken, zodat nodes niet telkens via USB geflasht moeten worden.
21. Als eigenaar van het ESP32-netwerk, wil ik later MQTT of Home Assistant kunnen koppelen, zodat het ESP32-netwerk bruikbaar wordt binnen bestaande automatisering.
22. Als ontwikkelaar, wil ik ESP-IDF gebruiken in plaats van Arduino, zodat ESP-NOW, WiFi-events, NVS, logging, tasks en OTA robuuster beheerd kunnen worden.
23. Als ontwikkelaar, wil ik PlatformIO gebruiken om te builden, uploaden en monitoren, zodat de lokale workflow snel blijft.
24. Als ontwikkelaar, wil ik de huidige ESP32 op `COM5` kunnen blijven gebruiken, zodat de eerste hardware direct inzetbaar blijft.
25. Als ontwikkelaar, wil ik documenteren dat automatische download mode op de huidige module niet werkt, zodat uploadproblemen later sneller verklaard worden.

## Implementatiebeslissingen

- De firmwarebasis wordt ESP-IDF via PlatformIO, niet Arduino.
- De eerste hardware-target blijft `esp32dev` op `COM5`, met seriele monitor op `115200`.
- ESP-NOW is de primaire transportlaag tussen ESP32-modules.
- Wifi wordt in de eerste PoC alleen gebruikt waar ESP-NOW dit technisch vereist of waar de gateway later extern moet koppelen.
- De eerste netwerklaag gebruikt expliciete rollen: `gateway`, `relay`, `sensor` en `actuator`.
- De eerste rollen mogen hardcoded zijn, zodat radio en protocol eerst bewezen worden zonder configuratiecomplexiteit.
- Het berichtcontract bevat minimaal `message_id`, `correlation_id`, `source_node_id`, `target_node_id`, `target_endpoint_id`, `type`, `ttl` en `payload`. ESP-NOW gebruikt hiervoor een compact binair formaat; MQTT gebruikt later JSON met dezelfde semantiek.
- Belangrijke actuator-commands worden end-to-end bevestigd door de doelnode, niet alleen door een relay.
- Nodes houden een dedupe-cache bij op basis van `message_id`.
- Relay forwarding verlaagt de TTL en weigert verlopen berichten.
- Commands naar outputs zijn idempotent. `SET_OUTPUT` is toegestaan; `TOGGLE` hoort niet in de basislaag.
- Commands krijgen later een expiry, zodat oude commands niet onverwacht uitgevoerd worden.
- Elke node publiceert periodiek een `HELLO` of heartbeat met node-id, rol en bekende neighbors.
- De gateway bouwt topology state uit heartbeats en neighbor reports.
- Linkkwaliteit wordt in eerste instantie afgeleid uit RSSI en last-seen timestamps.
- Failsafe-gedrag wordt een eigenschap van actuatorcomponenten, niet van losse pinnen.
- Componenten worden geconfigureerd als functies zoals `analog_input`, `digital_input` en `relay_output`, niet als generieke pinlabels.
- Pinvalidatie wordt verplicht voordat componenten geactiveerd worden.
- Hardwareprofielen worden voorzien om ESP32 board- en pinregels te centraliseren.
- Persistent config komt pas nadat de hardcoded PoC stabiel is.
- OTA, MQTT, Home Assistant en dashboarding vallen na de radio- en protocolbasis.

Voorgestelde deep modules:

- `Transport`: smalle interface voor ESP-NOW send/receive, peer management en radio-initialisatie.
- `MessageCodec`: stabiel contract voor encoding, decoding, validatie en versiebeheer van berichten.
- `Router`: beslist of berichten lokaal verwerkt, geforward of geweigerd worden.
- `DeliveryTracker`: beheert end-to-end ACKs, retry-status, timeouts en delivery result.
- `TopologyStore`: houdt nodes, neighbors, RSSI, last-seen en routekandidaten bij.
- `ComponentRuntime`: activeert geconfigureerde sensoren en actuators via een beperkte componentinterface.
- `ConfigManager`: laadt, valideert en later persisteert configuratie.
- `HardwareProfile`: valideert boardtype, veilige pins, input-only pins en bootstrapping-risico's.

## Testbeslissingen

- De eerste test bewijst dat de ESP-IDF firmware bootinfo en heartbeat via Serial toont op de huidige ESP32.
- De eerste radio-integratietest bewijst dat twee nodes via ESP-NOW een `HELLO` kunnen uitwisselen.
- De eerste topologytest bewijst dat een gateway minstens een direct zichtbare node met last-seen en RSSI kan loggen.
- De eerste forwardingtest bewijst dat een sensorbericht via een relay bij een actuator aankomt.
- De eerste reliabilitytest bewijst dat een actuator een end-to-end ACK terugstuurt en dat duplicates genegeerd worden.
- De eerste failsafetest bewijst dat een relay-output naar veilige toestand gaat na communicatie-timeout.
- Message parsing en routingbeslissingen krijgen prioriteit voor unit-achtige tests via stabiele interfaces.
- Hardwaregedrag blijft initieel smoke-testgericht, omdat fysieke ESP32-testen afhankelijk zijn van aangesloten modules.
- seriele output is in de eerste fase een geldige acceptatiebron voor gedrag.
- Later verdienen gateway-naar-MQTT en config-persistentie aparte integratiechecks.

## Buiten Scope

- Geen volledig automatisch mesh-routingprotocol in de eerste versie.
- Geen ESP-WIFI-MESH of ESP-Mesh-Lite als primaire basis.
- Geen dashboard in de eerste PoC.
- Geen Home Assistant-integratie in de eerste PoC.
- Geen OTA-updates in de eerste PoC.
- Geen remote configuratie-update in de eerste PoC.
- Geen batterijoptimalisatie of deep sleep in de eerste PoC.
- Geen garanties voor industriele safety of kritische installaties.
- Geen hardwareontwerp voor definitieve PCB's in deze PRD.

## Verdere Opmerkingen

- De huidige testmodule is zichtbaar als `Silicon Labs CP210x USB to UART Bridge (COM5)`.
- De chip is bevestigd als `ESP32-D0WD-V3`, revision `v3.1`, met MAC `D0:EF:76:15:54:80`.
- Automatische download mode werkt momenteel niet betrouwbaar op COM5 of COM11. Voor beide boards moet bij upload voorlopig manueel de BOOT-knop gebruikt worden. DTR/RTS-naar-GPIO0/EN blijft de eerste hardwarecontrole.
- Hardwarematig vraagt automatische download mode correcte DTR/RTS-aansturing naar GPIO0 en EN via de klassieke ESP32 auto-program schakeling.
- Het bestaande Arduino-smoke-testprogramma mag vervangen worden door een ESP-IDF-smoke-test voordat de echte netwerkbasis start.
- De eerste implementatie moet klein blijven: liever een demonstrabele gateway-relay-node flow dan meteen een algemeen framework zonder bewezen radioflow.



