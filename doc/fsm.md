# Finite State Machine (STM)

At its core Navlico is based around a finite-state machine.
At every moment Navlico is in one of the following operational states:
- OFF (aka "idling")
- SAILING
- SAILING COAST
- DRIVING
- ANCHORING
- DISABLED (aka unable to steer, emergency)

Input buttons select the next operational state.
There is a one-to-one mapping between input buttons and operational state,
i.e. each input button corresponds to one operational state.
Pressing an input button lets the FSM transit into the target state which corresponds to the button.

There are two categories of outputs:
- indicator lights (aka confirmation or feedback lights)
- navigational lights

Indicator lights represent the current operational state.
For each operational state but OFF, there is one indicator light which is ON when the corresponding operational state is active.

Navigational lights are active/inactive depending on the current operational state.
For each operational state a subset of navigational lights is active, while the complement is inactive.
The mapping is shown in the following table

|               | Side & Stern | Masthead | Allround Green | Allround Red 1 | Allround Red 2 | Allround White |
|:--------------|:------------:|:--------:|:--------------:|:--------------:|:--------------:|:--------------:|
| OFF           |              |          |                |                |                |                |
| SAILING       |      ✔       |          |                |                |                |                |
| SAILING COAST |      ✔       |          |       ✔        |       ✔        |                |                |
| DRIVING       |      ✔       |    ✔     |                |                |                |                |
| ANCHORING     |              |          |                |                |                |       ✔        |
| DISABLED      |              |          |                |       ✔        |       ✔        |                |

## Implementation Variants

There are two aspects which allow to implement simplified variants:
- **Independence of Outputs:** Comparison of the indicator lights and the navigational lights shows that some of the lights are always switched together
  (for example the Driving Indicator and the Masthead Light are either on or off simultaneously).
  This allows to merge some outputs.
- **Number of States:** The operational states SAILING COAST and DISABLED are optional.
  This allows to omit two inputs and some outpus.

This yields four implementation variants:

1. **Independent Full-Fledged:** all states, independent outputs
2. **Merged Full-Fledged:** all states, merged outputs
3. **Independent Reduced:** reduced states, independent outputs
4. **Merged Reduced:** reduced states, merged outputs

To find mergeable outputs on has to identify pairs of identical columns.
These are:
 - Driving Indicator & Masthead Light
 - Sailing Coast Indicator & Allround Green Light
 - Disabled Indicator & Allround Red 2 Light
 - Anchoring Indicator & Allround White Light

### Variant 1: Independent Full-Fledged

|               | Sailing | Sailing Coast | Driving | Anchoring | Disabled | Side & Stern | Masthead | Allround Green | Allround Red 1 | Allround Red 2 | Allround White |
|:--------------|:-------:|:-------------:|:-------:|:---------:|:--------:|:------------:|:--------:|:--------------:|:--------------:|:--------------:|:--------------:|
| OFF           |         |               |         |           |          |              |          |                |                |                |                |
| SAILING       |    ✔    |               |         |           |          |      ✔       |          |                |                |                |                |
| SAILING COAST |         |       ✔       |         |           |          |      ✔       |          |       ✔        |       ✔        |                |                |
| DRIVING       |         |               |    ✔    |           |          |      ✔       |    ✔     |                |                |                |                |
| ANCHORING     |         |               |         |     ✔     |          |              |          |                |                |                |       ✔        |
| DISABLED      |         |               |         |           |    ✔     |              |          |                |       ✔        |       ✔        |                |

### Variant 2: Merged Full-Fledged

|               | Sailing | Sailing Coast & Allround Green | Driving & Masthead | Anchoring & Allround White | Disabled & Allround Red 2 | Side & Stern | Allround Red 1 |
|:--------------|:-------:|:------------------------------:|:------------------:|:--------------------------:|:-------------------------:|:------------:|:--------------:|
| OFF           |         |                                |                    |                            |                           |              |                |
| SAILING       |    ✔    |                                |                    |                            |                           |      ✔       |                |
| SAILING COAST |         |               ✔                |                    |                            |                           |      ✔       |       ✔        |
| DRIVING       |         |                                |         ✔          |                            |                           |      ✔       |                |
| ANCHORING     |         |                                |                    |             ✔              |                           |              |                |
| DISABLED      |         |                                |                    |                            |             ✔             |              |       ✔        |

### Variant 3: Independent Reduced

|           | Sailing | Driving | Anchoring | Side & Stern | Masthead | Allround White |
|:----------|:-------:|:-------:|:---------:|:------------:|:--------:|:--------------:|
| OFF       |         |         |           |              |          |                |
| SAILING   |    ✔    |         |           |      ✔       |          |                |
| DRIVING   |         |    ✔    |           |      ✔       |    ✔     |                |
| ANCHORING |         |         |     ✔     |              |          |       ✔        |

### Variant 4: Merged Reduced

|           | Sailing | Driving & Masthead | Anchoring & Allround White | Side & Stern |
|:----------|:-------:|:------------------:|:--------------------------:|:------------:|
| OFF       |         |                    |                            |              |
| SAILING   |    ✔    |                    |                            |      ✔       |
| DRIVING   |         |         ✔          |                            |      ✔       |
| ANCHORING |         |                    |             ✔              |              |
