# Guzvomator – IoT sistem za procenu zauzetosti prostora

Studija slučaja i predlog seminara

## 1 Osnovna ideja projekta

Cilj projekta je razvoj prototipa IoT sistema koji procenjuje trenutnu zauzetost prostora pasivnim osluškivanjem Bluetooth Low Energy (BLE) signala pomoću ESP32 uređaja.

Savremeni mobilni telefoni i drugi elektronski uređaji periodično emituju BLE advertising pakete koje ESP32 može da detektuje bez povezivanja sa samim uređajem. Broj detektovanih uređaja u kratkom vremenskom intervalu koristi se kao indikator (proxy) broja ljudi u prostoru.

Cilj sistema nije praćenje pojedinačnih uređaja, već agregirana procena zauzetosti. Za slučaj upotrebe i testiranje odabrana je studentska čitaonica na fakultetu: studenti bi pre dolaska mogli da provere približnu zauzetost i procene da li ima slobodnih mesta.

## 2 Problem i motivacija

Studenti nemaju jednostavan način da saznaju koliko je neki zajednički prostor trenutno zauzet, pa je potrebno fizički otići do njega da bi se procenilo da li je gužva. Isti problem postoji kod menze, čitaonica, biblioteka, laboratorija i drugih zajedničkih prostorija.

Komercijalni sistemi za praćenje zauzetosti postoje (na primer Occuspace i Cisco Spaces), ali su namenjeni velikim organizacijama i zasnovani na zatvorenim platformama, namenskom hardveru ili cloud servisima.

Projekat ispituje mogućnost razvoja sistema koji je:

- jeftin i zasnovan na ESP32 uređajima;
- open-source i self-hosted;
- bez instalacije aplikacije na telefonima korisnika;
- API-first, radi integracije sa drugim servisima;
- jednostavan za podešavanje: konfiguracija se trajno čuva na uređaju i menja se bez ponovnog flešovanja firmvera, a backend se pokreće jednom komandom.

Projekat je inspirisan eksperimentom Occumetrics [1, 2], u kome je ESP32 korišćen za pasivno BLE skeniranje i procenu promena zauzetosti. Slična rešenja su open-source projekti ESP32-Paxcounter [4] i ESPresense [7]. Rezultati pokazuju da broj detektovanih uređaja može poslužiti kao indikator zauzetosti, ali da njegov odnos prema stvarnom broju ljudi zavisi od konkretnog prostora, zbog čega je potrebna kalibracija.

## 3 Istraživačko pitanje

U kojoj meri broj BLE uređaja detektovanih pomoću jeftinog ESP32 senzorskog čvora može da posluži kao koristan indikator stvarne zauzetosti studentske čitaonice?

Pored toga, posmatra se kako se broj detektovanih uređaja menja tokom dana i u kojoj meri ta promena prati promenu stvarnog broja ljudi (korelacija).

Broj detektovanih uređaja nije jednak broju ljudi: jedna osoba može imati više uređaja, neki uređaji ostaju nedetektovani, a ponašanje BLE emitovanja zavisi od proizvođača i operativnog sistema. Zato se procena kalibriše i validira poređenjem sa nezavisnim brojanjem ljudi u prostoru, kako je opisano u odeljku 6.

## 4 Predložena arhitektura sistema

Sistem se sastoji od senzorskog čvora, dva izmenljiva načina slanja i backend-a koji prima i čuva agregirane zapise.

ESP32 uređaj periodično skenira BLE, lokalno obrađuje rezultate i šalje samo agregirane podatke. Jedan zapis sadrži:

- identifikator senzorskog čvora i vreme merenja (UTC, sa sata sinhronizovanog preko NTP-a);
- broj detektovanih uređaja i broj uređaja čiji RSSI prelazi definisani prag;
- prosečan RSSI i trajanje perioda skeniranja;
- temperaturu, vlažnost vazduha i osvetljenost.

```mermaid
flowchart LR
  subgraph ESP["XIAO ESP32-C6"]
    A["Skeniranje i agregacija"]
    B["BME280 i BH1750"]
    C["NVS konfiguracija i CLI"]
  end
  ESP -->|"HTTPS POST (JSON)"| BE["REST endpoint (FastAPI)"]
  ESP -.->|"MQTT (opciono)"| MB["MQTT broker"]
  MB -.-> BE
  BE --> DB[("PostgreSQL")]
  DB --> API["REST API, CSV izvoz"]
```

Backend prima isti zapis sa oba transporta: HTTP endpoint prima zapis direktno, a MQTT pretplatnik ga čita iz Mosquitto brokera (QoS 1) i upisuje u istu bazu istom logikom.

## 5 Hardver i firmware

Senzorski čvor je Seeed Studio XIAO ESP32-C6 [3]. Ploča ima integrisan BLE i Wi-Fi. Firmver se razvija u PlatformIO okruženju sa Arduino framework-om.

