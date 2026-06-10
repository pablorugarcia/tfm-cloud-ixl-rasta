import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'

describe('InterlockingSimulator domain boundary', () => {
  it('uses the domain fault-injection helper for fault toggles', () => {
    const source = readFileSync(
      new URL('./InterlockingSimulator.tsx', import.meta.url),
      'utf8',
    )

    expect(source).toContain("from '../domain/faultInjection'")
    expect(source).toContain('injectTrackCircuitOccupancy(')
    expect(source).not.toContain('trackCircuits: currentState.trackCircuits.map')
  })
})
