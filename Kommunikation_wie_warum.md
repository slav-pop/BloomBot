# BloomBot: Warum die Kommunikation so aufgebaut ist

## Kurz: wie alles zusammenspielt

BloomBot gießt die Pflanze komplett selbständig. Der Server ist nur für Benachrichtigungen und Einstellungen da. Die genauen Datenformate stehen in INTEGRATION v0.3, hier wird erklärt, warum wir uns so entschieden haben.

- **ESP32** misst Bodenfeuchte und Wasserstand, gießt nach dem Pflanzenprofil und funktioniert auch ohne WLAN und Server.
- **Server** (Cloudflare Worker, kostenlos, muss nicht dauerhaft laufen) speichert Status und Verlauf und berechnet Pflanzenprofil und Wasserprognose.
- **Telegram-Bot** läuft auf demselben Server: Er schickt Benachrichtigungen und nimmt Einstellungen an (Pflanze, Topf, Urlaub).
- Die Verbindung startet immer der ESP32: Er hat keine öffentliche Adresse, deshalb kann der Server ihn nicht von sich aus erreichen.


Der Server spricht den ESP32 nie von sich aus an: Einstellungen und Befehle warten auf dem Server und gehen als Antwort auf die nächste Abfrage raus.

## Zwei Kommunikationswege

Dringendes wird sofort gesendet, alle 10 Minuten geht nur das, was warten kann. Deshalb macht die seltene Abfrage die Benachrichtigungen nicht langsamer.

| Weg | Wann | Was übertragen wird |
| --- | --- | --- |
| **Ereignisse** (`/event`) | sofort, sobald etwas passiert | „Tank leer“, „wenig Wasser“, „Tank aufgefüllt“, „Fehler“. Der Server schreibt sofort in Telegram |
| **Abfrage** (`/poll`) | alle 10 Minuten | Hin: aktueller Status (Feuchte, Tank). Zurück: neue Pflanzeneinstellungen oder der Befehl „Fehler zurücksetzen“ |

Während die Pumpe läuft, macht der ESP32 keine Netzwerkanfragen. Kann ein Ereignis nicht gesendet werden, wird es gespeichert (bis zu 5) und vor der nächsten Abfrage nachgeschickt.

## Warum alle 10 Minuten

10 Minuten sind eine sinnvolle Wahl, keine exakte Berechnung: Die Pflanze braucht es nicht schneller, und weniger Anfragen machen das Gießen zuverlässiger.

**Warum es nicht schneller sein muss**

- Die Bodenfeuchte ändert sich über Stunden und Tage. Das Gerät selbst prüft sie einmal pro Stunde.
- Dringende Benachrichtigungen laufen nicht über die Abfrage, sondern sofort (siehe oben).
- Über die Abfrage kommen nur Einstellungen und das Zurücksetzen eines Fehlers. Wenn das 5–10 Minuten dauert, passiert nichts.

**Was es bringt**

- **Zuverlässigeres Gießen.** Jede HTTPS-Anfrage blockiert den ESP32 für 0,5–2 s. Je seltener die Anfragen, desto weniger stören sie die eigentliche Arbeit.
- **Weniger Last für WLAN und Gerät**, weniger Ausfälle in einem schwachen Netz (zum Beispiel in der Schule).
- **Robust bei Störungen.** „Offline“ wird erst nach 30 Minuten ohne Kontakt gemeldet (3 verpasste Abfragen). Eine verlorene Anfrage löst keinen Fehlalarm aus.

**Was wir dafür in Kauf nehmen**

- Einstellungen und das Zurücksetzen eines Fehlers werden mit bis zu 10 Minuten Verzögerung übernommen. Der Bot antwortet sofort: „Angenommen, wird in ca. 10 Min. ausgeführt“.
- Der Status im Bot kann bis zu 10 Minuten alt sein. Deshalb zeigt der Bot „aktualisiert vor 4 Min.“.

**Wichtig zu wissen**

- Das Intervall legt der Server fest (Feld `poll_s`, 60 bis 3600 s), ein neues Flashen ist nicht nötig. **Auf der Messe stellen wir 60 Sekunden ein**, damit die Demo schnell reagiert, im Alltag wieder 10 Minuten.

## Warum kein Server auf dem ESP32

Der ESP32 kümmert sich um Hardware und Gießen, der Server um Kommunikation und Daten. Ein Server direkt auf dem ESP32 funktioniert nicht: Das Gerät steht hinter einem Router, hat keine öffentliche Adresse, und Telegram kann es nicht erreichen.

