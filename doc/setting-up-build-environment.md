# Build Environment

This documentation has a strong focus on Gentoo Linux.

 - [Required Kernel Options and User Group Settings](#required-kernel-options-and-user-group-settings)
 - Option 1 (dysfunctional): [Setting Up Build Environment (Gentoo Way, INCOMPLETE)](#setting-up-build-environment-gentoo-way-incomplete)
 - Option 2 (working): [Setting up Build Environment (Espressif Way)](#setting-up-build-environment-espressif-way)

## Required Kernel Options and User Group Settings

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

## Setting Up Build Environment (Gentoo Way, INCOMPLETE)

The following descriptions are based on
 - [Gentoo Wiki: Arduino](https://wiki.gentoo.org/wiki/Arduino)
 - [Gentoo Wiki: Embedded Handbook - Creating a Cross-Compiler](https://wiki.gentoo.org/wiki/Embedded_Handbook/General/Creating_a_cross-compiler)
 - [Gentoo Wiki: Crossdev](https://wiki.gentoo.org/wiki/Crossdev)
The Gentoo Arduino documentation discourages to use the pre-built, proprietary, manufacturer's toolchain,
[but use the native Gentoo tools](https://wiki.gentoo.org/wiki/Arduino#Recommended:_Install_the_toolchain_using_crossdev).

However, this approach has three major problems which makes it complete unusable:
 - The `crossdev` tool assumes that a kernel always exist and is also quite buggy.
 - The documentation is not in a good shape.
 - The provided tool chain is outdated.
Despite the documentation claiming otherwise, Crossdev seems to focus on cross-compiling Linux packages (specifically Gentoo, of course) for a different architecture which runs a Linux kernel.
This makes some things a bit easier as the Linux Kernel already abstracts away a lot of the differences of the underlying architecture through syscalls.
In particular, one C library (GNU C Lib, MUSL, etc.) can target several platforms, because it doesn't need to implement the low-level hardware-specific aspects.
However, in embedded programming we don't have a kernel, so this approach isn't viable.
On the one hand, the documentation is quite incomplete and misses descriptions for the most basic settings (e.g. allowed and correct values for the target triplet).
On the other hand, it describes features which simply do not exist or which might have been planned, but never realized.
The provided toolchain does not seem to have been updated since the first Arduinos were released.
It does not know ESP32, not even the very first ESP32 (without any suffix).

Bottom line is, this approach doesn't work.
I order to make it work, one must probably fix and extend `crossdev` considerably.
But it probably would be easier to port and package [CrosstoolNG](https://crosstool-ng.github.io/) for Gentoo.
It tries to solve the same problem as `crossdev`, i.e. setting up a toolchain for cross-compilation, but CrosstoolNG is neither limited to Gentoo nor a Gentoo-brewed solution.
Also, Espressif seems to use CrosstoolNG themselves to built their own toolchain.

The following guide is just conserved for historic reasons.
It's incomplete and not working.
 
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
## Setting up Build Environment (Espressif Way)

 1. Emerge `dev-embedded/espressif-eim`
 2. Run `eim wizard --skip-prerequisites-check true` as a **normal user**
     - Arrows keys for navigation, space key for toggling, enter key for confirmation
     - The following error is expected and can be ignored as `dev-embedded/openocd` has already installed that rules in `/usr/lib/udev/rules.d/60-openocd.rules`
       ```
       ERROR - Failed to copy OpenOCD rules: Failed to copy ~/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32/share/openocd/contrib/60-openocd.rules to /etc/udev/rules.d/60-openocd.rules . Make sure you have the necessary permissions. Now you can copy it manually.
       Caused by:
           Permission denied (os error 13)
       ```
     - The final messages should look like
       ```
       Added environment variable ESP_IDF_VERSION = 6.0
       Added environment variable IDF_TOOLS_PATH = ~/.espressif/tools
       Added environment variable IDF_COMPONENT_LOCAL_STORAGE_URL = file://~/.espressif/tools
       Added environment variable IDF_PATH = ~/.espressif/v6.0.2/esp-idf
       Added environment variable ESP_ROM_ELF_DIR = ~/.espressif/tools/esp-rom-elfs/20241011/
       Added environment variable OPENOCD_SCRIPTS = ~/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32/share/openocd/scripts
       Added environment variable IDF_PYTHON_ENV_PATH = ~/.espressif/tools/python/v6.0.2/venv
       Added environment variable ESP_CLANG_LIBS_PATH = ~/.espressif/tools/esp-clang-libs/esp-20.1.1_20250829/esp-clang/lib
       Added proper directory to PATH
       Activated virtual environment at ~/.espressif/tools/python/v6.0.2/venv
       Environment setup complete for the current shell session.
       These changes will be lost when you close this terminal.
       You are now using IDF version 6.0.2.
       NOTICE: Collecting local storage from folder "~/.espressif/tools"
       NOTICE: 0 components loaded from "~/.espressif/tools" folder
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
              source "~/.espressif/tools/activate_idf_v6.0.2.sh"
       ============================================
       2026-08-01 16:14:42 -  4 - 08 - INFO - Wizard result: Ok
       2026-08-01 16:14:42 -  4 - 08 - INFO - Successfully installed IDF
       2026-08-01 16:14:42 -  4 - 08 - INFO - Now you can start using IDF tools
       ```
