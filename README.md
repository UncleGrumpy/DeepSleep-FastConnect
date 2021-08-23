# DeepSleep-FastConnect
### Demonstration of the fastest wireless reconnection possible after ESP.deepSleep()


  I found this series of articles immensely helpful in coming up with this solution:
  
  [www.bakke.online/part1](https://www.bakke.online/index.php/2017/05/21/reducing-wifi-power-consumption-on-esp8266-part-1/)
  
  I also took ideas from the ESP8266 LowPowerDemo sketch.
  
  For the fastest contention times use a static IP address, and set the gateway and netmask.
  Waiting for an IP address to be assigned by DHCP can add .5 to 2 additional seconds when connecting.
  The biggest time savings though is saving the mac address and channel of the WiFi Access Point in RTC
  memory.  This eliminates the need for the esp to do a network scan before it can begin the connection.

  Make sure your board is set up to wake from DeepSleep! For example...
+  D1 Mini >> connect D0 to RST.
+  ESP12-F >> connect GPIO16 to RST.
+  ESP01   >> see [this post](https://blog.enbiso.com/post/esp-01-deep-sleep/) to make the necessary modifications. For this mod I personally like to use conductive paint and a sharp needle to apply it...
