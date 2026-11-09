# flower-cc2530

> **Branch `flower-gp`: experimental Green Power build.** This branch replaces
> the Zigbee end device with a unidirectional Green Power Device (GPD); see
> [Green Power prototype](#green-power-prototype-flower-gp). Use `main` for
> the regular firmware.

Open firmware for the [faronov/flower](https://github.com/faronov/flower) v2
soil sensor board (DIYRuZ FlowerX, E18-MS1-PCB, CC2530). It is a LEAN, PM2
sleepy Zigbee 3.0 end device built on the
[cc2530-zigbee](https://github.com/faronov/cc2530-zigbee) stack with the
LLVM MCS-51 compiler, and it does not use Z-Stack.

It keeps the DIYRuZ_FlowerX wire identity (manufacturer `modkam.ru`, model
`DIYRuZ_FlowerX`), so the existing flower Zigbee2MQTT converter and ZHA quirk
apply unchanged.

**Status:**

- Works on hardware:
  - It has joined ZHA through a router parent and reached READY.
  - The quirk interview completed.
  - Battery, soil moisture, illuminance, chip temperature and TX power report.
- Not yet qualified:
  - sleep current;
  - BME280 and DS18B20 sensors;
  - the button, the LED and NV persistence of the interval.

See [docs/FIRMWARE.md](docs/FIRMWARE.md) for details.

## Layout

| Path | Contents |
|---|---|
| `cc2530-zigbee/` | the stack (git submodule) |
| `cc2530-app.json` | [external application](https://github.com/faronov/cc2530-zigbee/blob/main/docs/EXTERNAL_APP.md) manifest: stack options and replaced units |
| `include/app_config.h` | board identity, pins, CCA threshold, Timer 3 SFRs |
| `include/app_zcl.h`, `src/zcl_flower.c` | ZCL application server and sensor cycle |
| `boards/flower_v2.c` | early pin parking |
| `tests/test_zcl_flower.c` | host tests against modeled ADC and Timer 3 |
| `include/gpd_frame.h`, `src/gpd_main.c`, `tests/test_gpd_frame.c` | Green Power frame builder, GPD runtime and host test (flower-gp) |
| `quirks/` | ZHA Green Power quirk and its pytest test (flower-gp) |

## Build

```sh
git clone --recursive https://github.com/faronov/flower-cc2530.git
cd flower-cc2530
make test                                   # host tests (cc, ASan/UBSan)
make firmware LLVM_TOOLS=<llvm-build>/bin LLVM_SRC=<llvm-project>
```

`make firmware` needs Python 3 with `pyelftools`. It also needs the LLVM
MCS-51 compiler built from
[faronov/llvm-project](https://github.com/faronov/llvm-project) at the
revision pinned in `cc2530-zigbee/tools/llvm/toolchain.json`; see the stack's
[LLVM_DEVELOPMENT.md](https://github.com/faronov/cc2530-zigbee/blob/main/docs/LLVM_DEVELOPMENT.md).
The image, its audit and its sealed receipt are written to `build/firmware/`
(`firmware.hex`).

CI builds the compiler and the firmware on every push and uploads the HEX as an
artifact.

## Flash

```sh
cc-tool -e -w build/firmware/firmware.hex
```

Then permit joining in your coordinator. The device scans all channels and can
join through a router.

## Green Power prototype (flower-gp)

`src/gpd_main.c` replaces the stack's join unit (`join_smoke_main` in
`cc2530-app.json`, built with `FLOWER_GPD`). The device never joins a
network. It reuses the regular sensor cycle and broadcasts raw GPDFs:

- Frame type: Green Power Device Frames, ApplicationID 0 (SrcID), **no
  security**, unidirectional, with the MAC sequence number as the frame
  counter.
- Channel: **15**, fixed (`GPD_CHANNEL`).
- SrcID: the low 32 bits of the chip IEEE address, avoiding the reserved
  values.
- Commissioning: for the first 12 rounds, every 10 s, it sends a
  Commissioning frame (0xE0) followed by a report. The Commissioning frame
  uses DeviceID 0x33, manufacturer 0x6D6B, model 0xF100, and server clusters
  0x0001, 0x0400, 0x0402 and 0x0408.
- Reports: after commissioning, a Multi-Cluster Reporting frame (0xA2) every
  `GPD_INTERVAL` (60 s). It carries temperature (0x0402), soil moisture
  (0x0408), illuminance (0x0400), battery voltage (0x0001/0x0020) and battery
  remaining (0x0001/0x0021), all in their ZCL encodings.
- Repeats and sleep: each GPDF is sent 3 times, and the device sleeps in PM2
  between rounds.

### Pairing with ZHA

1. Copy `quirks/zha_quirk_diyruz_flowerx_gp.py` to
   `<HA config>/custom_zha_quirks/` and restart Home Assistant. This needs a
   ZHA build with Green Power support.
2. Put the coordinator network on channel 15, or change `GPD_CHANNEL`.
3. Permit joining, then power the sensor within two minutes.

### Tests

```sh
make test-gp                                # frame builder (also run by make test)
cd quirks && python -m pytest test_zha_quirk_diyruz_flowerx_gp.py   # zha, zha-quirks
```

### Status

**Checked:**

- Host tests pass.
- zigpy 2.4.1 parses both frames.
- The quirk passes against zha 2.4.2 and zha-quirks 2.4.1.
- In MCUEMU the image transmits the expected frames on channel 15 and wakes
  from PM2.

**Not yet verified on hardware:**

- whether the coordinator's NCP forwards GPDFs;
- sleep current.

## License

BSD-3-Clause, see [LICENSE](LICENSE).
