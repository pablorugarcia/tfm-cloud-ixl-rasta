# Pruebas de degradación de red

Este directorio controla condiciones de red de laboratorio para el tráfico
RaSTA/UDP que sale del Cloud IXL hacia el Object Controller (OC). No arranca
ningún programa ni modifica SCI-LS, PDI o RaSTA.

## Configuración comprobada

El estado actual del código usa estos valores por defecto:

| Camino | Cloud IXL local | OC destino |
|---|---|---|
| canal 1 | `192.168.0.151:9998` | `192.168.0.152:8888/udp` |
| canal 2 | `192.168.0.151:9999` | `192.168.0.152:8889/udp` |

El Cloud IXL tiene RaSTA ID `0x62`; el OC, `0x61`. El OC escucha en
`0.0.0.0:8888/8889`, pero `0.0.0.0` es una dirección de escucha y **no** debe
usarse como `OC_IP`.

Los destinos reales del Cloud IXL se pueden cambiar con
`CLOUD_IXL_OC_CH1_IP/PORT` y `CLOUD_IXL_OC_CH2_IP/PORT`. Si se cambian al
ejecutar el programa, deben pasarse los mismos valores al script mediante
`OC_CH1_IP/PORT` y `OC_CH2_IP/PORT`.

Al inspeccionar este WSL, la ruta a `192.168.0.152` usaba `eth0`. En otra
máquina o sesión puede cambiar; el script vuelve a detectarla en cada uso.
Además, la IP local `192.168.0.151` del fichero de laboratorio no está asignada
actualmente a este WSL. Esa configuración debe coincidir con la topología real
antes de arrancar el Cloud IXL.

## Requisitos e interfaz

- Linux con Bash, `awk`, `ip` y `tc` de `iproute2`.
- Privilegios de administrador para aplicar o limpiar una qdisc.
- Destinos IPv4; esta primera versión no configura IPv6 ni ingress.

Para comprobar manualmente la ruta:

```bash
ip route get 192.168.0.152
```

Sin `NETEM_IFACE`, el script extrae la interfaz de ese resultado. Se puede
indicar explícitamente, pero el script comprueba que la ruta al OC realmente
pasa por ella:

```bash
sudo env OC_IP=192.168.0.152 NETEM_IFACE=eth0 \
  ./scripts/network/netem.sh status
```

Las variables deben pasarse después de `sudo` con `sudo env`, porque `sudo`
puede eliminar variables del entorno.

## Uso

Estado nominal y limpieza:

```bash
sudo ./scripts/network/netem.sh status
sudo ./scripts/network/netem.sh clear
```

Latencia:

```bash
sudo ./scripts/network/netem.sh delay 100
sudo ./scripts/network/netem.sh delay 500
sudo ./scripts/network/netem.sh delay 1000
sudo ./scripts/network/netem.sh delay 3000
```

Pérdida:

```bash
sudo ./scripts/network/netem.sh loss 1
sudo ./scripts/network/netem.sh loss 5
sudo ./scripts/network/netem.sh loss 10
sudo ./scripts/network/netem.sh loss 30
```

Duplicación:

```bash
sudo ./scripts/network/netem.sh duplicate 1
sudo ./scripts/network/netem.sh duplicate 5
sudo ./scripts/network/netem.sh duplicate 10
```

Reordenamiento:

```bash
sudo ./scripts/network/netem.sh reorder 5
sudo ./scripts/network/netem.sh reorder 10
sudo ./scripts/network/netem.sh reorder 25
```

`netem reorder` necesita una cola con retardo: sin ella los paquetes no pueden
adelantar a otros paquetes retenidos. El script añade `100 ms` por defecto. El
reordenamiento observable también requiere que llegue otro datagrama antes de
que venza ese retardo. Para tráfico más espaciado se puede aumentarlo, sabiendo
que esa latencia también forma parte del ensayo:

```bash
sudo env REORDER_DELAY_MS=3000 ./scripts/network/netem.sh reorder 10
```

Corte completo hasta ejecutar `clear`, o corte temporizado con limpieza al
terminar:

```bash
sudo ./scripts/network/netem.sh loss 100  # equivalente al corte persistente
sudo ./scripts/network/netem.sh outage
sudo ./scripts/network/netem.sh outage 5
sudo ./scripts/network/netem.sh outage 10
```

Pérdida de un único camino RaSTA:

```bash
sudo ./scripts/network/netem.sh channel1-loss  # solo dport 8888/udp
sudo ./scripts/network/netem.sh channel2-loss  # solo dport 8889/udp
```

