# Lightning Control

Lightning Control is a small, embedded program for the Espressif ESP32-H2 to control the navigation lights on a leisure boat.

## Documentation

 - [Development Board ESP32-H2-DevKitM-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32h2/esp32-h2-devkitm-1/index.html)
 - [ESP-IDF Programming Guide ESP32-H2](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/get-started/index.html)
 - [ESP-H2 Hardware Design Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32h2/index.html)
    - [ESP-H2 Schematic Checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32h2/schematic-checklist.html)
    - [ESP-H2 PCB Layout Design](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32h2/pcb-layout-design.html)
    - [ESP-H2 Programming Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32h2/download-guidelines.html)

## Build Environment on Gentoo

### Preparing the Kernel

See: https://wiki.gentoo.org/wiki/Arduino#Prepare_the_kernel_for_USB_connection:
```
 Device Drivers  --->
   [*] USB support  --->
     <*> USB Serial Converter support  --->
       <*> USB CP210x family of UART Bridge Controllers     (CONFIG_USB_SERIAL_CP210X)
       <*> USB FTDI Single Port Serial Driver               (CONFIG_USB_SERIAL_FTDI_SIO)
       <*> USB Winchiphead CH341 Single Port Serial Driver  (CONFIG_USB_SERIAL_CH341)
     <*> USB Modem (CDC ACM) support                        (ONFIG_USB_ACM)
```

Add users to the following groups
 - `dialout` (for programming)
 - `plugdev` (for using OpenOCD)

### Setting Up Build Environment (Gentoo Way)
 
 1. Create repository `crossdev`
     1. Run `eselect repository create crossdev`
     2. Move repo config: `cd cd /etc/portage/repos.conf/ && mv mv eselect-repo.conf crossdev.conf`
     3. Edit repo config as follows:
        ```
        [crossdev]    
        location = /var/db/repos/crossdev
        priority = 5  
        masters = gentoo
        auto-sync = no
        ```
 2. Emerge `sys-devel/crossdev`
 3. Initialize cross development tool chain `crossdev --init-target riscv32-esp32-elf`
 4. Fix Portage configuration inside `/usr/riscv32-esp32-elf`
     - `/usr/riscv32-esp32-elf/etc/portage/profile/make.defaults`
       ```
       ARCH="riscv"
       KERNEL=""
       ELIBC="newlib"
       ```
     - `/usr/riscv32-esp32-elf/etc/portage/profile/use.force`
       ```
       -kernel_linux
       ```
     - Add the following lines to `/usr/riscv32-esp32-elf/etc/portage/make.conf`
       ```
       KERNEL=""
       INPUT_DEVICES=""
       VIDEO_CARDS=""
       USE=""
       ```
     - Run `PORTAGE_CONFIGROOT=/usr/riscv32-esp32-elf emerge --info` to check if everything looks good
 5. Initialize cross development tool chain `crossdev --target riscv32-esp32-elf`
    ```
    -----------------------------------------------------------------------------
     * crossdev version:      20260501
     * Host Portage ARCH:     amd64
     * Host Portage System:   x86_64-pc-linux-gnu (i686-pc-linux-gnu x86_64-pc-linux-gnu)
     * Target Portage ARCH:   riscv
     * Target System:         riscv32-esp32-elf
     * Stage:                 4 (C/C++ compiler)
     * USE=multilib:          yes
     * Target ABIs:           ilp32d
    
     * binutils:              binutils-[latest]
     * gcc:                   gcc-[latest]
     * libc:                  newlib-[latest]
     * libcxxabi:             libcxxabi-[latest]
     * libcxx:                libcxx-[latest]
    
     * CROSSDEV_OVERLAY:      /var/db/repos/crossdev
     * PORT_LOGDIR:           /var/log/portage
     * PORTAGE_CONFIGROOT:    /
     * Portage flags:         
      _  -  ~  -  _  -  ~  -  _  -  ~  -  _  -  ~  -  _  -  ~  -  _  -  ~  -  _
     * leaving metadata/layout.conf alone in /var/db/repos/crossdev
      _  -  ~  -  _  -  ~  -  _  -  ~  -  _  -  ~  -  _  -  ~  -  _  -  ~  -  _
    !!! WARNING - Cannot auto-configure CHOST riscv32-esp32-elf;
    !!! You should edit /usr/riscv32-esp32-elf/etc/portage/make.conf /usr/riscv32-esp32-elf/etc/portage/profile/make.defaults /usr/riscv32-esp32-elf/etc/portage/profile/use.force
    !!! by hand to complete your configuration.
    !!!  No KERNEL is known for this target.
    ```
