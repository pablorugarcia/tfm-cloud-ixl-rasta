import { DEFAULT_SIGNAL_ASPECT } from './types'
import type {
  EventLogEntry,
  EventLogEntryType,
  InfrastructureState,
  Point,
  RequiredPointPosition,
  Route,
  RouteId,
  RouteRequestRejected,
  RouteRequestRejectionReason,
  RouteRequestResult,
  Signal,
  TrackCircuit,
} from './types'

export function requestRoute(
  state: InfrastructureState,
  routeId: string,
): RouteRequestResult {
  const route = state.routes.find((candidate) => candidate.id === routeId)

  if (route === undefined) {
    return rejectUnknownRoute(state, routeId)
  }

  const occupiedTrackCircuit = findRequiredTrackCircuit(route, state, (trackCircuit) => {
    return trackCircuit.state === 'OCCUPIED'
  })

  if (occupiedTrackCircuit !== undefined) {
    return rejectKnownRoute(state, route, {
      code: 'TRACK_CIRCUIT_OCCUPIED',
      trackCircuitId: occupiedTrackCircuit.id,
    })
  }

  const reservedTrackCircuit = findRequiredTrackCircuit(route, state, (trackCircuit) => {
    return (
      trackCircuit.state === 'RESERVED' &&
      trackCircuit.reservedByRouteId !== route.id
    )
  })

  if (reservedTrackCircuit !== undefined) {
    return rejectKnownRoute(state, route, {
      code: 'TRACK_CIRCUIT_RESERVED',
      trackCircuitId: reservedTrackCircuit.id,
      reservedByRouteId: reservedTrackCircuit.reservedByRouteId,
    })
  }

  const conflictingRoute = state.routes.find((candidate) => {
    return route.conflictsWith.includes(candidate.id) && candidate.state === 'LOCKED'
  })

  if (conflictingRoute !== undefined) {
    return rejectKnownRoute(state, route, {
      code: 'CONFLICTING_ROUTE_LOCKED',
      conflictingRouteId: conflictingRoute.id,
    })
  }

  const lockedPoint = state.points.find((point) => {
    return (
      route.requiredPointPositions.some((requiredPoint) => {
        return requiredPoint.pointId === point.id
      }) &&
      point.lockedByRouteId !== undefined &&
      point.lockedByRouteId !== route.id
    )
  })

  if (lockedPoint !== undefined) {
    return rejectKnownRoute(state, route, {
      code: 'POINT_LOCKED',
      pointId: lockedPoint.id,
      lockedByRouteId: lockedPoint.lockedByRouteId,
    })
  }

  const acceptedState = appendEvent(
    {
      ...state,
      routes: lockRequestedRoute(state.routes, route.id),
      trackCircuits: reserveRequiredTrackCircuits(state.trackCircuits, route),
      points: setAndLockRequiredPoints(state.points, route),
      signals: setEntrySignalAspect(state.signals, route, route.commandedAspect),
    },
    'ROUTE_LOCKED',
    route.id,
  )

  return {
    accepted: true,
    routeId: route.id,
    commandedAspect: route.commandedAspect,
    state: acceptedState,
  }
}

function rejectUnknownRoute(
  state: InfrastructureState,
  routeId: string,
): RouteRequestRejected {
  const reason: RouteRequestRejectionReason = { code: 'ROUTE_NOT_FOUND' }

  return {
    accepted: false,
    routeId,
    reason,
    state: appendEvent(state, 'ROUTE_REJECTED', routeId, reason),
  }
}

function rejectKnownRoute(
  state: InfrastructureState,
  route: Route,
  reason: RouteRequestRejectionReason,
): RouteRequestRejected {
  const rejectedState = appendEvent(
    {
      ...state,
      routes: rejectRequestedRoute(state.routes, route.id),
      signals: setEntrySignalAspect(state.signals, route, DEFAULT_SIGNAL_ASPECT),
    },
    'ROUTE_REJECTED',
    route.id,
    reason,
  )

  return {
    accepted: false,
    routeId: route.id,
    reason,
    state: rejectedState,
  }
}

function findRequiredTrackCircuit(
  route: Route,
  state: InfrastructureState,
  predicate: (trackCircuit: TrackCircuit) => boolean,
): TrackCircuit | undefined {
  for (const trackCircuitId of route.requiredClearTrackCircuits) {
    const trackCircuit = state.trackCircuits.find(({ id }) => id === trackCircuitId)

    if (trackCircuit !== undefined && predicate(trackCircuit)) {
      return trackCircuit
    }
  }

  return undefined
}

function lockRequestedRoute(
  routes: readonly Route[],
  routeId: RouteId,
): readonly Route[] {
  return routes.map((route) => {
    if (route.id !== routeId) {
      return route
    }

    return {
      ...route,
      state: 'LOCKED',
    }
  })
}

function rejectRequestedRoute(
  routes: readonly Route[],
  routeId: RouteId,
): readonly Route[] {
  return routes.map((route) => {
    if (route.id !== routeId) {
      return route
    }

    return {
      ...route,
      state: 'REJECTED',
    }
  })
}

function reserveRequiredTrackCircuits(
  trackCircuits: readonly TrackCircuit[],
  route: Route,
): readonly TrackCircuit[] {
  return trackCircuits.map((trackCircuit) => {
    if (!route.requiredClearTrackCircuits.includes(trackCircuit.id)) {
      return trackCircuit
    }

    return {
      ...trackCircuit,
      state: 'RESERVED',
      reservedByRouteId: route.id,
    }
  })
}

function setAndLockRequiredPoints(
  points: readonly Point[],
  route: Route,
): readonly Point[] {
  return points.map((point) => {
    const requiredPoint = route.requiredPointPositions.find((candidate) => {
      return candidate.pointId === point.id
    })

    if (requiredPoint === undefined) {
      return point
    }

    return setAndLockPoint(point, requiredPoint, route.id)
  })
}

function setAndLockPoint(
  point: Point,
  requiredPoint: RequiredPointPosition,
  routeId: RouteId,
): Point {
  return {
    ...point,
    position: requiredPoint.position,
    lockedByRouteId: routeId,
  }
}

function setEntrySignalAspect(
  signals: readonly Signal[],
  route: Route,
  aspect: Route['commandedAspect'],
): readonly Signal[] {
  return signals.map((signal) => {
    if (signal.id !== route.entrySignalId) {
      return signal
    }

    return {
      ...signal,
      aspect,
    }
  })
}

function appendEvent(
  state: InfrastructureState,
  type: EventLogEntryType,
  routeId: string,
  reason?: RouteRequestRejectionReason,
): InfrastructureState {
  const sequence = nextEventSequence(state.eventLog)
  const entry: EventLogEntry = {
    id: `event-${sequence}`,
    sequence,
    type,
    routeId,
    reason,
  }

  return {
    ...state,
    eventLog: [...state.eventLog, entry],
  }
}

function nextEventSequence(eventLog: readonly EventLogEntry[]): number {
  return eventLog.reduce((maxSequence, entry) => {
    return Math.max(maxSequence, entry.sequence)
  }, 0) + 1
}
