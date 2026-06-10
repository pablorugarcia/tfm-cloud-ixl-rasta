import { DEFAULT_SIGNAL_ASPECT } from './types'
import type {
  InfrastructureState,
  InfrastructureValidationIssue,
  InfrastructureValidationResult,
  Route,
  TrackCircuit,
} from './types'

export function validateInfrastructureState(
  state: InfrastructureState,
): InfrastructureValidationResult {
  const issues: InfrastructureValidationIssue[] = []
  const signalIds: ReadonlySet<string> = new Set(state.signals.map(({ id }) => id))
  const pointIds: ReadonlySet<string> = new Set(state.points.map(({ id }) => id))
  const trackCircuitIds: ReadonlySet<string> = new Set(
    state.trackCircuits.map(({ id }) => id),
  )
  const routeIds: ReadonlySet<string> = new Set(state.routes.map(({ id }) => id))

  for (const route of state.routes) {
    validateRouteReferences(route, signalIds, pointIds, trackCircuitIds, routeIds, issues)
    validateLockedRoute(route, state, issues)
    validateReleasedRouteOwnership(route, state, issues)
  }

  for (const trackCircuit of state.trackCircuits) {
    validateTrackCircuitReservation(trackCircuit, routeIds, issues)
    validateTrackCircuitOccupancy(trackCircuit, state, issues)
  }

  for (const point of state.points) {
    if (point.lockedByRouteId !== undefined && !routeIds.has(point.lockedByRouteId)) {
      issues.push({
        code: 'POINT_LOCKED_BY_UNKNOWN_ROUTE',
        severity: 'ERROR',
        message: `Point ${point.id} is locked by unknown route ${point.lockedByRouteId}.`,
        pointId: point.id,
        relatedRouteId: point.lockedByRouteId,
      })
    }
  }

  validateTrains(state, trackCircuitIds, issues)
  validateSignalAspects(state, issues)

  return {
    valid: issues.every(({ severity }) => severity !== 'ERROR'),
    issues,
  }
}

function validateRouteReferences(
  route: Route,
  signalIds: ReadonlySet<string>,
  pointIds: ReadonlySet<string>,
  trackCircuitIds: ReadonlySet<string>,
  routeIds: ReadonlySet<string>,
  issues: InfrastructureValidationIssue[],
) {
  if (!signalIds.has(route.entrySignalId)) {
    issues.push({
      code: 'ROUTE_ENTRY_SIGNAL_NOT_FOUND',
      severity: 'ERROR',
      message: `Route ${route.id} references unknown entry signal ${route.entrySignalId}.`,
      routeId: route.id,
      signalId: route.entrySignalId,
    })
  }

  for (const trackCircuitId of route.requiredClearTrackCircuits) {
    if (!trackCircuitIds.has(trackCircuitId)) {
      issues.push({
        code: 'ROUTE_REQUIRED_TRACK_CIRCUIT_NOT_FOUND',
        severity: 'ERROR',
        message: `Route ${route.id} requires unknown track circuit ${trackCircuitId}.`,
        routeId: route.id,
        trackCircuitId,
      })
    }
  }

  for (const requiredPoint of route.requiredPointPositions) {
    if (!pointIds.has(requiredPoint.pointId)) {
      issues.push({
        code: 'ROUTE_REQUIRED_POINT_NOT_FOUND',
        severity: 'ERROR',
        message: `Route ${route.id} requires unknown point ${requiredPoint.pointId}.`,
        routeId: route.id,
        pointId: requiredPoint.pointId,
      })
    }
  }

  for (const conflictingRouteId of route.conflictsWith) {
    if (!routeIds.has(conflictingRouteId)) {
      issues.push({
        code: 'ROUTE_CONFLICT_NOT_FOUND',
        severity: 'ERROR',
        message: `Route ${route.id} conflicts with unknown route ${conflictingRouteId}.`,
        routeId: route.id,
        relatedRouteId: conflictingRouteId,
      })
    }
  }
}

function validateTrackCircuitReservation(
  trackCircuit: TrackCircuit,
  routeIds: ReadonlySet<string>,
  issues: InfrastructureValidationIssue[],
) {
  if (
    trackCircuit.reservedByRouteId !== undefined &&
    !routeIds.has(trackCircuit.reservedByRouteId)
  ) {
    issues.push({
      code: 'TRACK_CIRCUIT_RESERVED_BY_UNKNOWN_ROUTE',
      severity: 'ERROR',
      message: `Track circuit ${trackCircuit.id} is reserved by unknown route ${trackCircuit.reservedByRouteId}.`,
      trackCircuitId: trackCircuit.id,
      relatedRouteId: trackCircuit.reservedByRouteId,
    })
  }
}

