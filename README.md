# Cloud IXL: SCI-LS sobre RaSTA

Prototipo de laboratorio de un Cloud Interlocking (Cloud IXL) simulado que
intercambia órdenes y estados de una señal luminosa con un Object Controller
(OC). La aplicación utiliza conceptos SCI-LS/PDI en la capa de aplicación y la
biblioteca RaSTA incluida en el repositorio como transporte de comunicación.

Este repositorio deriva de la implementación C de RaSTA de Railway-CCS y añade
el Cloud IXL, el contrato de interfaz SCI-LS del proyecto y herramientas de
ensayo de red.

> [!IMPORTANT]
> Es un prototipo académico de laboratorio. No es una implementación normativa
> de EULYNX, SCI-LS, RaSTA, PoS-Signalling o EN 50159 y no constituye evidencia
> de conformidad, seguridad funcional, SIL4 ni certificación ferroviaria.

## Alcance actual

El flujo principal implementado es:

```text
operador
   |
   v
Cloud IXL -> decisión de ruta -> aspecto/luminosidad -> adaptador ICD
                                                        |
                                                        v
                                             SCI-LS/PDI -> RaSTA -> UDP -> OC
                                                        ^                  |
                                                        |                  v
                                   estado o error <- comparación <- feedback
```

La separación de responsabilidades es la siguiente:

- **Cloud IXL:** mantiene un estado de enclavamiento simplificado, decide las
  rutas, genera órdenes, compara el aspecto ordenado con el informado y registra
  los cambios observables.
- **SCI-LS/PDI:** aporta los telegramas de aplicación y la secuencia de
  establecimiento utilizada por el contrato del proyecto.
- **RaSTA:** transporta los mensajes entre los RaSTA ID `0x62` y `0x61` mediante
  dos canales UDP redundantes.
- **Object Controller:** valida y ejecuta las órdenes, actúa sobre la señal y
  devuelve el estado o un error de ejecución. La implementación funcional del
  OC usada en la integración completa está en un repositorio externo.
- **PoS-Signalling y EN 50159:** forman parte del contexto arquitectónico y de
  seguridad, pero no aparecen como servicios o evidencias normativas
  independientes en este código.

El Cloud IXL implementa actualmente:

- establecimiento RaSTA y PDI con timeout global de 11 segundos;
- comprobación de versión PDI `0x05`;
- solicitud y recepción de la transferencia de estado inicial;
- órdenes de aspecto con payload de 18 bytes según el ICD del proyecto;
- comparación exacta entre el payload ordenado y el informado por el OC;
- recepción de errores de ejecución del OC;
- luminosidad de día y de noche;
- dos rutas simuladas, una aguja, tres circuitos de vía y una señal;
- ensayos de degradación de red en el sentido Cloud IXL hacia OC.

No implementa un enclavamiento ferroviario completo ni persistencia, entradas de
campo reales, gestión dinámica de topología o una pila PoS-Signalling separada.

## Estructura del repositorio

| Ruta | Responsabilidad |
|---|---|
| `src/cloud_ixl/` | Aplicación principal, estado, rutas, entrada, salida y adaptación SCI-LS. |
| `src/sci_ls_icd/` | Codificación y validación del contrato SCI-LS específico del proyecto. |
| `src/sci/` | Funciones SCI y SCI-LS heredadas/adaptadas. |
| `src/rasta/` | Biblioteca de transporte RaSTA en C. |
| `config/` | Configuraciones local y de laboratorio del Cloud IXL y de los ejemplos heredados. |
| `scripts/network/` | Inyección controlada de latencia, pérdida, duplicación, reordenamiento y cortes. |
| `tests/` | Pruebas CUnit heredadas de las bibliotecas RaSTA y SCI. |
| `docs/` | Arquitectura, ICD del proyecto y referencias EULYNX. |
| `extra/` | Ejemplos, diagnósticos, SCI-P, Java, Docker y construcción Gradle heredada. |

