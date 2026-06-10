import { describe, expect, it } from 'vitest'
import { mvpInfrastructureState } from '../data/mvpLayout'
import { injectTrackCircuitOccupancy } from './faultInjection'
import { requestRoute } from './requestRoute'
import type { InfrastructureState, TrackCircuit, TrackCircuitId } from './types'

describe('injectTrackCircuitOccupancy', () => {
  it('sets track circuit occupancy and appends FAULT_INJECTED', () => {
    const result = injectTrackCircuitOccupancy(
      mvpInfrastructureState,
      'CV_1',
      'OCCUPIED',
    )

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected fault injection to be accepted')
    }

    expect(trackCircuitById(result.state, 'CV_1').state).toBe('OCCUPIED')
    expect(lastEvent(result.state)).toMatchObject({
      type: 'FAULT_INJECTED',
      circuitId: 'CV_1',
    })
  })

  it('preserves route reservation unless explicitly cleared elsewhere', () => {
    const lockedState = acceptedState(requestRoute(mvpInfrastructureState, 'R_MAIN'))
    const result = injectTrackCircuitOccupancy(lockedState, 'CV_1', 'OCCUPIED')

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error('Expected fault injection to be accepted')
    }

    expect(trackCircuitById(result.state, 'CV_1')).toMatchObject({
      state: 'OCCUPIED',
      reservedByRouteId: 'R_MAIN',
    })
  })

  it('rejects unknown circuits and appends FAULT_INJECTION_REJECTED', () => {
    const result = injectTrackCircuitOccupancy(
      mvpInfrastructureState,
      'UNKNOWN_CV',
      'OCCUPIED',
    )

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Expected fault injection to be rejected')
    }

    expect(result.reason).toEqual({
      code: 'CIRCUIT_NOT_FOUND',
      circuitId: 'UNKNOWN_CV',
    })
    expect(lastEvent(result.state)).toMatchObject({
      type: 'FAULT_INJECTION_REJECTED',
      circuitId: 'UNKNOWN_CV',
      reason: result.reason,
    })
  })

  it('does not mutate input state', () => {
    const before = JSON.stringify(mvpInfrastructureState)

    const result = injectTrackCircuitOccupancy(
      mvpInfrastructureState,
      'CV_1',
      'OCCUPIED',
    )

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

function lastEvent(state: InfrastructureState) {
  const event = state.eventLog.at(-1)

  if (event === undefined) {
    throw new Error('Missing event log entry')
  }

  return event
}
