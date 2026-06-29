# RaSTA

C library implementation of the Rail Safe Transport Application (RaSTA) protocol.

## Cloud IXL and Pedro OC local integration

This repository is used as a laboratory prototype of a simulated Cloud IXL
exchanging SCI-LS/PDI payloads with an Object Controller over RaSTA or a
RaSTA-like safe communication transport.

The integration described here was tested against Pedro's Object Controller
repository:

* <https://github.com/pedrobaena01/rasta-protocol-ff>

This is a PoC implementation and a repeatable local integration test. It is not
a normative SCI-LS, EULYNX, RaSTA, EN 50159, or SIL4 compliance claim.

### Interface values used in the local test

| Item | Cloud IXL | Pedro OC |
|---|---:|---:|
| RaSTA ID | `0x62` | `0x61` |
| SCI node name | `IXL_CENTRAL` | `LS_OC` |
| PDI version | `0x05` | `0x05` |
| OC UDP channels | connects to `8888/8889` | listens on `8888/8889` |

The local Cloud IXL configuration is `config/rasta_client1_local.cfg`.
For readable manual runs, its `LOGGER_MAX_LEVEL` is set to `1` so that RaSTA
debug logs do not dominate the terminal.

### Build both sides

Keep both repositories as sibling directories under `/home/pablo`:

```bash
cd /home/pablo
git clone https://github.com/pedrobaena01/rasta-protocol-ff.git
```

Build Pedro's local OC:

```bash
cd /home/pablo/rasta-protocol-ff
cmake -S . -B build
cmake --build build --target ls_oc_local -j2
```

Build this Cloud IXL:

```bash
cd /home/pablo/tfm-cloud-ixl-rasta
cmake --build build --target cloud_ixl -j2
```

### Run the local integration

Terminal 1, start Pedro's OC first:

```bash
cd /home/pablo/rasta-protocol-ff/build/bin/exe/examples
./ls_oc_local
```

Expected startup evidence:

```text
Starting SCI-LS Object Controller as RaSTA Server/Downstream.
Listening as LS RaSTA ID 0x61 / SCI node LS_OC.
```

Terminal 2, start the Cloud IXL from this repository root:

```bash
cd /home/pablo/tfm-cloud-ixl-rasta
./build/bin/exe/examples/cloud_ixl
```

Expected establishment evidence on the Cloud IXL side:

```text
Completed RaSTA handshake with LS_OC
PDI: ICD version response received
PDI: initial status transfer started
PDI: brightness status received: 0x01
PDI: initialisation completed
```

### Manual checks

At the Cloud IXL prompt, run these checks:

| Input | Expected Cloud IXL result | Expected Pedro OC result |
|---|---|---|
| `r`, then `1` | `Decision: GO`, `Command and message match` | `vector=[01 ...] -> GREEN` |
| `r`, then `1` again | `Decision: STOP`, `Command and message match` | `vector=[02 ...] -> RED` |
| `l`, then `1` | route `0` becomes `FREE` after feedback | `vector=[02 ...] -> RED` |
| `r`, then `2` | incompatible point, `Decision: STOP` | `vector=[02 ...] -> RED` |

The first request validates the nominal route command:

```text
RUTA_AB -> GO -> VIA_LIBRE -> GREEN -> feedback matches
```

The rejection and release checks validate that the Cloud IXL waits for the OC
feedback before accepting the commanded signal state.

### Current compatibility limits

The Cloud IXL exposes a direct laboratory aspect command for these project
interface values only:

```text
0=VIA_LIBRE
1=PARADA
2=ANUNCIO_PARADA
4=ANUNCIO_PRECAUCION
5=REBASE
6=PARADA_SELECTIVA_N2
7=REBASE_AUTORIZADO
9=APAGADA
```

The entries `VIA_LIBRE_CONDICIONAL` and `PARADA_SELECTIVA_N1` remain commented
out in `src/cloud_ixl/headers/cloud_ixl_types.h` and
`src/cloud_ixl/c/cloud_ixl_scils_adapter.c`. Do not use aspects `3` or `8`
unless the interface contract and the OC implementation used in the lab have
explicitly been updated for them.

To stop the test, enter `q` in the Cloud IXL terminal and then `q` in Pedro's OC
terminal.

## Physical laboratory run with Pedro's OC

Use this procedure only as a laboratory prototype test. It is intended to check
integration with Pedro's physical Object Controller and signal hardware. It is
not evidence of compliance with EN 50159, EULYNX, SCI-LS, RaSTA, or SIL4.

### Before energising the signal

Check and record the actual laboratory IP addresses. The example below assumes:

