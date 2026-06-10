import { describe, expect, it } from 'vitest'
import { mvpInfrastructureState } from '../data/mvpLayout'
import { requestRoute } from './requestRoute'
import {
  moveTrainToCircuit,
  placeTrain,
  releaseRoute,
} from './trainOperations'
import type {
  InfrastructureState,
  PointId,
  Route,
  RouteId,
  Signal,
  TrackCircuit,
  TrackCircuitId,
  Train,
} from './types'

describe('train operations', () => {
  it('places a train on CV_ENTRY and marks CV_ENTRY occupied', () => {
    const result = placeTrain(mvpInfrastructureState, 'T1', 'CV_ENTRY')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected train placement to be accepted')
    }

    expect(trainById(result.state, 'T1').currentCircuitId).toBe('CV_ENTRY')
    expect(trackCircuitById(result.state, 'CV_ENTRY').state).toBe('OCCUPIED')
    expect(lastEvent(result.state)).toMatchObject({
      type: 'TRAIN_PLACED',
      trainId: 'T1',
      circuitId: 'CV_ENTRY',
    })
  })

  it('rejects train placement on an unknown circuit', () => {
    const result = placeTrain(mvpInfrastructureState, 'T1', 'UNKNOWN_CV')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected train placement to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'CIRCUIT_NOT_FOUND',
      circuitId: 'UNKNOWN_CV',
    })
  })

  it('rejects train placement on an occupied circuit', () => {
    const occupiedState = acceptedState(placeTrain(mvpInfrastructureState, 'T1', 'CV_ENTRY'))
    const result = placeTrain(occupiedState, 'T2', 'CV_ENTRY')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected train placement to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRACK_CIRCUIT_OCCUPIED',
      trackCircuitId: 'CV_ENTRY',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'TRAIN_OPERATION_REJECTED',
      trainId: 'T2',
      reason: result.reason,
    })
  })

  it('rejects duplicate train ids', () => {
    const occupiedState = acceptedState(placeTrain(mvpInfrastructureState, 'T1', 'CV_ENTRY'))
    const result = placeTrain(occupiedState, 'T1', 'CV_1')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected train placement to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRAIN_ALREADY_EXISTS',
      trainId: 'T1',
    })
  })

  it('moves a train from CV_ENTRY to CV_1 and clears CV_ENTRY if not reserved', () => {
    const placedState = acceptedState(
      placeTrain(mvpInfrastructureState, 'T1', 'CV_ENTRY'),
    )
    const result = moveTrainToCircuit(placedState, 'T1', 'CV_1')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected train movement to be accepted')
    }

    expect(trainById(result.state, 'T1').currentCircuitId).toBe('CV_1')
    expect(trackCircuitById(result.state, 'CV_ENTRY')).toEqual({
      id: 'CV_ENTRY',
      state: 'CLEAR',
    })
    expect(trackCircuitById(result.state, 'CV_1').state).toBe('OCCUPIED')
    expect(lastEvent(result.state)).toMatchObject({
      type: 'TRAIN_MOVED',
      trainId: 'T1',
      fromCircuitId: 'CV_ENTRY',
      toCircuitId: 'CV_1',
    })
  })

  it('moves a train from CV_ENTRY to CV_1 and keeps CV_ENTRY reserved by R_MAIN', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const placedState = acceptedState(placeTrain(lockedState, 'T1', 'CV_ENTRY'))
    const result = moveTrainToCircuit(placedState, 'T1', 'CV_1')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected train movement to be accepted')
    }

    expect(trackCircuitById(result.state, 'CV_ENTRY')).toMatchObject({
      state: 'CLEAR',
      reservedByRouteId: 'R_MAIN',
    })
    expect(trackCircuitById(result.state, 'CV_1')).toMatchObject({
      state: 'OCCUPIED',
      reservedByRouteId: 'R_MAIN',
    })
  })

  it('rejects movement for an unknown train', () => {
    const result = moveTrainToCircuit(mvpInfrastructureState, 'UNKNOWN_TRAIN', 'CV_1')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected train movement to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRAIN_NOT_FOUND',
      trainId: 'UNKNOWN_TRAIN',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'TRAIN_OPERATION_REJECTED',
      trainId: 'UNKNOWN_TRAIN',
      reason: result.reason,
    })
  })

  it('rejects movement into a circuit occupied by another train', () => {
    const firstTrainState = acceptedState(
      placeTrain(mvpInfrastructureState, 'T1', 'CV_ENTRY'),
    )
    const occupiedTargetState = acceptedState(placeTrain(firstTrainState, 'T2', 'CV_1'))
    const result = moveTrainToCircuit(occupiedTargetState, 'T1', 'CV_1')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected train movement to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'TRACK_CIRCUIT_OCCUPIED',
      trackCircuitId: 'CV_1',
    })
  })
})

