import { appendEvent } from './events'
import { DEFAULT_SIGNAL_ASPECT } from './types'
import type {
  InfrastructureState,
  Point,
  Route,
  RouteId,
  RouteReleaseResult,
  Signal,
  TrackCircuit,
  Train,
  TrainOperationResult,
} from './types'

export function placeTrain(
  state: InfrastructureState,
  trainId: string,
  circuitId: string,
): TrainOperationResult {
  const targetCircuit = findTrackCircuit(state, circuitId)

  if (targetCircuit === undefined) {
    return {
      accepted: false,
      trainId,
      reason: {
        code: 'CIRCUIT_NOT_FOUND',
        circuitId,
      },
      state,
    }
  }

  if (targetCircuit.state === 'OCCUPIED') {
    return {
      accepted: false,
      trainId,
      reason: {
        code: 'TRACK_CIRCUIT_OCCUPIED',
        trackCircuitId: circuitId,
      },
      state,
    }
  }

  const placedState = appendEvent(
    {
      ...state,
      trains: [
        ...state.trains,
        {
          id: trainId,
          currentCircuitId: circuitId,
        },
      ],
      trackCircuits: state.trackCircuits.map((trackCircuit) => {
        if (trackCircuit.id !== circuitId) {
          return trackCircuit
        }

        return {
          ...trackCircuit,
          state: 'OCCUPIED',
        }
      }),
    },
    {
      type: 'TRAIN_PLACED',
      trainId,
      circuitId,
    },
  )

  return {
    accepted: true,
    trainId,
    state: placedState,
  }
}

export function moveTrainToCircuit(
  state: InfrastructureState,
  trainId: string,
  nextCircuitId: string,
): TrainOperationResult {
  const train = findTrain(state, trainId)

  if (train === undefined) {
    return {
      accepted: false,
      trainId,
      reason: {
        code: 'TRAIN_NOT_FOUND',
        trainId,
      },
      state,
    }
  }

  const targetCircuit = findTrackCircuit(state, nextCircuitId)

  if (targetCircuit === undefined) {
    return {
      accepted: false,
      trainId,
      reason: {
        code: 'CIRCUIT_NOT_FOUND',
        circuitId: nextCircuitId,
      },
      state,
    }
  }

  const occupyingTrain = state.trains.find((candidate) => {
    return candidate.currentCircuitId === nextCircuitId
  })

  if (
    targetCircuit.state === 'OCCUPIED' &&
    (occupyingTrain === undefined || occupyingTrain.id !== trainId)
  ) {
    return {
      accepted: false,
      trainId,
      reason: {
        code: 'TRACK_CIRCUIT_OCCUPIED',
        trackCircuitId: nextCircuitId,
      },
      state,
    }
  }

  const movedState = appendEvent(
    {
      ...state,
      trains: state.trains.map((candidate) => {
        if (candidate.id !== trainId) {
          return candidate
        }

        return {
          ...candidate,
          currentCircuitId: nextCircuitId,
        }
      }),
      trackCircuits: moveTrackCircuitOccupancy(
        state.trackCircuits,
        train.currentCircuitId,
        nextCircuitId,
      ),
    },
    {
      type: 'TRAIN_MOVED',
      trainId,
      fromCircuitId: train.currentCircuitId,
      toCircuitId: nextCircuitId,
    },
  )

  return {
    accepted: true,
    trainId,
    state: movedState,
  }
}

export function releaseRoute(
  state: InfrastructureState,
  routeId: string,
): RouteReleaseResult {
  const route = state.routes.find((candidate) => candidate.id === routeId)

  if (route === undefined) {
    return {
      accepted: false,
      routeId,
      reason: {
        code: 'ROUTE_NOT_FOUND',
      },
      state,
    }
  }

  const releasedState = appendEvent(
    {
      ...state,
      routes: releaseRequestedRoute(state.routes, route.id),
      trackCircuits: clearRouteReservations(state, route.id),
      points: unlockRoutePoints(state.points, route.id),
      signals: setEntrySignalToStop(state.signals, route),
    },
    {
      type: 'ROUTE_RELEASED',
      routeId: route.id,
    },
  )

  return {
    accepted: true,
    routeId: route.id,
    state: releasedState,
  }
}

function findTrackCircuit(
  state: InfrastructureState,
  circuitId: string,
): TrackCircuit | undefined {
  return state.trackCircuits.find((trackCircuit) => {
    return trackCircuit.id === circuitId
  })
}

function findTrain(
  state: InfrastructureState,
  trainId: string,
): Train | undefined {
  return state.trains.find((train) => train.id === trainId)
}

function moveTrackCircuitOccupancy(
  trackCircuits: readonly TrackCircuit[],
  previousCircuitId: string,
  nextCircuitId: string,
): readonly TrackCircuit[] {
  return trackCircuits.map((trackCircuit) => {
    if (trackCircuit.id === previousCircuitId && trackCircuit.id !== nextCircuitId) {
      return {
        ...trackCircuit,
        state: 'CLEAR',
      }
    }

    if (trackCircuit.id === nextCircuitId) {
      return {
        ...trackCircuit,
        state: 'OCCUPIED',
      }
    }

    return trackCircuit
  })
}

function releaseRequestedRoute(
  routes: readonly Route[],
  routeId: RouteId,
): readonly Route[] {
  return routes.map((route) => {
    if (route.id !== routeId) {
      return route
    }

    return {
      ...route,
      state: 'RELEASED',
    }
  })
}

function clearRouteReservations(
  state: InfrastructureState,
  routeId: RouteId,
): readonly TrackCircuit[] {
  return state.trackCircuits.map((trackCircuit) => {
    if (trackCircuit.reservedByRouteId !== routeId) {
      return trackCircuit
    }

    if (isCircuitOccupiedByTrain(state, trackCircuit.id)) {
      return trackCircuit
    }

    return {
      id: trackCircuit.id,
      state: trackCircuit.state,
    }
  })
}

function isCircuitOccupiedByTrain(
  state: InfrastructureState,
  circuitId: string,
): boolean {
  return state.trains.some((train) => train.currentCircuitId === circuitId)
}

function unlockRoutePoints(
  points: readonly Point[],
  routeId: RouteId,
): readonly Point[] {
  return points.map((point) => {
    if (point.lockedByRouteId !== routeId) {
      return point
    }

    return {
      id: point.id,
      position: point.position,
    }
  })
}

function setEntrySignalToStop(
  signals: readonly Signal[],
  route: Route,
): readonly Signal[] {
  return signals.map((signal) => {
    if (signal.id !== route.entrySignalId) {
      return signal
    }

    return {
      ...signal,
      aspect: DEFAULT_SIGNAL_ASPECT,
    }
  })
}
