import { appendEvent } from './events'
import type {
  FaultInjectionResult,
  FaultInjectionRejectionReason,
  InfrastructureState,
  TrackCircuitState,
} from './types'

export function injectTrackCircuitOccupancy(
  state: InfrastructureState,
  circuitId: string,
  occupancy: TrackCircuitState,
): FaultInjectionResult {
  const targetCircuit = state.trackCircuits.find(({ id }) => id === circuitId)

  if (targetCircuit === undefined) {
    const reason: FaultInjectionRejectionReason = {
      code: 'CIRCUIT_NOT_FOUND',
      circuitId,
    }

    return {
      accepted: false,
      circuitId,
      reason,
      state: appendEvent(state, {
        type: 'FAULT_INJECTION_REJECTED',
        circuitId,
        reason,
      }),
    }
  }

  const injectedState = appendEvent(
    {
      ...state,
      trackCircuits: state.trackCircuits.map((trackCircuit) => {
        if (trackCircuit.id !== circuitId) {
          return trackCircuit
        }

        return {
          ...trackCircuit,
          state: occupancy,
        }
      }),
    },
    {
      type: 'FAULT_INJECTED',
      circuitId,
    },
  )

  return {
    accepted: true,
    circuitId,
    state: injectedState,
  }
}
