1tracker_pi Plugin — Requirements (MVP v0.1)

1. Doel

Deze plugin verzendt periodiek geselecteerde navigatiegegevens vanuit OpenCPN naar één of meer HTTP-endpoints.

De MVP ondersteunt uitsluitend verzending naar een eigen website via HTTP POST met JSON payload en een API-key in de request header.

De plugin wordt zo ontworpen dat later eenvoudig extra endpointtypes toegevoegd kunnen worden, zoals noforeignland.

⸻

2. Scope

In scope (MVP)

De plugin ondersteunt:
	•	periodieke verzending van actuele data
	•	meerdere HTTP endpoints
	•	JSON payload
	•	API-key authenticatie via configureerbare HTTP header
	•	configuratie via JSON configbestand
	•	logging
	•	automatische tests voor core logica

Buiten scope (MVP)

Niet inbegrepen in deze fase:
	•	noforeignland integratie
	•	GUI configuratiescherm
	•	database
	•	offline buffering
	•	retry queue
	•	historische trackexport
	•	extra navigatievelden
	•	Signal K integratie

⸻

3. Te verzenden datavelden

De plugin ondersteunt uitsluitend deze velden:

veld	betekenis
timevalue	timestamp in epoch milliseconds
lat	latitude
lon	longitude
awa	apparent wind angle
aws	apparent wind speed

Deze waarden worden steeds als actuele snapshot verzonden.

Er wordt geen averaging toegepast in MVP.

⸻

4. Databron

De plugin ontvangt data via OpenCPN.

De plugin bewaart in memory steeds de laatst bekende geldige waarde van:
	•	timevalue
	•	lat
	•	lon
	•	awa
	•	aws

Als lat/lon ontbreken wordt geen payload verzonden.

⸻

5. Verzendinterval

Default:

60 seconden

Configureerbaar via:

sendIntervalSeconds

Voorwaarden:
	•	verzending gebeurt niet vaker dan interval
	•	verzending blokkeert OpenCPN UI-thread niet

⸻

6. Endpointtype (MVP)

Ondersteund endpointtype:

http_json_with_header_key

Eigenschappen:

veld	verplicht
name	ja
enabled	ja
url	ja
timeoutSeconds	ja
headerName	ja
headerValue	ja


⸻

7. HTTP request formaat

Methode

POST

Headers

Content-Type: application/json
: 

Body

{
  "timevalue": 1710000000000,
  "lat": 52.12345,
  "lon": 4.98765,
  "awa": 38.2,
  "aws": 14.6
}

De headernaam voor authenticatie is configureerbaar.

Voorbeelden:

Authorization
X-API-Key

⸻

8. Meerdere endpoints

De plugin ondersteunt meerdere endpoints tegelijk.

Voorwaarden:
	•	endpoints worden onafhankelijk uitgevoerd
	•	fout in één endpoint blokkeert andere endpoints niet

⸻

9. Configuratiebestand

Configuratie gebeurt via JSON.

Voorbeeld:

{
  "enabled": true,
  "sendIntervalSeconds": 60,
  "endpoints": [
    {
      "name": "main",
      "type": "http_json_with_header_key",
      "enabled": true,
      "url": "https://example.com/api/position",
      "timeoutSeconds": 10,
      "headerName": "X-API-Key",
      "headerValue": "YOUR_SECRET_KEY"
    }
  ]
}


⸻

10. Logging

De plugin logt:
	•	startup
	•	config geladen
	•	endpoints geactiveerd
	•	verzending geslaagd
	•	verzending mislukt
	•	foutreden

Secrets worden nooit volledig gelogd.

Voorbeeld:

X-API-Key: ****abcd

⸻

11. Foutafhandeling

De plugin mag niet crashen bij:
	•	ontbrekende data
	•	ongeldige config
	•	netwerkfouten
	•	timeouts
	•	HTTP fouten

Bij fouten:
	•	endpoint wordt als mislukt gemarkeerd
	•	volgende interval opnieuw proberen

⸻

12. Security

De plugin:
	•	bevat geen secrets in sourcecode
	•	leest secrets uit configbestand
	•	logt secrets gemaskeerd
	•	verstuurt secrets alleen via headers

⸻

13. Architectuur-eisen (voor uitbreidbaarheid)

De interne structuur moet scheiden:

ConfigLoader
StateStore
PayloadBuilder
EndpointSender
Scheduler

⸻

14. Definition of Done (MVP)

De MVP is gereed wanneer:
	•	plugin laadt in OpenCPN
	•	plugin actuele waarden verwerkt
	•	plugin periodiek payload verstuurt
	•	meerdere endpoints ondersteund worden
	•	auth header configureerbaar is
	•	configbestand werkt
	•	secrets niet gelogd worden
	•	fouten per endpoint geïsoleerd blijven
	•	core logica automatisch getest is