# ehcomp
My modded/customized esphome components

dsmr component is trying to solve parse issues with Holley DTSD545 3phase electricity meter @see https://github.com/esphome/issues/issues/3042
The current state of the parser:
  - pass crc checks on my meter, i have to check the telegrams in the issue though
  - handles data blocks with newlines inside
  - ignores missing unit fields
  - integrates with HA without problems

