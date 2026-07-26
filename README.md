Stoppuhr für die Arduino-IDE bzw. C++. 
Die Stoppuhr kann nur eine voreingestellte Minutenanzahl abwärts zählen. 
Eine Start- /Stopptaste startet bzw. stoppt den Zähler. Der Endwert bleibt so lange im Display stehen, bis die Resettaste gedrückt wird. 
Eine Resettaste setzt den Zähler wieder auf den Anfangswert. 
Ein langes Drücken der Resettaste erlaubt die Minuten einzustellen: mit der Start- / Stopptaste kann der Wert erhöht werden, ein langes Drücken verringert den Minutenwert. Ein nochmaliges Drücken der Resettaste speichert den Minuten-Startwert. 
Bei Überlauf wird eine Minus- bzw. Überzeit solange angezeigt werden, bis mit der Start- / Stopptaste der Zähler gestoppt wird. Beim Erreichen der letzten 30, 20 und 10 Sekunden ertönt ein Piepser, bei 9,8,7,6,5,4,3,2 und 1 Sekunde weitere kurze Piepstöne und bei 00:00 ein langer Piepston. 
Verwende wurde microchip ATtiny 3216 mit Display DOGM081.
Wenn die Spannung von 3,1V der Lithium-Batteriezelle unterschritten wird, erscheint "BATTLOW". Der Strombedarf im Betrieb ist ca. 1,7mA, im sleep-Mode 15uA, daher ist kein Ein-/Ausschalter nötig. Die verwendete Batterie hat eine  Sicherheitsschaltung und zusätzlich hat die Ladeschaltung TP4056 eine Batterie-Überwachung.

<img width="2891" height="3642" alt="totale" src="https://github.com/user-attachments/assets/7a6f89ff-a7ca-406d-b1ea-5b429b6fb91f" />

Angaben zum Gehäuse:
msu.f3d ist der CAD-file für fusion360.
msu.step für andere CAD-Programme verwendbar.
Die *.stl zum direkten Drucken, alle Teile in Drucklage gespeichert.

Gehäuseteile gedruckt mit PCblend, ASA oder ABS im Prusa coreone. 0,2mm Schichthöhe, falls mit Rand, dann Finish der Kanten mit Schleifschwamm nötig.
Start-/Stopp- und Reset-Knopf mit Prusa-MK3S PLA 0,2mm, das transparente Fensterchen mit PETG, 0,1mm Schichthöhe.

Teilelisteund Bezug siehe Tabellenblatt BOM-msu.

Platine bestücken, zuletzt low-cost Fassung und Display.

Software mit Arduino-IDE 2.3.7 erstellt. Für die megaTiny-AVR die Einstellungen machen, die hier beschrieben sind: https://wolles-elektronikkiste.de/megatinycore-nutzen#prep

Arduino-Nano als Programmer mit https://github.com/ElTangas/jtag2updi programmieren und 2 Leitungen für PROG und GND anlöten.

siehe auch dort im readme-file: " 
Building with Arduino IDE
If you prefer, the program can be built as if it was an Arduino sketch. Inside the "source" directory, there is an empty file called "jtag2updi.ino" so that the Arduino IDE can recognize the source code.
Just copy all the files inside "source" to a new directory called "jtag2updi" inside your sketch main directory.
The Arduino IDE will automatically set the correct MCU model and F_CPU, but if you want to change the speed of the UPDI link, you will have to edit UPDI_BAUD directly in the source code.
"

