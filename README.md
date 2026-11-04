# ehcomp
My modded/customized esphome components

dsmr component is trying to solve parse issues with Holley DTSD545 3phase electricity meter @see https://github.com/esphome/issues/issues/3042
The current state of the parser:
  - pass crc checks on my meter, i have to check the telegrams in the issue though
  - handles data blocks with newlines inside
  - ignores missing unit fields
  - integrates with HA without problems

oregon listens on a remote receiver and decodes Oregon Scientific v2
(and logs v3) frames with the pulse decoder from oregonStation.ino. v2
temperature and humidity are published on the sensor platform.

rf_capture is an ESP8266 receiver. When its pulse ring overflows it logs the
captured widths before dropping that sequence.

