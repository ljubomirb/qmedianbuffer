# qmedianbuffer

**Small circular queue buffer with median.**

**Usecase**:
Sometimes you just need to get median value out of 5-10 entries on low resources, Arduino type systems.
Most often, those values are simple uint8_t or uint16_t, and there is no need for bigger or more complex solutions. 

