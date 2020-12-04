# Streamdeck Driver (Windows)
This is the windows driver for our home-made streamdeck.

## Build (Windows only!)
* create new visual studio Win32 console project
* add all files from folder `streamdeck_driver`
* go to the project settings and add preprocessor definition `_CRT_SECURE_NO_WARNINGS`
* build the project, it should compile without warnings
* load sketch in folder `arduino` onto the Arduino, using the Arduino IDE

## Configuration
* when you run the `streamdeck_driver.exe` a new configuration file: `C:\Users\<user>\streamdeck_config.txt`
* see the examples in the `streamdeck_config.txt` file for details on how to map buttons to hotkeys
