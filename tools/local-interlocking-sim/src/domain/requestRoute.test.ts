import { describe, expect, it } from 'vitest'
import { mvpInfrastructureState } from '../data/mvpLayout'
import { requestRoute } from './requestRoute'
import type {
  InfrastructureState,
  PointId,
  Route,
  RouteId,
  RouteState,
  Signal,
  TrackCircuit,
  TrackCircuitId,
  TrackCircuitState,
} from './types'

describe('requestRoute', () => {
  it('accepts R_MAIN in the initial clear state', () => {
    const result = requestRoute(mvpInfrastructureState, 'R_MAIN')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected R_MAIN to be accepted')
    }

    expect(result.commandedAspect).toBe('PROCEED_MAIN')
    expect(routeById(result.state, 'R_MAIN').state).toBe('LOCKED')
    expect(trackCircuitById(result.state, 'CV_ENTRY')).toMatchObject({
      state: 'CLEAR',
      reservedByRouteId: 'R_MAIN',
    })
    expect(trackCircuitById(result.state, 'CV_1')).toMatchObject({
      state: 'CLEAR',
      reservedByRouteId: 'R_MAIN',
    })
    expect(pointById(result.state, 'P1')).toMatchObject({
      position: 'NORMAL',
      lockedByRouteId: 'R_MAIN',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'ROUTE_LOCKED',
      routeId: 'R_MAIN',
    })
  })

  it('accepts R_DIVERGING in the initial clear state', () => {
    const result = requestRoute(mvpInfrastructureState, 'R_DIVERGING')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected R_DIVERGING to be accepted')
    }

    expect(result.commandedAspect).toBe('PROCEED_DIVERGING')
    expect(routeById(result.state, 'R_DIVERGING').state).toBe('LOCKED')
    expect(trackCircuitById(result.state, 'CV_ENTRY')).toMatchObject({
      state: 'CLEAR',
      reservedByRouteId: 'R_DIVERGING',
    })
    expect(trackCircuitById(result.state, 'CV_2')).toMatchObject({
      state: 'CLEAR',
      reservedByRouteId: 'R_DIVERGING',
    })
    expect(pointById(result.state, 'P1')).toMatchObject({
      position: 'REVERSE',
      lockedByRouteId: 'R_DIVERGING',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'ROUTE_LOCKED',
      routeId: 'R_DIVERGING',
    })
  })

  it('rejects R_MAIN if CV_1 is OCCUPIED', () => {
    const state = withTrackCircuitState(mvpInfrastructureState, 'CV_1', 'OCCUPIED')
    const result = requestRoute(state, 'R_MAIN')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_MAIN to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRACK_CIRCUIT_OCCUPIED',
      trackCircuitId: 'CV_1',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'ROUTE_REJECTED',
      reason: result.reason,
    })
  })

  it('rejects R_DIVERGING if CV_2 is OCCUPIED', () => {
    const state = withTrackCircuitState(mvpInfrastructureState, 'CV_2', 'OCCUPIED')
    const result = requestRoute(state, 'R_DIVERGING')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_DIVERGING to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRACK_CIRCUIT_OCCUPIED',
      trackCircuitId: 'CV_2',
    })
  })

  it('rejects R_MAIN if CV_1 is RESERVED by R_DIVERGING', () => {
    const state = withTrackCircuitReservedBy(
      mvpInfrastructureState,
      'CV_1',
      'R_DIVERGING',
    )
    const result = requestRoute(state, 'R_MAIN')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_MAIN to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRACK_CIRCUIT_RESERVED',
      trackCircuitId: 'CV_1',
      reservedByRouteId: 'R_DIVERGING',
    })
  })

  it('rejects R_MAIN if R_DIVERGING is already LOCKED', () => {
    const state = withRouteState(mvpInfrastructureState, 'R_DIVERGING', 'LOCKED')
    const result = requestRoute(state, 'R_MAIN')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_MAIN to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'CONFLICTING_ROUTE_LOCKED',
      conflictingRouteId: 'R_DIVERGING',
    })
  })

  it('rejects R_DIVERGING if R_MAIN is already LOCKED', () => {
    const state = withRouteState(mvpInfrastructureState, 'R_MAIN', 'LOCKED')
    const result = requestRoute(state, 'R_DIVERGING')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_DIVERGING to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'CONFLICTING_ROUTE_LOCKED',
      conflictingRouteId: 'R_MAIN',
    })
  })

  it('rejects R_DIVERGING with CONFLICTING_ROUTE_LOCKED after R_MAIN is locked', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = requestRoute(lockedState, 'R_DIVERGING')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_DIVERGING to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'CONFLICTING_ROUTE_LOCKED',
      conflictingRouteId: 'R_MAIN',
    })
  })

  it('does not change LS_01 away from R_MAIN commanded aspect after rejecting R_DIVERGING', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = requestRoute(lockedState, 'R_DIVERGING')

    expect(signalById(result.state, 'LS_01').aspect).toBe('PROCEED_MAIN')
  })

  it('does not corrupt the already locked route state after a rejected request', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = requestRoute(lockedState, 'R_DIVERGING')

    expect(routeById(result.state, 'R_MAIN').state).toBe('LOCKED')
    expect(routeById(result.state, 'R_DIVERGING').state).toBe('FREE')
    expect(trackCircuitById(result.state, 'CV_ENTRY').reservedByRouteId).toBe(
      'R_MAIN',
    )
    expect(pointById(result.state, 'P1').lockedByRouteId).toBe('R_MAIN')
  })

  it('rejects R_DIVERGING if P1 is locked by R_MAIN', () => {
    const state = withPointLockedBy(mvpInfrastructureState, 'P1', 'R_MAIN')
    const result = requestRoute(state, 'R_DIVERGING')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected R_DIVERGING to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'POINT_LOCKED',
      pointId: 'P1',
      lockedByRouteId: 'R_MAIN',
    })
  })

  it('rejects an unknown route with ROUTE_NOT_FOUND', () => {
    const result = requestRoute(mvpInfrastructureState, 'UNKNOWN_ROUTE')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected UNKNOWN_ROUTE to be rejected')
    }

    expect(result.reason).toEqual({ code: 'ROUTE_NOT_FOUND' })
    expect(result.state.signals).toBe(mvpInfrastructureState.signals)
    expect(result.state.points).toBe(mvpInfrastructureState.points)
    expect(result.state.trackCircuits).toBe(mvpInfrastructureState.trackCircuits)
    expect(lastEvent(result.state)).toMatchObject({
      type: 'ROUTE_REJECTED',
      routeId: 'UNKNOWN_ROUTE',
      reason: { code: 'ROUTE_NOT_FOUND' },
    })
  })

  it('does not alter signals, points, or track circuits when an unknown route is rejected', () => {
    const result = requestRoute(mvpInfrastructureState, 'UNKNOWN_ROUTE')

    expect(result.state.signals).toBe(mvpInfrastructureState.signals)
    expect(result.state.points).toBe(mvpInfrastructureState.points)
    expect(result.state.trackCircuits).toBe(mvpInfrastructureState.trackCircuits)
  })

  it('keeps LS_01 at STOP when a route is rejected', () => {
    const state = withTrackCircuitState(mvpInfrastructureState, 'CV_1', 'OCCUPIED')
    const result = requestRoute(state, 'R_MAIN')

    expect(signalById(result.state, 'LS_01').aspect).toBe('STOP')
  })

  it('sets LS_01 to the commanded aspect when a route is accepted', () => {
    const result = requestRoute(mvpInfrastructureState, 'R_MAIN')

    expect(signalById(result.state, 'LS_01').aspect).toBe('PROCEED_MAIN')
  })

  it('does not mutate the input state', () => {
    const before = JSON.stringify(mvpInfrastructureState)

    const result = requestRoute(mvpInfrastructureState, 'R_MAIN')

    expect(result.state).not.toBe(mvpInfrastructureState)
    expect(JSON.stringify(mvpInfrastructureState)).toBe(before)
    expect(routeById(mvpInfrastructureState, 'R_MAIN').state).toBe('FREE')
    expect(trackCircuitById(mvpInfrastructureState, 'CV_ENTRY').state).toBe('CLEAR')
    expect(signalById(mvpInfrastructureState, 'LS_01').aspect).toBe('STOP')
  })

  it('does not mutate the input state when a request is rejected', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const before = JSON.stringify(lockedState)

    const result = requestRoute(lockedState, 'R_DIVERGING')

    expect(result.state).not.toBe(lockedState)
    expect(JSON.stringify(lockedState)).toBe(before)
  })
})