El material de `extra/` no participa en el flujo principal Cloud IXL -> Light
Signal OC salvo que se habilite explícitamente para aprendizaje o diagnóstico.

## Requisitos

Para la construcción principal:

- Linux o WSL2 con soporte de colas POSIX y sockets UDP;
- compilador C con soporte de C11;
- CMake `3.6.2` o posterior;
- `make`, Ninja u otro generador admitido por CMake;
- bibliotecas del sistema `pthread` y `rt`.

En ARM, o al configurar `-DUSE_OPENSSL=ON`, también se necesita OpenSSL/
`libcrypto` para MD4. CUnit, Gradle, Docker, Java, Doxygen y Cppcheck no son
necesarios para compilar el Cloud IXL.

Para los ensayos de red se necesitan además Bash, `awk`, `ip` y `tc` de
`iproute2`, junto con permisos de administrador para modificar una qdisc.

## Compilación principal

Desde la raíz del repositorio:

```bash
cmake -S . -B build
cmake --build build --target cloud_ixl -j2
```

El ejecutable queda en:

```text
build/bin/exe/examples/cloud_ixl
```

La biblioteca compartida RaSTA queda en `build/bin/lib/librasta.so` y el
adaptador ICD estático en `build/bin/lib/libsci_ls_icd.a`.

### Opciones CMake

Todas las opciones siguientes están desactivadas por defecto:

| Opción | Efecto |
|---|---|
| `BUILD_SCIP_SUPPORT` | Añade SCI-P para los ejemplos opcionales de controladores de aguja. |
| `BUILD_LOCAL_EXAMPLES` | Construye ejemplos RaSTA locales conservados en `extra/`. |
| `BUILD_REMOTE_EXAMPLES` | Construye ejemplos RaSTA de red conservados en `extra/`. |
| `EXAMPLE_IP_OVERRIDE` | Permite sobrescribir IP en los ejemplos remotos; no afecta a `cloud_ixl`. |
| `USE_OPENSSL` | Usa la implementación MD4 de OpenSSL. |

SCI-P no forma parte del camino Cloud IXL -> OC de señal luminosa.

## Interfaz y configuración

Los valores fijos del contrato usado por la aplicación son:

| Elemento | Cloud IXL | Object Controller |
|---|---:|---:|
| RaSTA ID | `0x62` | `0x61` |
| Nombre SCI | `IXL_CENTRAL` | `LS_OC` |
| Versión PDI | `0x05` | `0x05` |
| Red RaSTA | `1234` | `1234` |
| Versión RaSTA aceptada | `0303` | `0303` |

El Cloud IXL diferencia entre sus endpoints locales, definidos dentro del
fichero RaSTA, y los endpoints de destino del OC, configurables mediante
variables de entorno:

| Variable | Valor predeterminado |
|---|---|
| `CLOUD_IXL_RASTA_CONFIG` | `config/rasta_client1_lab.cfg` |
| `CLOUD_IXL_OC_CH1_IP` | `192.168.0.152` |
| `CLOUD_IXL_OC_CH1_PORT` | `8888` |
| `CLOUD_IXL_OC_CH2_IP` | `192.168.0.152` |
| `CLOUD_IXL_OC_CH2_PORT` | `8889` |

Las variables de destino no cambian las IP o los puertos locales en los que
escucha el Cloud IXL. Para cambiar esos endpoints debe utilizarse un fichero
RaSTA coherente y seleccionarlo con `CLOUD_IXL_RASTA_CONFIG`.

### Perfiles incluidos

