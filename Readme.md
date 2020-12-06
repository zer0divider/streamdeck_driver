# Streamdeck Driver (Windows)
This is the windows driver for our home-made streamdeck.

## Releases
Prebuilt binary in folder `Release`.

## Configuration
* when starting the driver the configuration is read from `C:\Users\<user>\streamdeck_config.txt`
* if the configuration file does not exist a default is created
* the config file describes the button mapping to hotkey sequences
* each button is assigned to a group, buttons of the same group can not be triggered simultaneously (mutual exclusion)
* assign button `X` to group `Y` and map to hotkey:
```
X@Y: [C=CTRL][A=ALT][S=SHIFT](a-z | 0-9)
```
* Example, maps button 2 (group 1) to hotkey CTRL+SHIFT+B:
```
02@01: CSb
```
* Use `$X` to delay the following hotkey by `X` milliseconds
* Example, map button 10 (group 10) to sequence: trigger CTRL+A, wait for 1500 milliseconds, trigger CTRL+B:
```
10@10: Ca $1500 Cb
```

## Build (Windows only!)
* create new visual studio Win32 console project
* add all files from folder `streamdeck_driver`
* go to the project settings and add preprocessor definition `_CRT_SECURE_NO_WARNINGS`
* build the project, it should compile without warnings
* load sketch in folder `arduino` onto the Arduino, using the Arduino IDE