**Hardver**

- XIAO ESP32-C6 kao senzorski čvor, USB napajanje; uređaj je stalno uključen;
- BME280 (temperatura i vlažnost vazduha) i BH1750 (osvetljenost) preko I2C, kao ambijentalni kontekst uz procenu zauzetosti;
- Wi-Fi konekcija za slanje podataka, uz do tri sačuvana profila mreže.

**Ciklus firmvera**

Uređaj radi stalno i u ponavljajućem ciklusu:

1. Wi-Fi veza sa sačuvanom mrežom i sinhronizacija sata preko NTP-a; merenje počinje tek kada je sat sinhronizovan.
2. BLE skeniranje u prozoru zadatog trajanja. Wi-Fi ostaje povezan tokom skeniranja, a pošto ESP32-C6 ima jedan 2,4 GHz radio koji BLE i Wi-Fi dele, prenos podataka može da utiče na broj uhvaćenih paketa.
3. Heširanje adresa uređaja novom nasumičnom solju za svaki prozor, brojanje jedinstvenih heševa, RSSI filtriranje i odbacivanje sirovih adresa.
4. Očitavanje BME280 i BH1750.
5. Slanje agregiranog zapisa preko odabranog transporta; ako slanje ne uspe, zapisi se privremeno čuvaju i šalju kasnije, a ponovno slanje istog zapisa je bezbedno.
6. Kratka pauza do sledećeg ciklusa.

Sat sinhronizovan preko NTP-a koristi se za vremenske oznake zapisa (UTC) i za proveru TLS sertifikata pri HTTPS komunikaciji.

**Konfiguracija i komandna linija**

Parametri uređaja (identifikator, Wi-Fi profili, transport i adresa, trajanje prozora skeniranja, interval, RSSI prag) trajno se čuvaju u NVS-u i podešavaju preko serijske komandne linije. Koristi se dizajn biblioteke arduinoConfig [5], koja je označena kao zastarela, pa se implementira sopstveni registar parametara, i komandni procesor arduinoCmdProc [6].

## 6 Validacija i prikupljanje podataka

Za validaciju se paralelno posmatraju dva izvora podataka vezana za iste vremenske intervale: BLE merenje senzorskog čvora i nezavisno utvrđen broj ljudi u prostoru.

**Ground truth.** Osnovni metod je ručno brojanje u unapred određenim terminima, usklađeno sa prozorima skeniranja. Evidencija postojećeg sistema ulaska u čitaonicu ne može se koristiti: proverom je utvrđeno da ne postoji digitalno beleženje, jer portir ručno menja fizičke kartice za studentske indekse.

Primer oblika podataka (linearnost je samo ilustracija, u realnosti odnos ne mora biti linearan):

| Vreme | BLE uređaji | Realan broj osoba |
| --- | --- | --- |
| 10:00 | 31 | 22 |
| 10:01 | 34 | 23 |
| 10:02 | 37 | 25 |

**Analiza.** Na osnovu podataka ispituju se:

- odnos između broja detektovanih BLE uređaja i stvarne zauzetosti;
- promena broja detektovanih uređaja tokom dana i njena korelacija sa promenom broja ljudi;
- odstupanje procene od stvarne zauzetosti;
- uticaj RSSI praga na rezultate;
- ponašanje sistema pri različitim nivoima zauzetosti;
- stabilnost merenja tokom vremena;
- pozadinski broj uređaja iz susednih prostorija i spratova, izmeren pri praznom prostoru.

Broj detektovanih uređaja nije uporediv između prostora, pa se RSSI prag i kalibracija određuju za svaki prostor posebno. RSSI ne razlikuje pravac, pa uređaji iz susednih prostorija i sa drugih spratova ulaze u slabiji deo opsega; zato se za svaki prostor beleži merenje pri praznoj prostoriji kao referenca. Za velike prostorije jedan čvor pokriva samo zonu oko sebe. Varijacija broja uređaja pri istom broju osoba (smena posetilaca, ponašanje telefona, rotacija adresa) navodi se u analizi.

Način kalibracije (na primer regresija nad prikupljenim podacima) određuje se tek nakon analize odnosa koji se u podacima pokaže. Kalibracija važi za konkretan prostor.

## 7 Privatnost i ograničenja

Obrada identifikatora uređaja vrši se lokalno na ESP32 čvoru. Adrese se heširaju nasumičnom solju koja se generiše iznova za svaki prozor skeniranja (strože od dnevne promene); so se ne čuva niti šalje, a heševi se čuvaju samo u RAM-u tokom prozora skeniranja i odbacuju zajedno sa solju posle brojanja. Centralnom sistemu se šalju samo agregirani podaci, bez adresa ili drugih podataka koji bi omogućili dugoročno praćenje pojedinačnih uređaja. Kamere se ne koriste.

