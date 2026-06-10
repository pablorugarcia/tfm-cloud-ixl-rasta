import { DEFAULT_SIGNAL_ASPECT } from '../domain/types'
import type { LayoutDefinition } from '../domain/types'

export const mvpLayout = {
  signals: [
    {
      id: 'LS_01',
      defaultAspect: DEFAULT_SIGNAL_ASPECT,
    },
  ],
  points: [
    {
      id: 'P1',
      positions: ['NORMAL', 'REVERSE'],
    },
  ],
  trackCircuits: [{ id: 'CV_ENTRY' }, { id: 'CV_1' }, { id: 'CV_2' }],
  routes: [
    {
      id: 'R_MAIN',
      fromSignal: 'LS_01',
      to: 'VIA_1',
      requiredClearTrackCircuits: ['CV_ENTRY', 'CV_1'],
      requiredPointPosition: 'NORMAL',
      conflictsWith: ['R_DIVERGING'],
      commandedAspect: 'PROCEED_MAIN',
    },
    {
      id: 'R_DIVERGING',
      fromSignal: 'LS_01',
      to: 'VIA_2',
      requiredClearTrackCircuits: ['CV_ENTRY', 'CV_2'],
      requiredPointPosition: 'REVERSE',
      conflictsWith: ['R_MAIN'],
      commandedAspect: 'PROCEED_DIVERGING',
    },
  ],
} as const satisfies LayoutDefinition