describe('route release', () => {
  it('releases R_MAIN and unlocks P1', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = releaseRoute(lockedState, 'R_MAIN')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected route release to be accepted')
    }

    expect(pointById(result.state, 'P1')).toEqual({
      id: 'P1',
      position: 'NORMAL',
    })
  })

  it('releases R_MAIN and sets LS_01 to STOP', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = releaseRoute(lockedState, 'R_MAIN')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected route release to be accepted')
    }

    expect(signalById(result.state, 'LS_01').aspect).toBe('STOP')
  })

  it('clears reservations from R_MAIN circuits that are not occupied', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = releaseRoute(lockedState, 'R_MAIN')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected route release to be accepted')
    }

    expect(routeById(result.state, 'R_MAIN').state).toBe('RELEASED')
    expect(trackCircuitById(result.state, 'CV_ENTRY')).toEqual({
      id: 'CV_ENTRY',
      state: 'CLEAR',
    })
    expect(trackCircuitById(result.state, 'CV_1')).toEqual({
      id: 'CV_1',
      state: 'CLEAR',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'ROUTE_RELEASED',
      routeId: 'R_MAIN',
    })
  })

  it('clears reservations from occupied and clear circuits reserved by R_MAIN', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const occupiedState = acceptedState(placeTrain(lockedState, 'T1', 'CV_ENTRY'))
    const result = releaseRoute(occupiedState, 'R_MAIN')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected route release to be accepted')
    }

    expect(trackCircuitById(result.state, 'CV_ENTRY')).toEqual({
      id: 'CV_ENTRY',
      state: 'OCCUPIED',
    })
    expect(trackCircuitById(result.state, 'CV_1')).toEqual({
      id: 'CV_1',
      state: 'CLEAR',
    })
  })

  it('preserves track circuit state while clearing reservations', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const occupiedState = acceptedState(placeTrain(lockedState, 'T1', 'CV_ENTRY'))
    const result = releaseRoute(occupiedState, 'R_MAIN')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected route release to be accepted')
    }

    expect(trackCircuitById(result.state, 'CV_ENTRY').state).toBe('OCCUPIED')
    expect(trackCircuitById(result.state, 'CV_1').state).toBe('CLEAR')
  })

  it('leaves no clear circuit reserved by R_MAIN after release and train movement away', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const occupiedState = acceptedState(placeTrain(lockedState, 'T1', 'CV_ENTRY'))
    const releasedState = acceptedState(releaseRoute(occupiedState, 'R_MAIN'))
    const movedState = acceptedState(moveTrainToCircuit(releasedState, 'T1', 'CV_1'))

    expect(
      movedState.trackCircuits.some((trackCircuit) => {
        return (
          trackCircuit.state === 'CLEAR' &&
          trackCircuit.reservedByRouteId === 'R_MAIN'
        )
      }),
    ).toBe(false)
  })

  it('appends an event when route release is rejected', () => {
    const result = releaseRoute(mvpInfrastructureState, 'UNKNOWN_ROUTE')

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected route release to be rejected')
    }

    expect(result.reason).toEqual({ code: 'ROUTE_NOT_FOUND' })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'ROUTE_RELEASE_REJECTED',
      routeId: 'UNKNOWN_ROUTE',
      reason: result.reason,
    })
  })

  it('does not mutate input state', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const before = JSON.stringify(lockedState)

    const result = releaseRoute(lockedState, 'R_MAIN')

    expect(result.state).not.toBe(lockedState)
    expect(JSON.stringify(lockedState)).toBe(before)
    expect(routeById(lockedState, 'R_MAIN').state).toBe('LOCKED')
    expect(pointById(lockedState, 'P1').lockedByRouteId).toBe('R_MAIN')
    expect(signalById(lockedState, 'LS_01').aspect).toBe('PROCEED_MAIN')
  })

  it('does not mutate input state when train placement is rejected', () => {
    const before = JSON.stringify(mvpInfrastructureState)

    const result = placeTrain(mvpInfrastructureState, 'T1', 'UNKNOWN_CV')

    expect(result.state).not.toBe(mvpInfrastructureState)
    expect(JSON.stringify(mvpInfrastructureState)).toBe(before)
  })

  it('does not mutate input state when train movement is rejected', () => {
    const before = JSON.stringify(mvpInfrastructureState)

    const result = moveTrainToCircuit(mvpInfrastructureState, 'UNKNOWN_TRAIN', 'CV_1')

    expect(result.state).not.toBe(mvpInfrastructureState)
    expect(JSON.stringify(mvpInfrastructureState)).toBe(before)
  })

  it('does not mutate input state when route release is rejected', () => {
    const before = JSON.stringify(mvpInfrastructureState)

    const result = releaseRoute(mvpInfrastructureState, 'UNKNOWN_ROUTE')

    expect(result.state).not.toBe(mvpInfrastructureState)
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

function trainById(state: InfrastructureState, trainId: string): Train {
  const train = state.trains.find(({ id }) => id === trainId)

  if (train === undefined) {
    throw new Error(`Missing train ${trainId}`)
  }

  return train
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
