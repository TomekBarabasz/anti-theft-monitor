# requirements

- detect motion : multiple pir sensors
- send alarm via gprs
- send picture / start streaming via gprs @ alarm 
- save pictures to sd card @ alarm
- actiate pepper spray if alarm [only if armed]
- arm/disarm by sms
- arm/disarm by bluetooth
- attach microphone and allow outgoing voice call to check situation
- attach speaker and allow incomming voice call to do audible warning
- play audio warning from sd card
- control via wifi if in range
- streaming via wifi in in range

# desing

## gpsr connectivity
- always on
- restricted numbers [hard coded]

## wifi
- tcp server started via gpsr command
- AP mode by default
- STA mode for testing @ home
