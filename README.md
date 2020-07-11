This is a proof of concept to read out the data of Valence U1-12RT batteries with a microcontroller. This is not a finished anything. It fetches the data of up to 4 batteries and writes the data array to the serial interface and a part of it to a display. So please only use it as an example of how to get the data from these batteries (up to 4). What you do with it is up to your specific use case. I've also released a python script that documents the data array a bit more and contains a data log of the genuine display that continuously polls the batteries for updated data.

The code is for the Lilygo T-Display with the ESP32. It is based on the example sketch for the T-Display (https://github.com/Xinyuan-LilyGO/TTGO-T-Display/) module and converted to PlatformIO. The T-Display module is probably not the best module for a battery display/monitor, because it needs about 63mA just running with the display, but it was sitting on my desk and has a lot of options(). Check "void requestBatteryData()" and modify it for your own use. So many possibilities, so little time.

### T-Display details
Connect RX to pin 37 and TX to pin 32 of the RS485 TTL converter. The module will try to fetch the battery data from the bus every time Button 1 is pressed and displays parts of the retrieved information on the display (SOC, U, I, P) for up to 4 batteries (actually the display would be to small for four batteries, but the data should probably be aggregated anyway).

### Build it for the T-Display
Load the project with PlatformIO and fetch the needed libraries. After that the TFT_eSPI/User_Setup_Select.h file of Bodmer's TFT_eSPI library must be modified (check https://github.com/Xinyuan-LilyGO/TTGO-T-Display/).

### Licence
MIT

Use it, improve it and make the world a better place.