function validateTrains(
  state: InfrastructureState,
  trackCircuitIds: ReadonlySet<string>,
  issues: InfrastructureValidationIssue[],
) {
  const trainsByCircuit = new Map<string, string[]>()

  for (const train of state.trains) {
    if (!trackCircuitIds.has(train.currentCircuitId)) {
      issues.push({
        code: 'TRAIN_CURRENT_CIRCUIT_NOT_FOUND',
        severity: 'ERROR',
        message: `Train ${train.id} references unknown current circuit ${train.currentCircuitId}.`,
        trainId: train.id,
        trackCircuitId: train.currentCircuitId,
      })
      continue
    }

    const trainIds = trainsByCircuit.get(train.currentCircuitId) ?? []
    trainsByCircuit.set(train.currentCircuitId, [...trainIds, train.id])
  }

  for (const [trackCircuitId, trainIds] of trainsByCircuit.entries()) {
    if (trainIds.length > 1) {
      issues.push({
        code: 'MULTIPLE_TRAINS_ON_CIRCUIT',
        severity: 'ERROR',
        message: `Track circuit ${trackCircuitId} has multiple trains: ${trainIds.join(', ')}.`,
        trackCircuitId,
      })
    }
  }

  for (const train of state.trains) {
    const trackCircuit = state.trackCircuits.find(({ id }) => {
      return id === train.currentCircuitId
    })

    if (trackCircuit !== undefined && trackCircuit.state !== 'OCCUPIED') {
      issues.push({
        code: 'TRAIN_ON_NON_OCCUPIED_CIRCUIT',
        severity: 'ERROR',
        message: `Train ${train.id} is on ${train.currentCircuitId}, but the circuit is ${trackCircuit.state}.`,
        trainId: train.id,
        trackCircuitId: train.currentCircuitId,
      })
    }
  }
}

function validateTrackCircuitOccupancy(
  trackCircuit: TrackCircuit,
  state: InfrastructureState,
  issues: InfrastructureValidationIssue[],
) {
  if (trackCircuit.state !== 'OCCUPIED') {
    return
  }

  const trainCount = state.trains.filter((train) => {
    return train.currentCircuitId === trackCircuit.id
  }).length

  if (trainCount !== 1) {
    issues.push({
      code: 'OCCUPIED_CIRCUIT_WITHOUT_EXACTLY_ONE_TRAIN',
      severity: 'ERROR',
      message: `Occupied track circuit ${trackCircuit.id} has ${trainCount} trains instead of exactly one.`,
      trackCircuitId: trackCircuit.id,
    })
  }
}

function validateLockedRoute(
  route: Route,
  state: InfrastructureState,
  issues: InfrastructureValidationIssue[],
) {
  if (route.state !== 'LOCKED') {
    return
  }

  for (const requiredPoint of route.requiredPointPositions) {
    const point = state.points.find(({ id }) => id === requiredPoint.pointId)

    if (point !== undefined && point.lockedByRouteId !== route.id) {
      issues.push({
        code: 'LOCKED_ROUTE_POINT_NOT_LOCKED',
        severity: 'ERROR',
        message: `Locked route ${route.id} requires point ${requiredPoint.pointId}, but it is not locked by that route.`,
        routeId: route.id,
        pointId: requiredPoint.pointId,
        relatedRouteId: point.lockedByRouteId,
      })
    }
  }

  for (const trackCircuitId of route.requiredClearTrackCircuits) {
    const trackCircuit = state.trackCircuits.find(({ id }) => id === trackCircuitId)

    if (trackCircuit !== undefined && trackCircuit.reservedByRouteId !== route.id) {
      issues.push({
        code: 'LOCKED_ROUTE_TRACK_CIRCUIT_NOT_RESERVED',
        severity: 'ERROR',
        message: `Locked route ${route.id} requires track circuit ${trackCircuitId}, but it is not reserved by that route.`,
        routeId: route.id,
        trackCircuitId,
        relatedRouteId: trackCircuit.reservedByRouteId,
      })
    }
  }
}

function validateSignalAspects(
  state: InfrastructureState,
  issues: InfrastructureValidationIssue[],
) {
  for (const signal of state.signals) {
    if (signal.aspect === DEFAULT_SIGNAL_ASPECT) {
      continue
    }

    const hasLockedRoute = state.routes.some((route) => {
      return route.state === 'LOCKED' && route.entrySignalId === signal.id
    })

    if (!hasLockedRoute) {
      issues.push({
        code: 'SIGNAL_ASPECT_WITHOUT_LOCKED_ROUTE',
        severity: 'ERROR',
        message: `Signal ${signal.id} shows ${signal.aspect} without a locked route from that signal.`,
        signalId: signal.id,
      })
    }
  }
}

function validateReleasedRouteOwnership(
  route: Route,
  state: InfrastructureState,
  issues: InfrastructureValidationIssue[],
) {
  if (route.state !== 'RELEASED') {
    return
  }

  for (const trackCircuit of state.trackCircuits) {
    if (trackCircuit.reservedByRouteId === route.id) {
      issues.push({
        code: 'RELEASED_ROUTE_OWNS_RESERVATION_OR_LOCK',
        severity: 'ERROR',
        message: `Released route ${route.id} still reserves track circuit ${trackCircuit.id}.`,
        routeId: route.id,
        trackCircuitId: trackCircuit.id,
      })
    }
  }

  for (const point of state.points) {
    if (point.lockedByRouteId === route.id) {
      issues.push({
        code: 'RELEASED_ROUTE_OWNS_RESERVATION_OR_LOCK',
        severity: 'ERROR',
        message: `Released route ${route.id} still locks point ${point.id}.`,
        routeId: route.id,
        pointId: point.id,
      })
    }
  }
}