| Perfil | Endpoints Cloud IXL | Endpoints OC | Uso |
|---|---|---|---|
| `config/rasta_client1_local.cfg` | `127.0.0.1:9998/9999` | `127.0.0.1:8888/8889` | Integración en una sola máquina. |
| `config/rasta_client1_lab.cfg` | `192.168.0.151:9998/9999` | `192.168.0.152:8888/8889` por defecto | Red física de laboratorio actual. |
| `config/rasta_client1_eulynx_evaluation.cfg` | `192.168.0.151:9998/9999` | `192.168.0.152:8888/8889` por defecto | Subconjunto Eu.Doc.92 compatible unilateralmente con el OC actual. |
| `config/rasta_server_local.cfg` | `127.0.0.1:8888/8889` | Cloud IXL `127.0.0.1:9998/9999` | Ejemplos localhost heredados. |

Los cuatro perfiles usan checksum SR `NONE` para mantener compatibilidad con el
OC utilizado en este PoC. Estos parámetros son decisiones del laboratorio, no
valores normativos de RaSTA o EULYNX. Las diferencias, la diversidad de canales
y la jerarquía temporal se detallan en
[`RASTA_EVALUATION.md`](RASTA_EVALUATION.md).

## Integración local con el OC funcional

La integración completa se diseñó y probó con el Object Controller externo:

- <https://github.com/pedrobaena01/rasta-protocol-ff>

Ese repositorio no forma parte de este proyecto y puede evolucionar de forma
independiente. La revisión actual de este repositorio no puede garantizar sus
targets, dependencias o configuración; compruébelos en su documentación. En la
integración utilizada por este PoC, el ejecutable local del OC era
`ls_oc_local`, con RaSTA ID `0x61`, nombre `LS_OC` y escucha en
`127.0.0.1:8888/8889`.

Arranque primero el OC funcional. Después ejecute el Cloud IXL con el perfil
loopback explícito:

```bash
CLOUD_IXL_RASTA_CONFIG=config/rasta_client1_local.cfg \
CLOUD_IXL_OC_CH1_IP=127.0.0.1 CLOUD_IXL_OC_CH1_PORT=8888 \
CLOUD_IXL_OC_CH2_IP=127.0.0.1 CLOUD_IXL_OC_CH2_PORT=8889 \
./build/bin/exe/examples/cloud_ixl
```

Además de la secuencia PDI, un OC funcional debe proporcionar estado inicial de
aspecto y luminosidad. Para las órdenes debe devolver el estado de aspecto
`0x0003`, el estado de luminosidad `0x0004` o un error de ejecución `0x0007`,
según corresponda.

## Interfaz de operación

Después del establecimiento PDI aparece:

```text
Command [r=request, l=release, a=aspect, b=brightness, q=quit]:
```

### Rutas simuladas

El estado inicial tiene todos los circuitos de vía libres, `P_01=LEFT`, las dos
rutas libres y `LS_01=PARADA`.

| Ruta | Circuitos requeridos | Posición requerida de `P_01` |
|---|---|---|
| `1`, `A` o `B`: `RUTA_AB` | `CV_01`, `CV_02` | `LEFT` |
| `2` o `C`: `RUTA_AC` | `CV_01`, `CV_03` | `RIGHT` |

Comprobaciones manuales previstas con un OC funcional:

| Entrada | Resultado esperado del Cloud IXL |
|---|---|
| `r`, después `1` | `Decision: GO`; envía `VIA_LIBRE`; el feedback debe coincidir. |
| `r`, después `1` de nuevo | La ruta ya está reservada; `Decision: STOP`; envía `PARADA`. |
| `l`, después `1` | Envía `PARADA` y libera la ruta solo si el feedback coincide. |
| `r`, después `2` | `P_01` está en posición incompatible; `Decision: STOP`. |

Limitación conocida: `request_route_decision()` marca la ruta como `RESERVED`
antes de recibir la confirmación del OC. Un mismatch o rechazo no revierte
automáticamente esa reserva, y el array interno de señales todavía no se
actualiza con el feedback. Este comportamiento es propio del PoC actual y no
debe interpretarse como lógica de enclavamiento segura.

### Orden directa de aspecto

La opción `a` evita la selección de ruta y está destinada únicamente a ensayos
de integración. El número introducido es el valor de la enumeración interna del
Cloud IXL; no siempre coincide con el byte de aspecto enviado por SCI-LS.

