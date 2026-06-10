import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import { mvpInfrastructureState } from '../data/mvpLayout'
import { requestRoute } from '../domain/requestRoute'
import type { InfrastructureState } from '../domain/types'
import {
  importInfrastructureStateJson,
  parseInfrastructureStateJson,
  serializeInfrastructureState,
} from './infrastructureStateJson'

describe('infrastructure state JSON helpers', () => {
  it('exports JSON that can be parsed back', () => {
    const json = serializeInfrastructureState(mvpInfrastructureState)

    expect(() => JSON.parse(json)).not.toThrow()

    const result = parseInfrastructureStateJson(json)

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error(result.error)
    }

    expect(result.state).toEqual(mvpInfrastructureState)
    expect(result.validation.valid).toBe(true)
  })

  it('keeps the checked-in example state JSON valid', () => {
    const json = readFileSync(
      new URL('../data/example-state.json', import.meta.url),
      'utf8',
    )

    const result = parseInfrastructureStateJson(json)

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error(result.error)
    }

    expect(result.validation.valid).toBe(true)
    expect(result.state).toEqual(mvpInfrastructureState)
  })

  it('imports valid state and appends a STATE_IMPORTED event', () => {
    const lockedRouteResult = requestRoute(mvpInfrastructureState, 'R_MAIN')

    if (!lockedRouteResult.accepted) {
      throw new Error('R_MAIN should lock in the MVP initial state')
    }

    const result = importInfrastructureStateJson(
      mvpInfrastructureState,
      serializeInfrastructureState(lockedRouteResult.state),
    )

    expect(result.accepted).toBe(true)

    if (!result.accepted) {
      throw new Error(result.error)
    }

    expect(result.validation.valid).toBe(true)
    expect(result.state.routes.find(({ id }) => id === 'R_MAIN')?.state).toBe(
      'LOCKED',
    )
    expect(result.state.eventLog.at(-1)).toMatchObject({
      type: 'STATE_IMPORTED',
    })
  })

  it('rejects malformed JSON', () => {
    const result = importInfrastructureStateJson(
      mvpInfrastructureState,
      '{ not valid json',
    )

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Malformed JSON should be rejected')
    }

    expect(result.state).toBe(mvpInfrastructureState)
    expect(result.validation).toBeUndefined()
    expect(result.error.length).toBeGreaterThan(0)
  })

  it('rejects parsed JSON that is not an infrastructure state', () => {
    const result = importInfrastructureStateJson(
      mvpInfrastructureState,
      '{"signals":[]}',
    )

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Structurally invalid JSON should be rejected')
    }

    expect(result.state).toBe(mvpInfrastructureState)
    expect(result.validation).toBeUndefined()
    expect(result.error).toContain('InfrastructureState')
  })

  it('rejects invalid infrastructure state', () => {
    const invalidState: InfrastructureState = {
      ...mvpInfrastructureState,
      signals: [
        {
          ...mvpInfrastructureState.signals[0],
          aspect: 'PROCEED_MAIN',
        },
      ],
    }

    const result = importInfrastructureStateJson(
      mvpInfrastructureState,
      serializeInfrastructureState(invalidState),
    )

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Invalid state should be rejected')
    }

    expect(result.state).toBe(mvpInfrastructureState)
    expect(result.validation?.valid).toBe(false)
    expect(result.validation?.issues).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          code: 'SIGNAL_ASPECT_WITHOUT_LOCKED_ROUTE',
          severity: 'ERROR',
          signalId: 'LS_01',
        }),
      ]),
    )
  })

  it('does not overwrite current state after failed import', () => {
    const currentState = requestRoute(mvpInfrastructureState, 'R_MAIN').state
    const invalidState: InfrastructureState = {
      ...mvpInfrastructureState,
      routes: [
        {
          ...mvpInfrastructureState.routes[0],
          entrySignalId:
            'UNKNOWN_SIGNAL' as InfrastructureState['signals'][number]['id'],
        },
        mvpInfrastructureState.routes[1],
      ],
    }

    const result = importInfrastructureStateJson(
      currentState,
      serializeInfrastructureState(invalidState),
    )

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Invalid state should be rejected')
    }

    expect(result.state).toBe(currentState)
    expect(result.state.routes.find(({ id }) => id === 'R_MAIN')?.state).toBe(
      'LOCKED',
    )
  })

  it('validates imported state with validateInfrastructureState', () => {
    const invalidState: InfrastructureState = {
      ...mvpInfrastructureState,
      trackCircuits: [
        {
          ...mvpInfrastructureState.trackCircuits[0],
          reservedByRouteId: 'UNKNOWN_ROUTE',
        },
        ...mvpInfrastructureState.trackCircuits.slice(1),
      ],
    }

    const result = parseInfrastructureStateJson(
      serializeInfrastructureState(invalidState),
    )

    expect(result.accepted).toBe(false)

    if (result.accepted) {
      throw new Error('Invalid state should be rejected')
    }

    expect(result.validation?.issues).toEqual(
      expect.arrayContaining([
        expect.objectContaining({
          code: 'TRACK_CIRCUIT_RESERVED_BY_UNKNOWN_ROUTE',
          relatedRouteId: 'UNKNOWN_ROUTE',
        }),
      ]),
    )
  })
})