### Setting up Build Environment (Espressif Way)

 1. Emerge `dev-embedded/espressif-eim`
 2. Run `eim wizard --skip-prerequisites-check true` as a **normal user**
     - Arrows keys for navigation, space key for toggling, enter key for confirmation
     - The following error is expected and can be ignored as `dev-embedded/openocd` has already installed that rules in `/usr/lib/udev/rules.d/60-openocd.rules`
       ```
       ERROR - Failed to copy OpenOCD rules: Failed to copy /home/matthias/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32/share/openocd/contrib/60-openocd.rules to /etc/udev/rules.d/60-openocd.rules . Make sure you have the necessary permissions. Now you can copy it manually.
       Caused by:
           Permission denied (os error 13)
       ```
     - The final messages should look like
       ```
       Added environment variable ESP_IDF_VERSION = 6.0
       Added environment variable IDF_TOOLS_PATH = /home/matthias/.espressif/tools
       Added environment variable IDF_COMPONENT_LOCAL_STORAGE_URL = file:///home/matthias/.espressif/tools
       Added environment variable IDF_PATH = /home/matthias/.espressif/v6.0.2/esp-idf
       Added environment variable ESP_ROM_ELF_DIR = /home/matthias/.espressif/tools/esp-rom-elfs/20241011/
       Added environment variable OPENOCD_SCRIPTS = /home/matthias/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32/share/openocd/scripts
       Added environment variable IDF_PYTHON_ENV_PATH = /home/matthias/.espressif/tools/python/v6.0.2/venv
       Added environment variable ESP_CLANG_LIBS_PATH = /home/matthias/.espressif/tools/esp-clang-libs/esp-20.1.1_20250829/esp-clang/lib
       Added proper directory to PATH
       Activated virtual environment at /home/matthias/.espressif/tools/python/v6.0.2/venv
       Environment setup complete for the current shell session.
       These changes will be lost when you close this terminal.
       You are now using IDF version 6.0.2.
       NOTICE: Collecting local storage from folder "/home/matthias/.espressif/tools"
       NOTICE: 0 components loaded from "/home/matthias/.espressif/tools" folder
       Collecting required components: 91
       NOTICE: 91 new files downloaded
       2026-08-01 16:14:42 -  4 - 08 - INFO - Components registry synchronized successfully
       Configuration saved successfully to config.toml
       2026-08-01 16:14:42 -  4 - 08 - INFO - Found git: /usr/bin/git
       You have successfully installed ESP-IDF
       for using the ESP-IDF tools inside the terminal, you will find activation scripts inside the base install folder
       sourcing the activation script will setup environment in the current terminal session
       ============================================
       to activate the environment, run the following command in your terminal:
              source "/home/matthias/.espressif/tools/activate_idf_v6.0.2.sh"
       ============================================
       2026-08-01 16:14:42 -  4 - 08 - INFO - Wizard result: Ok
       2026-08-01 16:14:42 -  4 - 08 - INFO - Successfully installed IDF
       2026-08-01 16:14:42 -  4 - 08 - INFO - Now you can start using IDF tools
       ```
### Activating the Build Environment

```
. ~/.espressif/tools/activate_idf_v6.0.2.sh
```

### Configuring projects

Create initial configuration for target.
This also overwrites an existing, custom configuration with the default configuration for the chose target.
```
idf.py --list-targets
idf.py set-target esp32h2
```

Customize configuration
```
idf.py menuconfig
```

## Appendix

### Comparison of Different Cross Compilation Settings

|           | AVR                  | MSP430               | Custom ESP32         |
|:----------|:---------------------|:---------------------|:---------------------|
| `ARCH`    | `avr`                | `unknown`            | `riscv`              |
| `CHOST`   | `avr`                | `msp430-elf`         | `riscv32-esp32-elf`  |
| `KERNEL`  | `-linux __KERNEL__`  | `-linux __KERNEL__`  | `-linux __KERNEL__`  |
| `ELIBC`   | `__LIBC__`           | `newlib`             | `newlib`             |
