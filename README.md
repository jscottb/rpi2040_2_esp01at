# rpi2040_2_esp01at
RPI Pico/RPI2040 ESP01 AT interface code and PCB

The PCB uses UART1 for the ESP connection and pin pairs 4 & 5 or 8 & 9 can be select via solder jumpers.

The sample code works with the MBED OS RPI2040 as well as the Earle F. Philhower based framework.
   The code does:
      <br>- Init's UART1 for propper comm's
      <br>- Connects to Wifi using the given SSID and PASSWORD
      <br>- Get's the IP adress assinged to the ESP from the router
      <br>- Sends a "Dweet" with the IP given via dweet.io (to be used later to find the Pi)
      <br>- Starts up the local web responder
      <br>- Serves up a landing page
        <p><br>- Responds to url routes: 
          <br>- /ledon
          <br>- /ledoff
          <br>- /flash</p>
       
Once the board is running the example, you can modify the FindMyPico.html file to pull back the IP of the Pico/ESP and open the example landing page. See the html file for more details.
