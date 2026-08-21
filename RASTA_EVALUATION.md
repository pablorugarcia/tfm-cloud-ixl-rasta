# Canales, parámetros RaSTA y jerarquía de timeouts

## Alcance

Esta nota describe decisiones del prototipo de laboratorio Cloud IXL. No es una
implementación normativa ni evidencia de conformidad EULYNX, RaSTA, EN 50159 o
SIL4.

La referencia comprobada es `Eu.Doc.92 v4.3 (1.A)`, conservada en
`docs/reference/eulynx/`. Para SCI sobre UDP indica `Tmax=1800 ms`, `Th=300 ms`,
`Nsendmax=20`, `MWA=10`, `NmaxPaket=1`, `Tseq=100 ms`,
`NdeferQueueSize=4`, safety code de ocho bytes y dos canales físicos.

## Decisión integrada para el OC actual

El perfil UDP completo no puede activarse unilateralmente contra la
configuración versionada del OC de Pedro:

- el OC usa `Th=2000 ms`; un `Tmax=1800 ms` local podría vencer antes del
  siguiente heartbeat entrante durante una conexión ociosa;
- el OC usa checksum SR `NONE`; cambiar solo Cloud IXL a `HALF` cambia el
  formato que el receptor espera y puede impedir el handshake.

Por ello se conservan `Tmax=10000 ms` y checksum `NONE`. El perfil
`config/rasta_client1_eulynx_evaluation.cfg` aplica únicamente el subconjunto
unilateral compatible. Es un perfil de evaluación, no un perfil conforme.

| Parámetro | Lab | Eu.Doc.92 UDP | Evaluación | Decisión |
|---|---:|---:|---:|---|
| `T_MAX` | 10000 ms | 1800 ms | 10000 ms | Mantener por `Th=2000 ms` en el OC. |
| `T_H` | 2000 ms | 300 ms | 300 ms | Local; mayor frecuencia es compatible. |
| `SR_CHECKSUM_LEN` | `NONE` | `HALF` | `NONE` | Requiere coordinación bilateral. |
| `SEND_MAX` | 10 | 20 | 20 | Se anuncia en el handshake; no exige igualdad. |
| `MWA` | 10 | 10 | 10 | Ya coincide. |
| `MAX_PACKET` | 3 | 1 | 1 | Agrupación local; SCI-LS envía un mensaje. |
| `T_SEQ` | 50 ms | 100 ms | 100 ms | Cola de redundancia local. |
| `N_DEFERQUEUE_SIZE` | 2 | 4 | 4 | Cola de redundancia local. |

El parser no soporta herencia entre configuraciones. Por eso el perfil de
evaluación es autosuficiente y explicita también las dos desviaciones.

## Configuración de los dos canales

Cada entrada de `RASTA_REDUNDANCY_CONNECTIONS` crea un socket UDP y lo liga a
su IP y puerto local. Los destinos se suministran por separado:

| Canal | Local | Remoto |
|---|---|---|
| 1 | primera entrada de `RASTA_REDUNDANCY_CONNECTIONS` | `CLOUD_IXL_OC_CH1_IP`, `CLOUD_IXL_OC_CH1_PORT` |
| 2 | segunda entrada de `RASTA_REDUNDANCY_CONNECTIONS` | `CLOUD_IXL_OC_CH2_IP`, `CLOUD_IXL_OC_CH2_PORT` |

El OC actual escucha `0.0.0.0:8888` y `0.0.0.0:8889`, por lo que puede recibir
por dos direcciones locales distintas sin editar su repositorio. Aun así, dos
puertos o dos IP aliases sobre la misma NIC solo aportan redundancia lógica.

Para demostrar dos canales físicos se necesitan, como mínimo:

- dos NIC físicas independientes en Cloud IXL y dos en el OC;
- direccionamiento y rutas que asocien cada par local/remoto a su NIC;
- dos enlaces y caminos de red sin un único enlace físico compartido;
- evidencia de ruta y una prueba de desconexión física de cada camino por
  separado, manteniendo operativo el otro.

El WSL inspeccionado expone únicamente `lo` y una `eth0`; no permite esa
demostración.

## Jerarquía temporal del PoC

Para las configuraciones compatibles incluidas se adopta:

```text
T_PDI = 11 s > T_RaSTA_failure_detection nominal = 10 s
```

El deadline PDI conserva ahora los nanosegundos del instante de inicio. El hilo
RaSTA comprueba `T_i` periódicamente, con hasta unos 500 ms entre sondeos en la
implementación actual; el margen no constituye una garantía de tiempo real.

La jerarquía anterior aplica a una sesión ya establecida. `T_i` se arma después
del `ConnectionResponse`; durante el handshake, el timeout global PDI sigue
limitando el establecimiento.

Los callbacks de timeout y `CLOSED` se ejecutan en hilos separados, por lo que
el orden de sus líneas de log puede variar. El Cloud IXL conserva `PDI_FAILED`
si el timeout RaSTA ya lo estableció, evitando que `CLOSED` lo sobrescriba con
`PDI_DISCONNECTED` durante una orden pendiente.

Toda campaña ejecutada con el perfil de evaluación o con el timeout PDI de
11 segundos es una campaña nueva. Los resultados históricos de
`results/network_degradation/` mantienen su configuración y significado.