function acceptedState(result: ReturnType<typeof requestRoute>): InfrastructureState {
  if (!result.accepted) {
    throw new Error('Expected accepted route request')
  }

  return result.state
}

function withTrackCircuitState(
  state: InfrastructureState,
  trackCircuitId: TrackCircuitId,
  trackCircuitState: TrackCircuitState,
): InfrastructureState {
  return {
    ...state,
    trackCircuits: state.trackCircuits.map((trackCircuit) => {
      if (trackCircuit.id !== trackCircuitId) {
        return trackCircuit
      }

      return {
        ...trackCircuit,
        state: trackCircuitState,
      }
    }),
  }
}

function withTrackCircuitReservedBy(
  state: InfrastructureState,
  trackCircuitId: TrackCircuitId,
  routeId: RouteId,
): InfrastructureState {
  return {
    ...state,
    trackCircuits: state.trackCircuits.map((trackCircuit) => {
      if (trackCircuit.id !== trackCircuitId) {
        return trackCircuit
      }

      return {
        ...trackCircuit,
        reservedByRouteId: routeId,
      }
    }),
  }
}

function withRouteState(
  state: InfrastructureState,
  routeId: RouteId,
  routeState: RouteState,
): InfrastructureState {
  return {
    ...state,
    routes: state.routes.map((route) => {
      if (route.id !== routeId) {
        return route
      }

      return {
        ...route,
        state: routeState,
      }
    }),
  }
}