Einen Bot direkt auf dem ESP32 gibt es zwar: Das Gerät fragt Telegram alle paar Sekunden selbst nach neuen Nachrichten (Bibliothek UniversalTelegramBot). Diese Variante haben wir bewusst nicht gewählt:

- Die Pflanzendatenbank mit Suche ist für den ESP32 zu groß.
- Ständige Abfragen bei Telegram alle paar Sekunden bedeuten viele HTTPS-Anfragen mit je 0,5–2 s. Sie stören das rechtzeitige Abschalten der Pumpe.
- Für Wasserprognose und Verlauf braucht man Uhrzeit und Speicher, der ESP32 hat keine Uhr.
- Ist das Gerät aus, schweigt der Bot. Der Server dagegen kann selbst melden: „BloomBot ist nicht erreichbar“.

## Warum kein MQTT

MQTT ist ein Standard im IoT-Bereich, würde bei uns aber zusätzliche Teile bringen, und seinen Hauptvorteil brauchen wir nicht.

- **Es braucht einen Broker.** Das ist ein weiterer Server, der rund um die Uhr laufen muss (Mosquitto auf einem Raspberry Pi oder HiveMQ in der Cloud).
- **Telegram spricht kein MQTT.** Man braucht trotzdem ein Vermittlungsprogramm zwischen MQTT und Telegram, das ebenfalls dauerhaft läuft. Das sind zwei Dienste statt einem.
- **Der Hauptvorteil von MQTT** ist die sofortige Zustellung von Befehlen an das Gerät. Das brauchen wir nicht: Es gibt kein Fern-Gießen, nur einen einzigen Befehl („Fehler zurücksetzen“), und Benachrichtigungen kommen ohnehin sofort.
- **Unsere Lösung:** normale HTTP-Anfragen vom ESP32 an den Cloudflare Worker. Das ist kostenlos, nichts muss dauerhaft laufen, und es gibt weniger Teile, die ausfallen können.
- **Erweiterung später möglich:** Falls wir einmal sofortige Steuerung brauchen, lässt sich MQTT ergänzen, ohne die Gießlogik umzubauen.

## Warum es keine Gießtaste gibt

BloomBot ist ein autonomes Bewässerungsgerät: Es gießt nach dem Sensor, nicht auf Knopfdruck.

- Der Nutzer kennt die Bodenfeuchte nicht besser als der Sensor. Zu viel Wasser bedeutet eine Pfütze auf dem Möbel und schadet der Pflanze.
- Wegen der Abfrage alle 10 Minuten würde eine Taste mit Verzögerung reagieren, und das sähe wie ein Defekt aus.
- Einen **Urlaubsmodus** gibt es, aber er funktioniert anders: Der Nutzer gibt nur das Rückkehrdatum ein. Der Server prüft, ob das Wasser reicht, und schlägt bei Bedarf einen Sparmodus vor. Menge und Häufigkeit des Gießens stellt der Nutzer nicht von Hand ein.
- Einen separaten „manuellen“ Modus gibt es nicht.

**Wie wir das Gießen auf der Messe zeigen.** Ein Test-Gießen starten wir mit dem Serial-Befehl `w` vom Laptop, und das Abfrageintervall stellen wir auf 60 Sekunden, damit der Bot schnell reagiert.

## Antworten in einem Satz

| Frage | Antwort |
| --- | --- |
| Läuft der Server auf dem ESP32? | Nein. Der ESP32 steht hinter dem Router und ist mit dem Gießen beschäftigt. Bot, Pflanzendatenbank und Verlauf laufen auf einem kostenlosen Cloudflare Worker, und das Gerät meldet sich selbst. |
| Brauchen wir einen MQTT-Broker? | Nein. Sofortige Steuerung brauchen wir nicht, und ein Broker ist ein weiterer Server, der ständig laufen muss. Bei Bedarf können wir ihn später ergänzen. |
| Warum 10 Minuten? | Dringende Ereignisse gehen sofort raus, alle 10 Minuten nur Status und Einstellungen. Die Pflanze braucht es nicht schneller, und weniger Anfragen machen das Gießen zuverlässiger. Für die Demo stellen wir eine Minute ein. |
| Kann man über den Bot gießen? | Nein, gegossen wird nach dem Sensor. Der Bot ist für Benachrichtigungen und Einstellungen da, inklusive Urlaubsmodus. |
