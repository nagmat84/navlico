# Hardware Implementation — Pin Assignment and GPIOs

## Pin Overview

The following table shows an overview on the available GPIOs of the ESP32-H2 incl. how the pins are used on the evaluation board ESP32-H2-DevKitM-1.
This table has been compiled from various sources:
- [Espressif: ESP32-H2 Datasheet](https://documentation.espressif.com/esp32-h2_datasheet_en.pdf)
- [Espressif: ESP32-H2 Technical Reference Manual](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
- [Espressif: ESP32-H2 Evaluation Board Schematics](https://dl.espressif.com/dl/schematics/esp32-h2-devkitm-1_v1.3_schematics.pdf)
- [Espressif: ESP32-H2 esp-dev-kits Documentation](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32h2/esp-dev-kits-en-master-esp32h2.pdf)

The columns have the following meaning:
- **µC Pin #:** The pin number directly at the µC package
- **Module Pin #:** The evaluation board does not use the µC directly,
  but the module [ESP32-H2-MINI-1-H42](https://www.espressif.com/sites/default/files/documentation/esp32-h2-mini-1_mini-1u_datasheet_en.pdf).
  This column contains the module pin.
- **GPIO #:** The GPIO number
- **Dual Purpose:** Most GPIO have a dual purpose for another peripheral.
  This column only list dual purposes which are of particular importance for debugging, programming or clock stability
  as those purposes might prevent the pin to be used as GPIO.
  The column doesn't list irrelevant dual purposes (like CAN, I²S, etc.) which we are not going to use anyway.
- **Power Pin:** The pin which provides power to the GPIO, see [Espressif: ESP32-H2 — Technical Reference Manual, Sec. 6.11.1](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
- **Low Power:** Indicates whether the GPIO is able to wake up the µC from deep sleep.
- **Strapping:** Indicates whether the pin has a special purpose at boot/reset time.
  If the function is put in paratheses (e.g. as in "(JTAG Source)"), this means that the strapping function is disabled by default.
- **Reset behaviour:** Indicates whether the µC itself uses the pin in any special way during boot time.
  "Pulled-up" means the µC pulls it up to Vdd with its own internal 45kΩ resistor.
  This may restrict how the GPIO can be used as an externally, connected MOSFET might become activated.
  If the column is empty, the pin is floating at high-impedance.
- **Eval Board Usage:** Shows how the eval board uses that pin.
  This may limit the pin's usefulness as the eval board may interfere with it.
- **Remark:** Additional remarks

| µC Pin # | Module Pin # |  GPIO # | Dual Purpose      | Power Pin     | Low Power | Strapping Pin | Reset Behaviour | Eval Board Usage             | Remark                                                                                                                  |
|---------:|-------------:|--------:|:------------------|:--------------|:---------:|:-------------:|:----------------|:-----------------------------|:------------------------------------------------------------------------------------------------------------------------|
|        3 |            9 |       0 |                   | VDDPST1       |           |               |                 |                              |                                                                                                                         |
|        4 |           10 |       1 |                   | VDDPST1       |           |               |                 |                              |                                                                                                                         |
|        5 |            5 |       2 | JTAG (MTMS)       | VDDPST1       |           |               |                 |                              |                                                                                                                         |
|        6 |            6 |       3 | JTAG (MTDO)       | VDDPST1       |           |               |                 |                              |                                                                                                                         |
|        7 |           18 |       4 | JTAG Clock (MTCK) | VDDPST1       |           |               | Pulled-up       |                              | Pulled-up during boot unless `EFUSE_DIS_PAD_JTAG` is blown                                                              |
|        8 |           19 |       5 | JTAG (MTDI)       | VDDPST1       |           |               |                 |                              |                                                                                                                         |
|       10 |           22 |       8 |                   | VDDPST1       |     ✔     |   Download    |                 | Pulled up with 3.3k, RGB LED |                                                                                                                         |
|       11 |           23 |       9 |                   | VDDPST1       |     ✔     |   SPI Boot    | Pulled-up       | "Boot" Button can pull down  |                                                                                                                         |
|       12 |           20 |      10 |                   | VDDPST1       |     ✔     |               |                 |                              |                                                                                                                         |
|       13 |           21 |      11 |                   | VDDPST1       |     ✔     |               |                 |                              |                                                                                                                         |
|       14 |           16 |      12 |                   | VDDA_PMU/VBAT |     ✔     |               |                 |                              |                                                                                                                         |
|       15 |           12 |      13 | XTAL_32K_P        | VDDA_PMU/VBAT |     ✔     |               |                 | 32k-crystal                  |                                                                                                                         |
|       16 |           13 |      14 | XTAL_32K_N        | VDDA_PMU/VBAT |     ✔     |               |                 | 32k-crystal                  |                                                                                                                         |
|       21 |           24 |      22 |                   | VDDPST2       |           |               |                 |                              |                                                                                                                         |
|       22 |           30 |      23 | UART RX           | VDDPST2       |           |               | Pulled-up       | USB-to-UART (CP2102N)        |                                                                                                                         |
|       23 |           21 |      24 | UART TX           | VDDPST2       |           |               | Pulled-up       | USB-to-UART (CP2102N)        |                                                                                                                         |
|       24 |           25 |      25 |                   | VDDPST2       |           | (JTAG Source) |                 |                              | Per default, no strapping function. Only if `EFUSE_JTAG_SEL_ENABLE` is blown, the strapping functionality is effective. |
|       25 |           26 |      26 | USB D-            | VDDPST2       |           |               |                 | USB                          |                                                                                                                         |
|       27 |           27 |      27 | USB D+            | VDDPST2       |           |               | Pulled-up       | USB                          |                                                                                                                         |

### Strapping Pins for Boot Control (GPIO 8+9)

_Summary/TLDR:_ As long as GPIO 9 remains pulled up during boot time everything is fine and GPIO 8 can be used freely.
GPIO 25 has no strapping functionality by default and can be used freely, too.
GPIO 9 can safely be used
- as an output pin in combination with a MOSFET as long as it doesn't matter that it shortly becomes active during boot or
- as an open-drain input which is pulled-up during idling anyway

More precisely, the strapping functionality of the GPIO 8+9 works as follows: 
- **GPIO 9:** GPIO 9 is the most important strapping pin.
  The µC has an internal flash which stores the 1st and 2nd stage bootloader as well as the user-defined program.
  The flash is connected to the µC core via SPI.
  For that reason the strapping function is called "SPI boot" (IMHO, "normal boot" would have been a better name).
  GPIO 9 determines whether the µC
  - boots normally from its internal flash (GPIO 9 = high, default) or
  - via an alternative way (GPIO 9 = low).
  
  The µC itself pulls GPIO 9 up via it's internal 45k resistor.
  This ensures SPI boot under normal conditions.
  As long as GPIO 9 is high/pulled up during reset/boot time, all other strapping pins have no effect.
- **GPIO 8:** GPIO 8 is secondary to GPIO 9 and determines what type of alternative boot method applies.
  GPIO 8 has only an effect, if and only if GPIO 9 pulled down.
  GPIO 8 determines whether the
  - µC accepts a download from USB-Serial-JTAG or UART (GPIO 8 = high, called "Joint Download Boot mode") or
  - user can directly write into the SPI flash (GPIO 8 = low, called "SPI Download Boot Mode").
  
  For the latter to work GPIO 2 and 3 must also be specifically pulled up/down.

The following table from
[Espressif: ESP32-H2 Technical Reference Manual, Sec. 8.2.2 "Boot Mode Control"](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
summarizes this. "×" means don't care or no effect.

| GPIO 9 | GPIO 8 | GPIO 3 | GPIO 2 | Boot mode                                                                                          |
|-------:|-------:|-------:|-------:|:---------------------------------------------------------------------------------------------------|
|      1 |      × |      × |      × | **SPI boot:** Normal boot from internal flash via SPI                                              |
|      0 |      1 |      × |      × | **Joint Download Boot mode:** Download, flash and boot program from either USB-Serial-JTAG or UART |
|      0 |      0 |      0 |      1 | **SPI Download Boot mode:** Download program directly into flash via SPI                           |
|      0 |      0 |      × |      × | Invalid combination, undefined behaviour                                                           |

### Strapping Pins for JTAG Source Control (GPIO 25)

The strapping functionality of GPIO 25 is only active under very specific conditions.
GIO 25 only determines the JTAG source, if and only if
- `EFUSE_JTAG_SEL_ENABLE` has been blown and
- both JTAG sources (direct JTAG or JTAG-over-USB) are still available, i.e.
  - `EFUSE_DIS_USB_JTAG` has not been blown, and
  - `EFUSE_DIS_PAD_JTAG` has not been blown.

The following table from
[Espressif: ESP32-H2 Technical Reference Manual, Sec. 8.2.4 "JTAG Signal Source Control"](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
summarizes this. "×" means don't care or no effect.

| `EFUSE_DIS_PAD_JTAG` | `EFUSE_DIS_USB_JTAG` | `EFUSE_JTAG_SEL_ENABLE` | GPIO 25 | JTAG Source   | Remark                                    |
|---------------------:|---------------------:|------------------------:|--------:|:--------------|:------------------------------------------|
|                    0 |                    0 |                       0 |       × | USB-2-JTAG    | GPIO 25 has no effect (default)           |
|                    0 |                    0 |                       1 |       0 | Direct JTAG   | Pins MTMS, MTDO, MTCK and MTDI (GPIO 2-5) |
|                    0 |                    0 |                       1 |       1 | USB-2-JTAG    |                                           |
|                    0 |                    1 |                       × |       × | Direct JTAG   | see above                                 |
|                    1 |                    0 |                       × |       × | USB-2-JTAG    |                                           |
|                    1 |                    1 |                       × |       × | JTAG disabled |                                           |

## Relevant eFUSES

### Programming and Debugging Related eFuses  

_Summary/TLDR:_
We blow `EFUSE_DIS_PAD_JTAG` and sacrifice direct JTAG as this allows us to use GPIOs 2-5 freely.
We keep UART and USB (for USB-to-Serial and USB-to-JTAG) intact (for now).
We also keep the µC re-programmable, i.e. GPIO 9 keeps it's strapping functionality.

- `EFUSE_DIS_PAD_JTAG`: Permanently disables (direct) JTAG.  
  This effects the pins MTMS, MTDO, MTCK and MTDI (GPIO 2-5).
  If the eFuse is blown, GPIO 4 won't be pulled up durig boot anymore and is free to be used as a GPIO.
  One can still use JTAG via the USB-to-JTAG bridge. 
- `EFUSE_SOFT_DIS_JTAG`: Softly disables (direct) JTAG.  
  This eFuse consists of three bits and an odd number of `1` disables JTAG.
  In contrast to `EFUSE_DIS_PAD_JTAG`, this allows to re-enable JTAG exactly one time after it has been disabled once.
  - `000`: JTAG is enabled (default)
  - `100`: JTAG is disabled (1st time)
  - `110`: JTAG is re-enabled
  - `111`: JTAG is disabled forever.
- `EFUSE_DIS_USB_JTAG`: Permanently disables the USB-to-JTAG bridge.  
  We don't want that, but keep the USB-to-JTAG bridge.
- `EFUSE_JTAG_SEL_ENABLE`: Allows Selection of the JTAG signal source  
  See section on ["Strapping Pins for JTAG Source Control"](#strapping-pins-for-jtag-source-control-gpio-25) above.
  We don't want this, but keep GPIO 25 freely available.
  We disable direct JTAG anyway, so the USB-to-JTAG bridge remains the only selectable source. 
- `EFUSE_DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE`: Disables USB-Serial-JTAG download  
  We don't want that, but keep option to download programs via the USB-to-JTAG bridge.
- `EFUSE_DIS_DOWNLOAD_MODE`: Disables any kind of download mode  
  This would free GPIO 9, because the µC will always boot from SPU, but this make the program basically immutable.
  We don't want that, but keep the µC re-programmable.

### Other Boot-related eFuses

_Summary/TLDR:_ Nothing here. Keep all eFuses at their default.

- `EFUSE_DIS_DIRECT_BOOT`: Disables direct boot mode
  We don't want that.
  Normally, the bootloader copies the program into RAM and executes the program from RAM.
  With "direct boot" (better: direct progam execution), the program is not copied into RAM but directly executed from
  flash storage.
  Running the program directly from flash is considerably slower than running the program vom RAM.
  We do not need it, we have sufficient RAM.
- `EFUSE_DIS_USB_SERIAL_JTAG_ROM_PRINT`: Disable messages from early boot via the USB-Serial bridge
  Not printing the early boot messages saves some time and power consumption, but makes debugging harder.
  We don't want this.
- `EFUSE_UART_PRINT_CONTROL`: Selects how and which messages from early boot are printed via UART
  See previous bullet point, we keep the default behaviour. 
 - `EFUSE_VDD_SPI_AS_GPIO`: Allows the VDD_SPI pin to be used as a regular GPIO  
   Unclear as there is no pin which has a dual purpose as VDD_SPI.
   Otherwise, this might be an interesting option to get an additional GPIO.

## Realized Implementation Variant

We choose a variant with **independent navigational and indicator lights**.
_Rationale:_ Keeping outputs for navigational and indicator lights separate
- requires more GPIOs, but
- allows extra features beyond the actual core functionality such as
  - dimming indicator lights via PWM in dark environments
  - testing of navigational lights via "blinking" at the end of the startup phase

For the **indicator lights** we use outputs in **push-pull mode** with **low-side switching**.
Using push-pull mode allows us to use PWM to dim the indicator lights.
For the **navigational lights** we use the outputs in **open-drain mode** with a **level shifter** and **high-side switching**.
Using open-drain mode allows us to use combined input/output pins and save some pins. 
For details see  [Hardware Implementation — Component Selection and Dimensioning](hw-impl-comp-selection.md).

Suitable input and output pins must meet the following criteria:
1. Output pins must be in a steady state during each operational state
2. Input pins can only be combined with output pins that become active during the operational state associated to the input pin

_Rationale:_
Criterion 1 ensures that an actually intended button press (which pulls down the input) can be distinguished from the input going down,
because the output isn't steady.
Criterion 1 rules out that output pins for indictor pins can be combined as those might be PWM driven.
A PWM signal on the combined input/output pin blinds the input for an actual button press.
Criterion 2 is necessary as the output is active low and a button press pulls down the input, too.
An active output on a combined input/output pin blinds the input for a button press.

This allows us to combine the following pins:
- Driving Button & Masthead Light
- Sailing Coast Button & Allround Green Light
- Disabled Button & Allround Red 2 Light
- Anchoring Button & Allround White Light

### Alternative "Variant 1: Independent Full-Fledged"

We do **not** realize [**Variant 1: Independent Full-Fledged**](fsm.md#variant-1-independent-full-fledged).
This variant requires 13 GPIOs as shown in the following table (I = input, O-od = output, open-draino O-pp = output, push-pull):

|  # | Direction | Input Function    | Output Function         | Wake-Up From Deep Sleep |
|---:|:---------:|:------------------|:------------------------|:-----------------------:|
|  1 |     I     | Off Btn           |                         |                         |
|  2 |     I     | Sailing Btn       |                         |            ✔            |
|  3 |  I, O-od  | Sailing Coast Btn | Allround Green Light    |            ✔            |
|  4 |  I, O-od  | Driving Btn       | Masthead Light          |            ✔            |
|  5 |  I, O-od  | Anchoring Btn     | Allround White Light    |            ✔            |
|  6 |  I, O-od  | Disabled Btn      | Allround Red 2 Light    |            ✔            |
|  7 |   O-pp    |                   | Sailing Indicator       |                         |
|  8 |   O-pp    |                   | Sailing Coast Indicator |                         |
|  9 |   O-pp    |                   | Driving Indicator       |                         |
| 10 |   O-pp    |                   | Anchoring Indicator     |                         |
| 11 |   O-pp    |                   | Disabled Indicator      |                         |
| 12 |   O-od    |                   | Side & Stern Light      |                         |
| 13 |   O-od    |                   | Allround Red 1 Light    |                         |

The following table shows the pin allocation.
The usage is assigned in a way such that
the [preferred variant 3](#preferred-variant-3-independent-reduced) is a strict subset of the pin assignment and
avoids dual-purpose GPIOs.

|  GPIO # | Usage | Direction | Dual Purpose      | Low Power | Strapping Pin | Reset Behaviour | Eval Board Usage             |
|--------:|------:|:---------:|:------------------|:---------:|:-------------:|:----------------|:-----------------------------|
|       0 |     1 |     I     |                   |           |               |                 |                              |
|       1 |     7 |   O-pp    |                   |           |               |                 |                              |
|       2 |     9 |   O-pp    | JTAG (MTMS)       |           |               |                 |                              |
|       3 |    10 |   O-pp    | JTAG (MTDO)       |           |               |                 |                              |
|       4 |     8 |   O-pp    | JTAG Clock (MTCK) |           |               | Pulled-up       |                              |
|       5 |    12 |   O-od    | JTAG (MTDI)       |           |               |                 |                              |
|       8 |       |           |                   |     ✔     |   Download    |                 | Pulled up with 3.3k, RGB LED |
|       9 |       |           |                   |     ✔     |   SPI Boot    | Pulled-up       | "Boot" Button can pull down  |
|      10 |     2 |     I     |                   |     ✔     |               |                 |                              |
|      11 |     4 |  I, O-od  |                   |     ✔     |               |                 |                              |
|      12 |     5 |  I, O-od  |                   |     ✔     |               |                 |                              |
|      13 |     3 |  I, O-od  | XTAL_32K_P        |     ✔     |               |                 | 32k-crystal                  |
|      14 |     6 |  I, O-od  | XTAL_32K_N        |     ✔     |               |                 | 32k-crystal                  |
|      22 |    11 |   O-pp    |                   |           |               |                 |                              |
|      23 |       |           | UART RX           |           |               | Pulled-up       | USB-to-UART (CP2102N)        |
|      24 |       |           | UART TX           |           |               | Pulled-up       | USB-to-UART (CP2102N)        |
|      25 |    13 |   O-od    |                   |           | (JTAG Source) |                 |                              |
|      26 |       |           | USB D-            |           |               |                 | USB                          |
|      27 |       |           | USB D+            |           |               | Pulled-up       | USB                          |

This solution needs GPIO 4 (JTAG Clock), 13 (XTAL_32K_P) and 14 (XTAL_32K_N).
Hence,
- `EFUSE_DIS_PAD_JTAG` must be blown
- external 32k-crystal cannot be used

### Preferred "Variant 3: Independent Reduced"

We realize [**Variant 3: Independent Reduced**](fsm.md#variant-3-independent-reduced).
This variant requires 8 GPIOs as shown in the following table.
The table is a strict subset of the [alternative variant 1](#alternative-variant-1-independent-full-fledged).
Hence, the numbers are non-consecutive.

|  # | Direction | Input Function    | Output Function         | Wake-Up From Deep Sleep |
|---:|:---------:|:------------------|:------------------------|:-----------------------:|
|  1 |     I     | Off Btn           |                         |                         |
|  2 |     I     | Sailing Btn       |                         |            ✔            |
|  4 |  I, O-od  | Driving Btn       | Masthead Light          |            ✔            |
|  5 |  I, O-od  | Anchoring Btn     | Allround White Light    |            ✔            |
|  7 |   O-pp    |                   | Sailing Indicator       |                         |
|  9 |   O-pp    |                   | Driving Indicator       |                         |
| 10 |   O-pp    |                   | Anchoring Indicator     |                         |
| 12 |   O-od    |                   | Side & Stern Light      |                         |

The following table shows the pin allocation.
This table is identical to the table of the [alternative variant 1](#alternative-variant-1-independent-full-fledged),
but with the omitted usages left out.

|  GPIO # | Usage | Direction | Dual Purpose      | Low Power | Strapping Pin | Reset Behaviour | Eval Board Usage             |
|--------:|------:|:---------:|:------------------|:---------:|:-------------:|:----------------|:-----------------------------|
|       0 |     1 |     I     |                   |           |               |                 |                              |
|       1 |     7 |   O-pp    |                   |           |               |                 |                              |
|       2 |     9 |   O-pp    | JTAG (MTMS)       |           |               |                 |                              |
|       3 |    10 |   O-pp    | JTAG (MTDO)       |           |               |                 |                              |
|       4 |       |           | JTAG Clock (MTCK) |           |               | Pulled-up       |                              |
|       5 |    12 |   O-od    | JTAG (MTDI)       |           |               |                 |                              |
|       8 |       |           |                   |     ✔     |   Download    |                 | Pulled up with 3.3k, RGB LED |
|       9 |       |           |                   |     ✔     |   SPI Boot    | Pulled-up       | "Boot" Button can pull down  |
|      10 |     2 |     I     |                   |     ✔     |               |                 |                              |
|      11 |     4 |  I, O-od  |                   |     ✔     |               |                 |                              |
|      12 |     5 |  I, O-od  |                   |     ✔     |               |                 |                              |
|      13 |       |           | XTAL_32K_P        |     ✔     |               |                 | 32k-crystal                  |
|      14 |       |           | XTAL_32K_N        |     ✔     |               |                 | 32k-crystal                  |
|      22 |       |           |                   |           |               |                 |                              |
|      23 |       |           | UART RX           |           |               | Pulled-up       | USB-to-UART (CP2102N)        |
|      24 |       |           | UART TX           |           |               | Pulled-up       | USB-to-UART (CP2102N)        |
|      25 |       |           |                   |           | (JTAG Source) |                 |                              |
|      26 |       |           | USB D-            |           |               |                 | USB                          |
|      27 |       |           | USB D+            |           |               | Pulled-up       | USB                          |

This solution avoids GPIO 4 (JTAG Clock) and hence `EFUSE_DIS_PAD_JTAG` can be left intact.
