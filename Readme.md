# Streamdeck Driver (Windows)
This is the windows driver for our home-made streamdeck.

## Releases
Prebuilt binary in folder `Release`.

## Configuration
* when you run the `streamdeck_driver.exe` a new configuration file: `C:\Users\<user>\streamdeck_config.txt` is created
* the config file describes the button mapping to hotkey sequences
```
# Mapping button X to hotkey:
# X: [C=CTRL][A=ALT][S=SHIFT](a-z | 0-9)
# Example, maps button 2 to hotkey CTRL+SHIFT+B:
# 02: CSb\n"
# Use $X to delay the following hotkey by X milliseconds
# Example, map button 10 to sequence: trigger CTRL+A, wait for 1500 milliseconds, trigger CTRL+B:
# 10: Ca $1500 Cb
```

## Build (Windows only!)
* create new visual studio Win32 console project
* add all files from folder `streamdeck_driver`
* go to the project settings and add preprocessor definition `_CRT_SECURE_NO_WARNINGS`
* build the project, it should compile without warnings
* load sketch in folder `arduino` onto the Arduino, using the Arduino IDE

