# SoapyTujaSDR

This is a [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) driver for TujaSDR.

## TODO

* Implement transmit
* Move i2c control to userspace.

## Building

You need [meson](https://mesonbuild.com/) and [ninja](https://ninja-build.org/).

Depends on SoapySDR and ALSA (often called libasound2-dev).

```bash
cd SoapyTujaSDR
meson build
ninja -C build
```
