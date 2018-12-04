# Clock-Gizmo
A clock-barometer-thermometer run by an MKR1000, using mechanical birds to show changes in conditions.

Required parts: 
- MKR1000
- 3 micro servos
- Bare Connectivity MPR121 Capacitive Touch Sensor
- 10RPM or ideally 30RPM Pololu-style motor
- Hall Effect sensor and 60 small magnets
- Two 10K resistors
- MOSFET transistor
- 6V power supply

Several libraries required:
- MPR121.h
- Wire.h
- Time.h
- TimeLib.h
- ArduinoJson.h
- SPI.h
- WiFi101.h
- Servo.h (default)
