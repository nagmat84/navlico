# Using the Build Environment

This guide assume that Espressif’s ESP-IDF is installed in `~/.espressif`.

## Activating the Build Environment

To activate the build environment source the helper script as follows:
```
. ~/.espressif/tools/activate_idf_v6.0.2.sh
```
The activation is only valid for the current session.
The script performs the following essential tasks:
 - Manipulating the environment variable `PATH` such that Espressif’s tools are found and take precedence over the system tools
 - Set the environment variable `IDF_PATH`. All Espressif tools and build scripts (CMake, Ninja, etc.) evaluate this variable.
   It “links” the project to the ESP IDF.

## Useful Commands

 - `idf.py menuconfig`: Customizes the built configuration
 - `idf.py build`: Builds the project
 - `idf.py flash`: Flashes the project onto the µC
 - `idf.py monitor`: Connects to the µC and reports log messages via UART
