# Lizenz

Die Timon Box darf jeder nachbauen, verändern und weitergeben. Geld verdienen soll damit niemand. Deshalb stehen die
Teile des Projekts unter zwei Lizenzen. Diese Seite fasst sie zusammen, verbindlich sind die Lizenztexte.

## Gehäuse, Platinen und Doku: CC BY-NC-SA 4.0

`models/`, `hardware/`, `doc/`, `assets/` und `sdcard/` sowie `README.md`, `LICENSE.md` und `CLAUDE.md` stehen unter
der [Creative Commons Namensnennung – Nicht kommerziell – Weitergabe unter gleichen Bedingungen 4.0 International](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.de)
(CC BY-NC-SA 4.0). Der Volltext liegt in [LICENSES/CC-BY-NC-SA-4.0.txt](LICENSES/CC-BY-NC-SA-4.0.txt).

- Nachbauen, verändern und weitergeben ist erlaubt.
- Die Quelle wird genannt, etwa so: „Timon Box von Aaron Blankenburg,
  https://github.com/aaronblankenburg/timon-box“.
- Keine kommerzielle Nutzung, also zum Beispiel keine Gehäuse, Platinen, Bausätze oder fertigen Boxen verkaufen.
- Abwandlungen stehen wieder unter derselben Lizenz.

## Firmware und Werkzeuge: GPL-3.0

Alles Übrige, vor allem `src/`, `tools/`, `android/`, `test/` und `platformio.ini`, steht unter der
[GNU General Public License, Version 3](https://www.gnu.org/licenses/gpl-3.0.html) (GPL-3.0). Der Volltext liegt in
[LICENSES/GPL-3.0.txt](LICENSES/GPL-3.0.txt). Die Firmware baut auf Bibliotheken unter der GPL-3.0 auf, deshalb gilt
sie auch hier. Wer die Firmware weitergibt, ob verändert oder auf einem Gerät, macht den Quellcode samt Änderungen
unter derselben Lizenz zugänglich.

## Ausnahmen

- `doc/assembly/fonts/`: die Schriften IBM Plex Sans und IBM Plex Mono, SIL Open Font License 1.1, siehe `OFL.txt`
  dort.
- `test/host/fixtures/`: Ausschnitte öffentlicher Podcast-Feeds als Testdaten. Die Rechte an ihren Inhalten liegen bei
  den jeweiligen Anbietern.
- Bibliotheken, die PlatformIO beim Bauen lädt, stehen unter ihren eigenen Lizenzen.

## Kommerzielle Nutzung

Wer die Box oder Teile davon kommerziell nutzen möchte, etwa als Bausatz, fragt bitte vorher über ein Issue.

© 2026 Aaron Blankenburg und Mitwirkende
