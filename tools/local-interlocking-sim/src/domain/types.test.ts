import { describe, expect, it } from 'vitest'
import { DEFAULT_SIGNAL_ASPECT, SIGNAL_ASPECTS } from './types'

describe('SignalAspect', () => {
  it('explicitly includes STOP', () => {
    expect(SIGNAL_ASPECTS).toContain('STOP')
  })

  it('uses STOP as the default safe aspect', () => {
    expect(DEFAULT_SIGNAL_ASPECT).toBe('STOP')
  })
})
