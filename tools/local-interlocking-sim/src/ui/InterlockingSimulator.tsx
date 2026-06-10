import { useState } from 'react'
import { requestRoute } from '../domain/requestRoute'
import { injectTrackCircuitOccupancy } from '../domain/faultInjection'
import {
  moveTrainToCircuit,
  placeTrain,
  releaseRoute,
} from '../domain/trainOperations'
import type {
  EventLogEntry,
  FaultInjectionRejectionReason,
  InfrastructureState,
  RouteId,
  RouteReleaseRejectionReason,
  RouteRequestRejectionReason,
  TrackCircuitId,
  TrainOperationRejectionReason,
} from '../domain/types'
import { TrackLayoutSvg } from './TrackLayoutSvg'

interface InterlockingSimulatorProps {
  readonly layout: InfrastructureState
}

type OperationReason =
  | RouteRequestRejectionReason
  | TrainOperationRejectionReason
  | RouteReleaseRejectionReason
  | FaultInjectionRejectionReason

export function InterlockingSimulator({ layout }: InterlockingSimulatorProps) {
  const [state, setState] = useState<InfrastructureState>(layout)
  const [lastStatus, setLastStatus] = useState('Ready')

  function handleRequestRoute(routeId: RouteId) {
    const result = requestRoute(state, routeId)

    setState(result.state)
    setLastStatus(
      result.accepted
        ? `${routeId} locked, ${result.commandedAspect}`
        : `${routeId} rejected: ${formatReason(result.reason)}`,
    )
  }

  function handleReleaseRoute(routeId: RouteId) {
    const result = releaseRoute(state, routeId)

    setState(result.state)
    setLastStatus(
      result.accepted
        ? `${routeId} released`
        : `${routeId} release rejected: ${formatReason(result.reason)}`,
    )
  }

  function handlePlaceTrainOnEntry() {
    const trainId = `T${state.trains.length + 1}`
    const result = placeTrain(state, trainId, 'CV_ENTRY')

    setState(result.state)
    setLastStatus(
      result.accepted
        ? `${trainId} placed on CV_ENTRY`
        : `Train placement rejected: ${formatReason(result.reason)}`,
    )
  }

  function handleMoveTrain(nextCircuitId: TrackCircuitId) {
    const trainId = state.trains.at(-1)?.id ?? 'T1'
    const result = moveTrainToCircuit(state, trainId, nextCircuitId)

    setState(result.state)
    setLastStatus(
      result.accepted
        ? `${trainId} moved to ${nextCircuitId}`
        : `Train movement rejected: ${formatReason(result.reason)}`,
    )
  }

  function handleReset() {
    setState(layout)
    setLastStatus('Simulation reset')
  }

  function handleToggleCircuitOccupancy(circuitId: TrackCircuitId) {
    const trackCircuit = state.trackCircuits.find(({ id }) => id === circuitId)
    const nextOccupancy =
      trackCircuit?.state === 'OCCUPIED' ? 'CLEAR' : 'OCCUPIED'
    const result = injectTrackCircuitOccupancy(state, circuitId, nextOccupancy)

    setState(result.state)
    setLastStatus(
      result.accepted
        ? `${circuitId} fault injection set ${nextOccupancy}`
        : `Fault injection rejected: ${formatReason(result.reason)}`,
    )
  }

  return (
    <main className="app-shell">
      <header className="app-header">
        <h1>Local Interlocking Simulator</h1>
        <p>{lastStatus}</p>
      </header>

      <section className="simulator" aria-label="Interlocking simulator">
        <div className="workspace">
          <div className="track-layout">
            <TrackLayoutSvg layout={state} />
          </div>

          <section className="control-panel" aria-label="Simulator controls">
            <div className="control-group">
              <h2>Routes</h2>
              <button type="button" onClick={() => handleRequestRoute('R_MAIN')}>
                Request R_MAIN
              </button>
              <button type="button" onClick={() => handleRequestRoute('R_DIVERGING')}>
                Request R_DIVERGING
              </button>
              <button type="button" onClick={() => handleReleaseRoute('R_MAIN')}>
                Release R_MAIN
              </button>
              <button type="button" onClick={() => handleReleaseRoute('R_DIVERGING')}>
                Release R_DIVERGING
              </button>
            </div>

            <div className="control-group">
              <h2>Train</h2>
              <button type="button" onClick={handlePlaceTrainOnEntry}>
                Place train on CV_ENTRY
              </button>
              <button type="button" onClick={() => handleMoveTrain('CV_1')}>
                Move train to CV_1
              </button>
              <button type="button" onClick={() => handleMoveTrain('CV_2')}>
                Move train to CV_2
              </button>
            </div>

            <div className="control-group">
              <h2>Fault Injection</h2>
              <button type="button" onClick={() => handleToggleCircuitOccupancy('CV_1')}>
                Toggle CV_1 occupied/free
              </button>
              <button type="button" onClick={() => handleToggleCircuitOccupancy('CV_2')}>
                Toggle CV_2 occupied/free
              </button>
            </div>

            <div className="control-group control-group-reset">
              <button type="button" onClick={handleReset}>
                Reset simulation
              </button>
            </div>
          </section>
        </div>

        <aside className="side-panel" aria-label="Simulator status">
          <section className="panel-section">
            <h2>Signal</h2>
            <p className="status-row">
              <span>{state.signals[0].id}</span>
              <strong className={aspectClassName(state.signals[0].aspect)}>
                {state.signals[0].aspect}
              </strong>
            </p>
          </section>

          <section className="panel-section">
            <h2>Route States</h2>
            {state.routes.map((route) => (
              <p className="status-row" key={route.id}>
                <span>{route.id}</span>
                <strong>{route.state}</strong>
              </p>
            ))}
          </section>

          <section className="panel-section">
            <h2>Track Circuits</h2>
            {state.trackCircuits.map((trackCircuit) => (
              <p className="status-row" key={trackCircuit.id}>
                <span>{trackCircuit.id}</span>
                <strong>
                  {trackCircuit.state}
                  {trackCircuit.reservedByRouteId === undefined
                    ? ''
                    : ` / ${trackCircuit.reservedByRouteId}`}
                </strong>
              </p>
            ))}
          </section>

          <section className="panel-section">
            <h2>Point</h2>
            {state.points.map((point) => (
              <p className="status-row" key={point.id}>
                <span>{point.id}</span>
                <strong>
                  {point.position}
                  {point.lockedByRouteId === undefined
                    ? ''
                    : ` / ${point.lockedByRouteId}`}
                </strong>
              </p>
            ))}
          </section>

          <section className="panel-section">
            <h2>Trains</h2>
            {state.trains.length === 0 ? (
              <p className="empty-state">No trains placed</p>
            ) : (
              state.trains.map((train) => (
                <p className="status-row" key={train.id}>
                  <span>{train.id}</span>
                  <strong>{train.currentCircuitId}</strong>
                </p>
              ))
            )}
          </section>

          <section className="panel-section event-log-panel">
            <h2>Event Log</h2>
            {state.eventLog.length === 0 ? (
              <p className="empty-state">No events yet</p>
            ) : (
              <ol className="event-log">
                {[...state.eventLog].reverse().map((entry) => (
                  <li key={entry.id}>{formatEvent(entry)}</li>
                ))}
              </ol>
            )}
          </section>
        </aside>
      </section>
    </main>
  )
}

function aspectClassName(aspect: string): string {
  if (aspect === 'STOP') {
    return 'aspect-stop'
  }

  if (aspect === 'PROCEED_MAIN') {
    return 'aspect-proceed-main'
  }

  return 'aspect-proceed-diverging'
}

function formatEvent(entry: EventLogEntry): string {
  const target =
    entry.routeId ??
    entry.trainId ??
    entry.circuitId ??
    [entry.fromCircuitId, entry.toCircuitId].filter(Boolean).join(' -> ')

  const reason =
    entry.reason === undefined ? '' : `: ${formatReason(entry.reason)}`

  return `#${entry.sequence} ${entry.type}${target ? ` ${target}` : ''}${reason}`
}

function formatReason(reason: OperationReason): string {
  const details = Object.entries(reason)
    .filter(([key, value]) => key !== 'code' && value !== undefined)
    .map(([key, value]) => `${key}=${value}`)

  return details.length === 0
    ? reason.code
    : `${reason.code} (${details.join(', ')})`
}