| Entrada | Aspecto | Byte básico | Byte nacional | Dark | Mapeo esperado en el OC del PoC |
|---:|---|---:|---:|---:|---|
| `0` | `VIA_LIBRE` | `0x01` | `0xFE` | `0x01` | verde |
| `1` | `PARADA` | `0x02` | `0xFE` | `0x01` | rojo |
| `2` | `ANUNCIO_PARADA` | `0x03` | `0xFE` | `0x01` | amarillo |
| `4` | `ANUNCIO_PRECAUCION` | `0x05` | `0xFE` | `0x01` | verde + amarillo |
| `5` | `REBASE` | `0xFE` | `0x01` | `0x01` | rojo + blanco |
| `6` | `PARADA_SELECTIVA_N2` | `0xFE` | `0x02` | `0x01` | rojo + azul |
| `7` | `REBASE_AUTORIZADO` | `0xFE` | `0x03` | `0x01` | rojo + blanco destellante |
| `9` | `APAGADA` | `0x02` | `0xFE` | `0x0F` | todos los focos apagados |

El Cloud IXL construye y compara el payload; la actuación física corresponde al
OC. El mapeo de focos debe comprobarse en la implementación y el hardware reales.

`VIA_LIBRE_CONDICIONAL` y `PARADA_SELECTIVA_N1` aparecen en el contrato de
interfaz, pero permanecen deshabilitados en el Cloud IXL hasta que el OC del
laboratorio acepte respectivamente el aspecto básico y el aspecto nacional
`0x04`. No utilice las entradas `3` u `8` sin actualizar y volver a validar ambos
extremos.

### Luminosidad

La opción `b` envía una orden de luminosidad independiente de la ruta:

| Entrada | Luminosidad | Payload SCI-LS |
|---|---|---:|
| `1` o `d` | día | `0x01` |
| `2` o `n` | noche | `0x02` |

Después de establecer PDI, el Cloud IXL envía además `DAY` automáticamente. La
orden manual confirma que el telegrama se ha construido y entregado a RaSTA,
pero no espera de forma síncrona un `Luminosity Status`; si el OC lo envía, el
Cloud IXL lo valida y lo registra.

## Ejecución en la red física de laboratorio

El perfil versionado actualmente representa esta topología:

| Camino | Cloud IXL local | OC destino |
|---|---|---|
| Canal 1 | `192.168.0.151:9998` | `192.168.0.152:8888` |
| Canal 2 | `192.168.0.151:9999` | `192.168.0.152:8889` |

Antes de ejecutar, compruebe que `192.168.0.151` está asignada al host Cloud IXL
y que existe una ruta hacia el OC:

```bash
ip -4 address show
ip route get 192.168.0.152
```

Si la topología real es distinta, prepare un fichero RaSTA con los endpoints
locales correctos y selecciónelo con `CLOUD_IXL_RASTA_CONFIG`. Cambie también
las cuatro variables de destino para que coincidan con los canales reales del
OC.

Con la topología versionada:

```bash
CLOUD_IXL_RASTA_CONFIG=config/rasta_client1_lab.cfg \
CLOUD_IXL_OC_CH1_IP=192.168.0.152 CLOUD_IXL_OC_CH1_PORT=8888 \
CLOUD_IXL_OC_CH2_IP=192.168.0.152 CLOUD_IXL_OC_CH2_PORT=8889 \
./build/bin/exe/examples/cloud_ixl
```

Antes de energizar una señal física:

1. verifique las IP, puertos, RaSTA ID, checksum y versión en ambos extremos;
2. mantenga las salidas aisladas o desenergizadas según el procedimiento del
   laboratorio;
3. pruebe primero RaSTA y el establecimiento PDI;
4. valide las órdenes con la señal bajo supervisión;
5. registre comandos, feedback, errores, tiempos y condiciones de red.