| Item | Address |
|---|---:|
| Pedro OC channel 1 | `10.0.0.100:8888` |
| Pedro OC channel 2 | `10.0.0.101:8889` |
| Cloud IXL channel 1 | `10.0.0.200:9998` |
| Cloud IXL channel 2 | `10.0.0.201:9999` |

Verify that `config/rasta_client1_lab.cfg` matches the Cloud IXL local
addresses. It uses checksum `NONE` to match Pedro's current OC configuration.

Start with the signal outputs de-energised or otherwise isolated according to
the laboratory procedure. First prove RaSTA/PDI communication, then energise the
physical signal under supervision.

### Pedro OC on the Raspberry Pi

Build Pedro's OC with GPIO support on the Raspberry Pi:

```bash
cd /home/pablo/rasta-protocol-ff
cmake -S . -B build-gpio -DGPIO_ENABLE=ON
cmake --build build-gpio --target ls_oc -j2
```

Run the OC:

```bash
cd /home/pablo/rasta-protocol-ff/build-gpio/bin/exe/examples
./ls_oc
```

If the GPIO library requires elevated privileges or a running daemon, follow the
laboratory Raspberry Pi setup. Do not bypass hardware safety procedures from
the application code.

### Cloud IXL against the physical OC

Build this repository:

```bash
cd /home/pablo/tfm-cloud-ixl-rasta
cmake -S . -B build
cmake --build build --target cloud_ixl -j2
```

Run the Cloud IXL with the laboratory RaSTA config and OC destination channels:

```bash
CLOUD_IXL_RASTA_CONFIG=config/rasta_client1_lab.cfg \
CLOUD_IXL_OC_CH1_IP=10.0.0.100 CLOUD_IXL_OC_CH1_PORT=8888 \
CLOUD_IXL_OC_CH2_IP=10.0.0.101 CLOUD_IXL_OC_CH2_PORT=8889 \
./build/bin/exe/examples/cloud_ixl
```

Expected startup evidence:

```text
Cloud IXL RaSTA config: config/rasta_client1_lab.cfg
OC RaSTA channels: 10.0.0.100:8888 and 10.0.0.101:8889
Completed RaSTA handshake with LS_OC
PDI: initialisation completed
```

### Minimal physical checks

Run the already validated route command path first:

| Input | Expected protocol result | Physical expectation |
|---|---|---|
| `r`, then `1` | `Decision: GO`, `Command and message match` | signal shows green |
| `l`, then `1` | route `0` becomes `FREE` after feedback | signal returns to red |
| `r`, then `2` | incompatible point, `Decision: STOP` | signal stays or returns red |

Then use the direct laboratory aspect command. Enter `a` at the command prompt
and then the aspect number:

| Aspect input | Expected protocol result | Expected physical mapping in this PoC |
|---:|---|---|
| `0` | `Manual signal aspect: VIA_LIBRE`, `Command and message match` | green |
| `1` | `Manual signal aspect: PARADA`, `Command and message match` | red |
| `2` | `Manual signal aspect: ANUNCIO_PARADA`, `Command and message match` | yellow |
| `4` | `Manual signal aspect: ANUNCIO_PRECAUCION`, `Command and message match` | green + yellow |
| `5` | `Manual signal aspect: REBASE`, `Command and message match` | red + white |
| `6` | `Manual signal aspect: PARADA_SELECTIVA_N2`, `Command and message match` | red + blue |
| `7` | `Manual signal aspect: REBASE_AUTORIZADO`, `Command and message match` | red + flashing white |
| `9` | `Manual signal aspect: APAGADA`, `Command and message match` | all lamps off |

The direct aspect command bypasses route selection and is intended only for this
laboratory prototype test.

## Deployment

### Unit tests

see [CUnit HowTo](md_doc/cunit.md)  

### Gradle

see [Gradle HowTo](md_doc/gradle.md)  

### How to use the RaSTA library

see [Getting started](md_doc/getting_started.md)  

### Raspberry Pi / ARM architecture

see [Building on Raspberry Pi](md_doc/raspberry_pi.md)

### Docker

see [Docker HowTo](md_doc/docker.md)

## Built With

* [Gradle](https://gradle.org/) - The build tool
* [CUnit](http://cunit.sourceforge.net/) - For Unit tests
* [CppCheck](http://cppcheck.sourceforge.net/) - For static code analysis
* [Doxygen](http://www.stack.nl/~dimitri/doxygen/) - Documentation generation
* [CMake](https://cmake.org/)  - Compilation on Raspberry Pi / ARM

## Contact

This repository is maintained by Markus Heinrich ([rasta@0x25.net](mailto:rasta@0x25.net))
