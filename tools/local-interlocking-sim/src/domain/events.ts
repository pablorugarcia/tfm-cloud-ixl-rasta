import type { EventLogEntry, InfrastructureState } from './types'

export function appendEvent(
  state: InfrastructureState,
  event: Omit<EventLogEntry, 'id' | 'sequence'>,
): InfrastructureState {
  const sequence = nextEventSequence(state.eventLog)

  return {
    ...state,
    eventLog: [
      ...state.eventLog,
      {
        id: `event-${sequence}`,
        sequence,
        ...event,
      },
    ],
  }
}

function nextEventSequence(eventLog: readonly EventLogEntry[]): number {
  return eventLog.reduce((maxSequence, entry) => {
    return Math.max(maxSequence, entry.sequence)
  }, 0) + 1
}