La preparación GPIO, el cableado, la alimentación y los procedimientos de
seguridad pertenecen al OC y al laboratorio; no deben sustituirse por
suposiciones de este README.

## Degradación de red con `tc/netem`

El script `scripts/network/netem.sh` permite aplicar degradaciones únicamente al
tráfico IPv4/UDP de salida del Cloud IXL hacia los puertos del OC:

```text
Cloud IXL -> Object Controller
```

No degrada el sentido OC -> Cloud IXL. Los escenarios disponibles son:

- latencia;
- pérdida;
- duplicación;
- reordenamiento;
- corte persistente o temporizado;
- pérdida aislada del canal 1 o del canal 2.

Ejemplo con los destinos predeterminados:

```bash
sudo ./scripts/network/netem.sh status
sudo ./scripts/network/netem.sh delay 500
sudo ./scripts/network/netem.sh clear
```

Para loopback:

```bash
sudo env OC_IP=127.0.0.1 NETEM_IFACE=lo \
  ./scripts/network/netem.sh loss 5
```

Ejecute siempre `clear` al finalizar y use los mismos destinos que haya pasado
al Cloud IXL. La interfaz, el alcance, la protección de qdiscs preexistentes y
todos los escenarios se explican en
[`scripts/network/README.md`](scripts/network/README.md).

Estas pruebas son inyección de fallos sobre un prototipo. Los porcentajes y
retardos no son límites ferroviarios ni requisitos EULYNX o EN 50159.

## Pruebas y verificación

Una configuración limpia de CMake compila `rasta`, `sci_ls_icd` y `cloud_ixl`
con `-Wall -Wextra -Wpedantic -Werror`, pero actualmente no registra pruebas en
CTest:

```bash
cmake -S . -B build
cmake --build build --target cloud_ixl -j2
ctest --test-dir build -N
```

El último comando debe mostrar `Total Tests: 0` mientras no se integren las
pruebas con CMake.

`tests/rasta/` y `tests/sci/` contienen pruebas CUnit heredadas. Su definición
de construcción está en `extra/legacy-build/build.gradle`; es un flujo histórico
y no se considera el procedimiento reproducible principal del TFM. No debe
interpretarse una compilación CMake correcta como prueba funcional o de
conformidad.

Consulte [`tests/README.md`](tests/README.md) para el estado actual y
[`extra/docs/rasta-study/md_doc/cunit.md`](extra/docs/rasta-study/md_doc/cunit.md)
para la documentación histórica.

## Documentación

- [`docs/README.md`](docs/README.md): índice y alcance documental.
- [`docs/architecture/cloud_ixl_main_flow.html`](docs/architecture/cloud_ixl_main_flow.html): flujo principal del Cloud IXL.
- [Contrato de interfaz SCI-LS del proyecto](<docs/icd/contrato_interfaz_pedro_pablo_sci_ls.pdf>).
- [`docs/reference/eulynx/`](docs/reference/eulynx/): documentos EULYNX de referencia.
- [`extra/README.md`](extra/README.md): material opcional y heredado.

El ICD PDF es el contrato de integración del proyecto. No sustituye una
especificación normativa ni demuestra compatibilidad general con cualquier OC
SCI-LS.

## Material heredado

Gradle, Docker, Doxygen, Java, SCI-P y los ejemplos RaSTA independientes se
conservan bajo `extra/` para estudio, diagnóstico o compatibilidad. No forman
parte de la compilación predeterminada ni del camino funcional Cloud IXL -> OC.

Las guías históricas están en `extra/docs/rasta-study/`. Algunas describen la
estructura y las herramientas del proyecto RaSTA original y pueden requerir
adaptación antes de utilizarlas con el repositorio actual.

## Licencia

El código se distribuye bajo la licencia MIT incluida en [`LICENSE`](LICENSE).
La licencia y el historial conservan la procedencia del proyecto RaSTA original;
las extensiones Cloud IXL y SCI-LS de este repositorio siguen siendo trabajo de
prototipo académico.
