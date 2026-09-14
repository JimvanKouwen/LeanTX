# -*- coding: utf-8 -*-

# English language sounds configuration

from tts_common import filename

systemSounds = []
sounds = []

for i in range(100):
    systemSounds.append((str(i), filename(i)))
for i in range(9):
    systemSounds.append((str(100 * (i + 1)), filename(100 + i)))
for i, s in enumerate(["thousand", "and", "minus", "point"]):
    systemSounds.append((s, filename(109 + i)))
for i, (s, f) in enumerate([("volt", "volt0"), ("volts", "volt1"),
                            ("amp", "amp0"), ("amps", "amp1"),
                            ("milliamp", "mamp0"), ("milliamps", "mamp1"),
                            ("knot", "knot0"), ("knots", "knot1"),
                            ("meter per second", "mps0"), ("meters per second", "mps1"),
                            ("foot per second", "fps0"), ("feet per second", "fps1"),
                            ("kilometer per hour", "kph0"), ("kilometers per hour", "kph1"),
                            ("mile per hour", "mph0"), ("miles per hour", "mph1"),
                            ("meter", "meter0"), ("meters", "meter1"),
                            ("foot", "foot0"), ("feet", "foot1"),
                            ("degree celsius", "celsius0"), ("degrees celsius", "celsius1"),
                            ("degree fahrenheit", "fahr0"), ("degrees fahrenheit", "fahr1"),
                            ("percent", "percent0"), ("percent", "percent1"),
                            ("milliamp-hour", "mamph0"), ("milliamp-hours", "mamph1"),
                            ("watt", "watt0"), ("watts", "watt1"),
                            ("milli-watt", "mwatt0"), ("milli-watts", "mwatt1"),
                            ("D B", "db0"),("D B", "db1"),
                            ("r p m", "rpm0"),("r p m", "rpm1"),
                            ("g", "g0"),("g", "g1"),
                            ("degree", "degree0"), ("degrees", "degree1"),
                            ("radian", "rad0"), ("radians", "rad1"),
                            ("milliliter", "ml0"), ("milliliters", "ml1"),
                            ("fluid ounce", "founce0"), ("fluid ounces", "founce1"),
                            ("milliliter per minute", "mlpm0"), ("milliliters per minute", "mlpm1"),
                            ("hertz", "hertz0"), ("hertz", "hertz1"),
                            ("milisecond", "ms0"), ("miliseconds", "ms1"),
                            ("microsecond", "us0"), ("microseconds", "us1"),
                            ("kilometer", "km0"), ("kilometers", "km1"),
                            ("hour", "hour0"), ("hours", "hour1"),
                            ("minute", "minute0"), ("minutes", "minute1"),
                            ("second", "second0"), ("seconds", "second1"),
                            ]):
    systemSounds.append((s, filename(f)))
for i, s in enumerate(["point zero", "point one", "point two", "point three",
                       "point four", "point five", "point six",
                       "point seven", "point eight", "point nine"]):
    systemSounds.append((s, filename(167 + i)))
for s, f in [             ("maximum trim reached", "maxtrim"),
             ("timer 1 elapsed", "timovr1"),
             ("timer 2 elapsed", "timovr2"),
             ("timer 3 elapsed", "timovr3"),
             ("transmitter battery low", "lowbatt"),
             ("inactivity alarm", "inactiv"),
             ("throttle warning", "thralert"),
             ("switch warning", "swalert"),
             ("bad eeprom", "eebad"),
             ("Welcome to open tea ex!", "hello"),
             ("RF signal, low", "rssi_org"),
             ("RF signal, critical", "rssi_red"),
             ("telemetry lost", "telemko"),
             ("telemetry recovered", "telemok"),
             ("sensor lost", "sensorko"),
             ("receiver still connected", "modelpwr"),
             ]:
    systemSounds.append((s, filename(f)))
for i, (s, f) in enumerate([("armed", "armed"),
                            ("disarmed", "disarm"),
                            ("throttle  cut", "thrcut"),
                            ("throttle  active", "thract"),
                            ("gear!, up!", "gearup"),
                            ("gear!, down!", "geardn"),
                            ("flaps!, up!", "flapup"),
                            ("flaps!, down!", "flapdn"),
                            ("spoiler!, up!", "splrup"),
                            ("spoiler!, down!", "splrdn"),
                            ("engine!, off!", "engoff"),
                            ("too. high!", "tohigh"),
                            ("too. low!", "tolow"),
                            ("low. battery!", "lowbat"),
                            ("crow!, on!", "crowon"),
                            ("crow!, off!", "crowof"),
                            ("rf. signal!, low!", "siglow"),
                            ("rf. signal!, critical!", "sigcrt"),
                            ("LQ", "lq"),
                            ("RF mode", "rfmode"),
                            ("high. speed. mode!, active", "spdmod"),
                            ("thermal. mode!, on", "thmmod"),
                            ("normal. mode!, on", "nrmmod"),
                            ("landing. mode!, on", "lnding"),
                            ("acro. mode!, on", "acro"),
                            ("vario!, on", "vrion"),
                            ("vario!, off", "vrioff"),
                            ("flight mode acro", "fm-acr"),
                            ("flight mode race", "fm-rce"),
                            ("flight mode stabilize", "fm-stb"),
                            ("flight mode horizon", "fm-hor"),
                            ("flight mode angle", "fm-ang"),
                            ("low rate", "ratlow"),
                            ("medium rate", "ratmed"),
                            ("high rate", "rathi"),
                            ]):
    sounds.append((s, filename(f)))
