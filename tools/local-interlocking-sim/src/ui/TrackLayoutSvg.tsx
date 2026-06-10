import type {
  InfrastructureState,
  SignalAspect,
  TrackCircuit,
  TrackCircuitId,
} from '../domain/types'

interface TrackLayoutSvgProps {
  readonly layout: InfrastructureState
}

export function TrackLayoutSvg({ layout }: TrackLayoutSvgProps) {
  const signal = layout.signals[0]
  const point = layout.points[0]
  const trainsByCircuit = new Map<string, string[]>()

  for (const train of layout.trains) {
    const trains = trainsByCircuit.get(train.currentCircuitId) ?? []

    trainsByCircuit.set(train.currentCircuitId, [...trains, train.id])
  }

  function trackCircuitById(trackCircuitId: TrackCircuitId): TrackCircuit {
    const trackCircuit = layout.trackCircuits.find(({ id }) => id === trackCircuitId)

    if (trackCircuit === undefined) {
      throw new Error(`Missing track circuit ${trackCircuitId}`)
    }

    return trackCircuit
  }

  return (
    <svg
      role="img"
      aria-labelledby="track-layout-title track-layout-description"
      viewBox="0 0 820 320"
    >
      <title id="track-layout-title">MVP railway layout</title>
      <desc id="track-layout-description">
        Signal LS_01, point P1, and track circuits CV_ENTRY, CV_1, CV_2.
      </desc>

      <rect width="820" height="320" fill="#f9fbfb" />

      <CircuitLine
        circuit={trackCircuitById('CV_ENTRY')}
        x1={110}
        y1={160}
        x2={320}
        y2={160}
      />
      <CircuitLine
        circuit={trackCircuitById('CV_1')}
        x1={320}
        y1={160}
        x2={690}
        y2={82}
      />
      <CircuitLine
        circuit={trackCircuitById('CV_2')}
        x1={320}
        y1={160}
        x2={690}
        y2={238}
      />

      <circle
        cx="82"
        cy="128"
        r="13"
        fill={signalAspectFill(signal.aspect)}
        stroke="#263238"
        strokeWidth="3"
      />
      <line x1="82" y1="141" x2="82" y2="192" stroke="#263238" strokeWidth="6" />

      <line
        x1="320"
        y1="160"
        x2={point.position === 'NORMAL' ? 406 : 406}
        y2={point.position === 'NORMAL' ? 142 : 178}
        stroke="#f59e0b"
        strokeWidth="7"
        strokeLinecap="round"
      />
      <circle cx="320" cy="160" r="13" fill="#f0b429" stroke="#263238" strokeWidth="3" />

      <text x="56" y="108" className="svg-label svg-label-strong">
        {signal.id}
      </text>
      <text x="284" y="136" className="svg-label svg-label-strong">
        {point.id} {point.position}
      </text>
      {point.lockedByRouteId === undefined ? null : (
        <text x="284" y="188" className="svg-label svg-label-small">
          locked {point.lockedByRouteId}
        </text>
      )}

      <text x="160" y="193" className="svg-label">
        CV_ENTRY
      </text>
      <text x="500" y="64" className="svg-label">
        CV_1
      </text>
      <text x="500" y="264" className="svg-label">
        CV_2
      </text>

      <text x="706" y="88" className="svg-label">
        VIA_1
      </text>
      <text x="706" y="244" className="svg-label">
        VIA_2
      </text>

      <TrainMarkers circuitId="CV_ENTRY" x={212} y={128} trainsByCircuit={trainsByCircuit} />
      <TrainMarkers circuitId="CV_1" x={510} y={92} trainsByCircuit={trainsByCircuit} />
      <TrainMarkers circuitId="CV_2" x={510} y={228} trainsByCircuit={trainsByCircuit} />
    </svg>
  )
}

interface CircuitLineProps {
  readonly circuit: TrackCircuit
  readonly x1: number
  readonly y1: number
  readonly x2: number
  readonly y2: number
}

function CircuitLine({ circuit, x1, y1, x2, y2 }: CircuitLineProps) {
  const stateClassName =
    circuit.state === 'OCCUPIED' ? 'track-line-occupied' : 'track-line-clear'

  return (
    <g>
      {circuit.reservedByRouteId === undefined ? null : (
        <line
          className="track-line-reserved"
          x1={x1}
          y1={y1}
          x2={x2}
          y2={y2}
        />
      )}
      <line
        className={`track-line ${stateClassName}`}
        x1={x1}
        y1={y1}
        x2={x2}
        y2={y2}
      />
      {circuit.reservedByRouteId === undefined ? null : (
        <text
          x={(x1 + x2) / 2 - 36}
          y={(y1 + y2) / 2 - 16}
          className="svg-label svg-label-small svg-label-reserved"
        >
          {circuit.reservedByRouteId}
        </text>
      )}
    </g>
  )
}

interface TrainMarkersProps {
  readonly circuitId: TrackCircuitId
  readonly x: number
  readonly y: number
  readonly trainsByCircuit: ReadonlyMap<string, readonly string[]>
}

function TrainMarkers({ circuitId, x, y, trainsByCircuit }: TrainMarkersProps) {
  const trainIds = trainsByCircuit.get(circuitId) ?? []

  return (
    <g>
      {trainIds.map((trainId, index) => (
        <g key={trainId} transform={`translate(${x + index * 34} ${y})`}>
          <circle r="16" className="train-marker" />
          <text y="5" textAnchor="middle" className="train-marker-label">
            {trainId}
          </text>
        </g>
      ))}
    </g>
  )
}

function signalAspectFill(aspect: SignalAspect): string {
  if (aspect === 'PROCEED_MAIN') {
    return '#1a7f37'
  }

  if (aspect === 'PROCEED_DIVERGING') {
    return '#b7791f'
  }

  return '#b42318'
}