**MAC randomizacija** je poznato osnovno ograničenje metode. Noviji telefoni menjaju adresu koju emituju, pa jedan uređaj može biti izbrojan više puta u dužem prozoru, a broj jedinstvenih adresa se ne poklapa sa brojem uređaja. Zato se kratki prozori skeniranja, RSSI prag i kalibracija prema ground truth podacima razmatraju u metodologiji rada, a ograničenje se navodi eksplicitno pri tumačenju rezultata.

## 8 Odabrani tech stack

| Komponenta | Tehnologija |
| --- | --- |
| Edge uređaj | Seeed Studio XIAO ESP32-C6, PlatformIO, Arduino framework |
| Senzori | BLE skeniranje, BME280, BH1750 |
| Konfiguracija | sopstveni registar parametara po uzoru na arduinoConfig, trajno čuvanje u NVS-u, serijska komandna linija (arduinoCmdProc) |
| Transport | HTTPS POST (JSON) ili MQTT, biranje u konfiguraciji |
| MQTT broker | Eclipse Mosquitto (opciono) |
| Backend | REST endpoint, FastAPI; ista logika radi self-hosted i kao Azure Function |
| Baza podataka | PostgreSQL |
| Pristup podacima | REST API, izvoz u CSV/JSON za analizu |
| Deployment | Docker Compose na sopstvenom serveru ili VPS-u, kao i Azure Functions |

Uređaj šalje isti JSON zapis bez obzira na transport, pa backend ne zavisi od izabranog načina slanja. Kod HTTPS-a čvor proverava serverski sertifikat ugrađenom listom javnih korenskih sertifikata (na primer Let's Encrypt iza obrnutog proksija na VPS-u), pa je za to potreban ispravan sat. Web dashboard nije deo obima ovog rada; podaci su dostupni preko REST API-ja i mogu se prikazati bilo kojim klijentom.

Sistem može da radi kao samostalna self-hosted instanca ili uz Azure servise, bez vezivanja osnovne funkcionalnosti za konkretnog cloud vendor-a.

## 9 Plan rada i obim

Projekat radi jedan student, pa je obim sveden na ono što je potrebno da se odgovori na istraživačko pitanje. Redosled rada:

1. Firmver: BLE skeniranje, prozori, heširanje, filtriranje i lokalna agregacija.
2. Konfiguracija i komandna linija (arduinoConfig, arduinoCmdProc) sa trajnim čuvanjem u NVS-u.
3. HTTP transport i backend: REST endpoint, PostgreSQL, Docker Compose. Deploy na Azure Functions.
4. MQTT transport preko istog interfejsa.
5. Očitavanje BME280 i BH1750 i njihovo dodavanje u zapis.
6. Prikupljanje podataka u čitaonici uz ručno brojanje, kalibracija i analiza.
7. Pisanje seminarskog rada.

## 10 Očekivani rezultat

Rezultat je funkcionalan proof-of-concept IoT sistem koji omogućava:

- automatsko prikupljanje i lokalnu agregaciju podataka o detektovanim BLE uređajima, uz ambijentalna merenja;
- konfigurisanje uređaja preko komandne linije uz trajno čuvanje podešavanja;
- prenos podataka do centralnog sistema preko HTTP-a i MQTT-a;
- skladištenje podataka i pristup njima preko REST API-ja;
- procenu zauzetosti na osnovu prikupljenih podataka;
- pokretanje backend-a jednom komandom u self-hosted okruženju;
- kalibraciju i evaluaciju BLE procene u odnosu na ručno brojanje u čitaonici.

## Reference

1. Matthew McCormick. Building an occupancy sensor with a $5 ESP32 and a serverless DB. [matthew.science/posts/occupancy](https://matthew.science/posts/occupancy/)
2. Hacker News discussion: Building an occupancy sensor with a $5 ESP32 and a serverless DB. [news.ycombinator.com/item?id=38252566](https://news.ycombinator.com/item?id=38252566)
3. Seeed Studio. Getting Started with Seeed Studio XIAO ESP32-C6. [wiki.seeedstudio.com](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
4. ESP32-Paxcounter. Open-source ESP32 Wi-Fi/BLE people counter. [github.com/cyberman54/ESP32-Paxcounter](https://github.com/cyberman54/ESP32-Paxcounter)
5. arduinoConfig, biblioteka za runtime konfiguraciju. [github.com/djherceg/arduinoConfig](https://github.com/djherceg/arduinoConfig)
6. arduinoCmdProc, komandni procesor. [github.com/djherceg/arduinoCmdProc](https://github.com/djherceg/arduinoCmdProc)
7. ESPresense. [github.com/ESPresense/ESPresense](https://github.com/ESPresense/ESPresense)