function withPointLockedBy(
  state: InfrastructureState,
  pointId: PointId,
  routeId: RouteId,
): InfrastructureState {
  return {
    ...state,
    points: state.points.map((point) => {
      if (point.id !== pointId) {
        return point
      }

      return {
        ...point,
        lockedByRouteId: routeId,
      }
    }),
  }
}

function routeById(state: InfrastructureState, routeId: RouteId): Route {
  const route = state.routes.find(({ id }) => id === routeId)

  if (route === undefined) {
    throw new Error(`Missing route ${routeId}`)
  }

  return route
}

function trackCircuitById(
  state: InfrastructureState,
  trackCircuitId: TrackCircuitId,
): TrackCircuit {
  const trackCircuit = state.trackCircuits.find(({ id }) => id === trackCircuitId)

  if (trackCircuit === undefined) {
    throw new Error(`Missing track circuit ${trackCircuitId}`)
  }

  return trackCircuit
}

function pointById(state: InfrastructureState, pointId: PointId) {
  const point = state.points.find(({ id }) => id === pointId)

  if (point === undefined) {
    throw new Error(`Missing point ${pointId}`)
  }

  return point
}

function signalById(state: InfrastructureState, signalId: Signal['id']) {
  const signal = state.signals.find(({ id }) => id === signalId)

  if (signal === undefined) {
    throw new Error(`Missing signal ${signalId}`)
  }

  return signal
}

function lastEvent(state: InfrastructureState) {
  const event = state.eventLog.at(-1)

  if (event === undefined) {
    throw new Error('Missing event log entry')
  }

  return event
}
