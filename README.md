# pico-pedal
Very much work-in-progress.

# High-level architecture

At a very high level, samples are read in from an ADC via the SPI bus, are processed, and are then output again. I may add hardware design documents at some point (gerbers, schematics, BoM etc).

## Multicore features

There isn't sufficient processing power on this thing to be able to do any interesting FX processing with a single core. So we dedicate one core to handling I/O and the other core to doing FX processing. The cores communicate using the built-in FIFOs, and use these for sync.

We add an additional sample of latency here on purpose; the I/O core always commits a read before a write, but the write is for the sample from the previous cycle. By ensuring the I/O core always reads one sample ahead and writes one sample behind we ensure that one core only blocks on the other when it has completed its task ahead of its deadline.

The scheduling deadline is enfoced using a repeating alarm. This alarm goes off every 1/44100 th of a second, and triggers a RW pair from the I/O core. If the FX core has completed its task ahead of the deadline, it will block on the FIFO for the next sample from the I/O core. The I/O core will block on the FX core when waiting for a sample and on the repeating alarm before a RW cycle.

Deadlines are not enforced rigidly; if one sample is missed due to an execution overrun, the next sample will also be intentionally missed. If deadlines are repeatedly missed, I think things will get pretty ugly pretty quickly and it will probably just crap its pants and lock up.

# List of FX currently supported


# Known issues
