import { describe, expect, it } from 'vitest'
import {
  DEFAULT_SIGNAL_ASPECT,
  EVENT_LOG_ENTRY_TYPES,
  POINT_POSITIONS,
  ROUTE_REQUEST_REJECTION_REASONS,
  ROUTE_STATES,
  SIGNAL_ASPECTS,
  TRACK_CIRCUIT_STATES,
} from './types'

describe('SignalAspect', () => {
  it('explicitly includes STOP', () => {
    expect(SIGNAL_ASPECTS).toContain('STOP')
  })

  it('uses STOP as the default safe aspect', () => {
    expect(DEFAULT_SIGNAL_ASPECT).toBe('STOP')
  })
})

describe('domain state vocabularies', () => {
  it('defines the supported track circuit states', () => {
    expect(TRACK_CIRCUIT_STATES).toEqual(['CLEAR', 'OCCUPIED', 'RESERVED'])
  })

  it('defines the supported point positions', () => {
    expect(POINT_POSITIONS).toEqual(['NORMAL', 'REVERSE'])
  })

  it('defines the supported route states', () => {
    expect(ROUTE_STATES).toEqual([
      'FREE',
      'REQUESTED',
      'LOCKED',
      'REJECTED',
      'RELEASED',
    ])
  })

  it('defines the supported route request rejection reasons', () => {
    expect(ROUTE_REQUEST_REJECTION_REASONS).toEqual([
      'ROUTE_NOT_FOUND',
      'TRACK_CIRCUIT_OCCUPIED',
      'TRACK_CIRCUIT_RESERVED',
      'CONFLICTING_ROUTE_LOCKED',
      'POINT_LOCKED',
    ])
  })

  it('defines the supported event log entry types', () => {
    expect(EVENT_LOG_ENTRY_TYPES).toEqual(['ROUTE_REJECTED', 'ROUTE_LOCKED'])
  })
})
