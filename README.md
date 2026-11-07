# flower-cc2530

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

## License

BSD-3-Clause, see [LICENSE](LICENSE).
