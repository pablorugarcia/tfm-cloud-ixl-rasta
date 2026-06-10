import { describe, expect, it } from 'vitest'
import { mvpInfrastructureState } from '../data/mvpLayout'
import { requestRoute } from './requestRoute'
import { moveTrainToCircuit, placeTrain, releaseRoute } from './trainOperations'
import type {
  InfrastructureState,
  InfrastructureValidationIssue,
  InfrastructureValidationIssueCode,
} from './types'
import { validateInfrastructureState } from './validateInfrastructureState'

describe('validateInfrastructureState', () => {
  it('accepts the MVP infrastructure state', () => {
    expect(validateInfrastructureState(mvpInfrastructureState)).toEqual({
      valid: true,
      issues: [],
    })
  })

  it('treats import/export consistency checks as ERROR severity', () => {
    const result = validateInfrastructureState(
      withFirstRoute({ entrySignalId: 'UNKNOWN_SIGNAL' }),
    )

    expect(result.issues).toHaveLength(1)
    expect(result.issues[0].severity).toBe('ERROR')
  })

  it('reports routes referencing unknown entry signals', () => {
    expectIssue(
      withFirstRoute({ entrySignalId: 'UNKNOWN_SIGNAL' }),
      'ROUTE_ENTRY_SIGNAL_NOT_FOUND',
      { routeId: 'R_MAIN', signalId: 'UNKNOWN_SIGNAL' },
    )
  })

  it('reports routes requiring unknown track circuits', () => {
    expectIssue(
      withFirstRoute({ requiredClearTrackCircuits: ['UNKNOWN_CV'] }),
      'ROUTE_REQUIRED_TRACK_CIRCUIT_NOT_FOUND',
      { routeId: 'R_MAIN', trackCircuitId: 'UNKNOWN_CV' },
    )
  })

  it('reports routes requiring unknown points', () => {
    expectIssue(
      withFirstRoute({
        requiredPointPositions: [{ pointId: 'UNKNOWN_POINT', position: 'NORMAL' }],
      }),
      'ROUTE_REQUIRED_POINT_NOT_FOUND',
      { routeId: 'R_MAIN', pointId: 'UNKNOWN_POINT' },
    )
  })

  it('reports routes conflicting with unknown routes', () => {
    expectIssue(
      withFirstRoute({ conflictsWith: ['UNKNOWN_ROUTE'] }),
      'ROUTE_CONFLICT_NOT_FOUND',
      { routeId: 'R_MAIN', relatedRouteId: 'UNKNOWN_ROUTE' },
    )
  })

  it('reports track circuits reserved by unknown routes', () => {
    expectIssue(
      withTrackCircuit('CV_1', { reservedByRouteId: 'UNKNOWN_ROUTE' }),
      'TRACK_CIRCUIT_RESERVED_BY_UNKNOWN_ROUTE',
      { trackCircuitId: 'CV_1', relatedRouteId: 'UNKNOWN_ROUTE' },
    )
  })

  it('reports points locked by unknown routes', () => {
    expectIssue(
      withPoint('P1', { lockedByRouteId: 'UNKNOWN_ROUTE' }),
      'POINT_LOCKED_BY_UNKNOWN_ROUTE',
      { pointId: 'P1', relatedRouteId: 'UNKNOWN_ROUTE' },
    )
  })

  it('reports trains on unknown current circuits', () => {
    expectIssue(
      withTrains([{ id: 'T1', currentCircuitId: 'UNKNOWN_CV' }]),
      'TRAIN_CURRENT_CIRCUIT_NOT_FOUND',
      { trainId: 'T1', trackCircuitId: 'UNKNOWN_CV' },
    )
  })

  it('reports multiple trains on the same current circuit', () => {
    expectIssue(
      {
        ...withTrackCircuit('CV_ENTRY', { state: 'OCCUPIED' }),
        trains: [
          { id: 'T1', currentCircuitId: 'CV_ENTRY' },
          { id: 'T2', currentCircuitId: 'CV_ENTRY' },
        ],
      } as unknown as InfrastructureState,
      'MULTIPLE_TRAINS_ON_CIRCUIT',
      { trackCircuitId: 'CV_ENTRY' },
    )
  })

  it('reports occupied track circuits without exactly one train', () => {
    expectIssue(
      withTrackCircuit('CV_ENTRY', { state: 'OCCUPIED' }),
      'OCCUPIED_CIRCUIT_WITHOUT_EXACTLY_ONE_TRAIN',
      { trackCircuitId: 'CV_ENTRY' },
    )
  })

  it('reports trains whose current circuit is not occupied', () => {
    expectIssue(
      withTrains([{ id: 'T1', currentCircuitId: 'CV_ENTRY' }]),
      'TRAIN_ON_NON_OCCUPIED_CIRCUIT',
      { trainId: 'T1', trackCircuitId: 'CV_ENTRY' },
    )
  })

  it('reports locked routes whose required points are not locked by that route', () => {
    expectIssue(
      withFirstRoute({ state: 'LOCKED' }),
      'LOCKED_ROUTE_POINT_NOT_LOCKED',
      { routeId: 'R_MAIN', pointId: 'P1' },
    )
  })

  it('reports locked routes whose required track circuits are not reserved by that route', () => {
    expectIssue(
      withLockedRouteAndPointLockOnly(),
      'LOCKED_ROUTE_TRACK_CIRCUIT_NOT_RESERVED',
      { routeId: 'R_MAIN', trackCircuitId: 'CV_ENTRY' },
    )
  })

  it('reports non-STOP signals without a locked route from that signal', () => {
    expectIssue(
      withSignal('LS_01', { aspect: 'PROCEED_MAIN' }),
      'SIGNAL_ASPECT_WITHOUT_LOCKED_ROUTE',
      { signalId: 'LS_01' },
    )
  })

  it('reports released routes that still own reservations or locks', () => {
    const state = {
      ...withFirstRoute({ state: 'RELEASED' }),
      trackCircuits: mvpInfrastructureState.trackCircuits.map((trackCircuit) => {
        if (trackCircuit.id !== 'CV_ENTRY') {
          return trackCircuit
        }

        return {
          ...trackCircuit,
          reservedByRouteId: 'R_MAIN',
        }
      }),
    } as unknown as InfrastructureState

    expectIssue(state, 'RELEASED_ROUTE_OWNS_RESERVATION_OR_LOCK', {
      routeId: 'R_MAIN',
      trackCircuitId: 'CV_ENTRY',
    })
  })

  it('accepts a normal route, train movement, and release sequence', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const placedState = acceptedState(placeTrain(lockedState, 'T1', 'CV_ENTRY'))
    const movedState = acceptedState(moveTrainToCircuit(placedState, 'T1', 'CV_1'))
    const releasedState = acceptedState(releaseRoute(movedState, 'R_MAIN'))

    expect(validateInfrastructureState(releasedState)).toEqual({
      valid: true,
      issues: [],
    })
  })

  it('does not mutate the input state', () => {
    const before = JSON.stringify(mvpInfrastructureState)

    validateInfrastructureState(mvpInfrastructureState)

    expect(JSON.stringify(mvpInfrastructureState)).toBe(before)
  })
})