Cada comando sustituye una degradación anterior creada por este mismo script
en la interfaz seleccionada. Para una prueba de recuperación:

```bash
sudo ./scripts/network/netem.sh delay 500
# Ejecutar la interacción Cloud IXL/OC y guardar sus logs.
sudo ./scripts/network/netem.sh clear
sudo ./scripts/network/netem.sh status
# Repetir la interacción y comprobar la recuperación RaSTA/PDI.
```

## Destinos distintos por canal

`OC_IP` sirve cuando ambos canales comparten IP. Si cada canal usa un destino
distinto:

```bash
sudo env \
  OC_CH1_IP=10.0.0.100 OC_CH1_PORT=8888 \
  OC_CH2_IP=10.0.0.101 OC_CH2_PORT=8889 \
  ./scripts/network/netem.sh loss 5
```

Los dos destinos deben salir por la misma interfaz para una degradación global.
Si las rutas usan interfaces diferentes, el script se detiene en lugar de
presentar un resultado parcial como si afectara a ambos canales. Los comandos
`channel1-loss` y `channel2-loss` sí resuelven únicamente la ruta de su canal.

Para una integración en la misma máquina, use el destino y la interfaz de
loopback de forma explícita. No basta con cambiar el script: el Cloud IXL debe
usar también su configuración local y los destinos loopback:

```bash
CLOUD_IXL_RASTA_CONFIG=config/rasta_client1_local.cfg \
CLOUD_IXL_OC_CH1_IP=127.0.0.1 CLOUD_IXL_OC_CH1_PORT=8888 \
CLOUD_IXL_OC_CH2_IP=127.0.0.1 CLOUD_IXL_OC_CH2_PORT=8889 \
./build/bin/exe/examples/cloud_ixl
```

En otra terminal:

```bash
sudo env OC_IP=127.0.0.1 NETEM_IFACE=lo \
  ./scripts/network/netem.sh delay 100
```

## Evidencia y limpieza

Después de aplicar un escenario, el script muestra la interfaz, destinos,
escenario, valor, qdisc y filtros. También pueden guardarse directamente:

```bash
tc -s qdisc show dev eth0
tc filter show dev eth0 parent c1d1:
```

`clear` busca únicamente la qdisc raíz `prio c1d1:` que identifica a este
script; no borra qdiscs raíz desconocidas. Es idempotente y puede ejecutarse
después de cualquier escenario:

```bash
sudo ./scripts/network/netem.sh clear
```

El corte temporizado instala un `trap` para limpiar ante `Ctrl-C` o `SIGTERM`.
Un `SIGKILL`, un fallo del proceso o un corte del sistema no ejecutan traps; en
ese caso se debe ejecutar `clear` al volver.

## Dirección y limitaciones

La qdisc está en **egress del host Cloud IXL**. Los filtros seleccionan IPv4,
UDP, IP de destino y puerto `8888/8889`, por lo que los escenarios afectan:

```text
Cloud IXL -> OC
```

No afectan al sentido `OC -> Cloud IXL`. Esto también se mantiene si ambos
procesos usan `lo`: las respuestas del OC tienen como destino `9998/9999` y no
coinciden con los filtros. Una prueba bidireccional requeriría actuar también
en el host del OC o añadir control ingress; esta versión no añade IFB,
namespaces, bridges ni otra infraestructura.

La qdisc `prio` sustituye temporalmente la qdisc raíz predeterminada de la
interfaz, pero solo los dos flujos filtrados pasan por `netem`. El script se
niega a sustituir una qdisc raíz personalizada que no reconoce; si la raíz es
`mq`, comprueba además que sus hojas y filtros sean los predeterminados. `clear`
elimina la qdisc propia y el kernel restaura la predeterminada.

En los escenarios globales los dos canales pasan por una única cola `netem`.
Esto mantiene pequeño el montaje, pero en `reorder` el porcentaje se aplica al
agregado: una réplica del canal 1 puede adelantar a una del canal 2. Para afirmar
reordenamiento efectivo dentro de cada canal hace falta observar los datagramas
de ese canal (por ejemplo mediante una captura); la presencia de la qdisc solo
demuestra que la condición fue configurada.

Los filtros `u32` presuponen datagramas IPv4/UDP sin opciones IPv4 ni
fragmentación. Es coherente con los datagramas pequeños observados en este PoC,
pero debe revisarse si el transporte llega a fragmentar paquetes.

Estas son pruebas de un prototipo de laboratorio. Los valores usados no son
requisitos EULYNX ni límites ferroviarios, y los resultados no demuestran
conformidad RaSTA, EN 50159, EULYNX ni SIL4.