function acceptedState<T extends { readonly accepted: boolean; readonly state: InfrastructureState }>(
  result: T,
): InfrastructureState {
  if (!result.accepted) {
    throw new Error('Expected accepted operation')
  }

  return result.state
}

function expectIssue(
  state: InfrastructureState,
  code: InfrastructureValidationIssueCode,
  expectedFields: Partial<InfrastructureValidationIssue>,
) {
  const result = validateInfrastructureState(state)
  const issue = result.issues.find((candidate) => candidate.code === code)

  expect(result.valid).toBe(false)
  expect(issue).toMatchObject({
    code,
    severity: 'ERROR',
    ...expectedFields,
  })
}

function withFirstRoute(routePatch: Record<string, unknown>): InfrastructureState {
  return {
    ...mvpInfrastructureState,
    routes: mvpInfrastructureState.routes.map((route) => {
      if (route.id !== 'R_MAIN') {
        return route
      }

      return {
        ...route,
        ...routePatch,
      }
    }),
  } as unknown as InfrastructureState
}

function withTrackCircuit(
  trackCircuitId: string,
  trackCircuitPatch: Record<string, unknown>,
): InfrastructureState {
  return {
    ...mvpInfrastructureState,
    trackCircuits: mvpInfrastructureState.trackCircuits.map((trackCircuit) => {
      if (trackCircuit.id !== trackCircuitId) {
        return trackCircuit
      }

      return {
        ...trackCircuit,
        ...trackCircuitPatch,
      }
    }),
  } as unknown as InfrastructureState
}

function withPoint(pointId: string, pointPatch: Record<string, unknown>): InfrastructureState {
  return {
    ...mvpInfrastructureState,
    points: mvpInfrastructureState.points.map((point) => {
      if (point.id !== pointId) {
        return point
      }

      return {
        ...point,
        ...pointPatch,
      }
    }),
  } as unknown as InfrastructureState
}

function withSignal(
  signalId: string,
  signalPatch: Record<string, unknown>,
): InfrastructureState {
  return {
    ...mvpInfrastructureState,
    signals: mvpInfrastructureState.signals.map((signal) => {
      if (signal.id !== signalId) {
        return signal
      }

      return {
        ...signal,
        ...signalPatch,
      }
    }),
  } as unknown as InfrastructureState
}

function withTrains(trains: readonly Record<string, unknown>[]): InfrastructureState {
  return {
    ...mvpInfrastructureState,
    trains,
  } as unknown as InfrastructureState
}

function withLockedRouteAndPointLockOnly(): InfrastructureState {
  return {
    ...withFirstRoute({ state: 'LOCKED' }),
    points: mvpInfrastructureState.points.map((point) => {
      if (point.id !== 'P1') {
        return point
      }

      return {
        ...point,
        lockedByRouteId: 'R_MAIN',
      }
    }),
  } as unknown as InfrastructureState
}
