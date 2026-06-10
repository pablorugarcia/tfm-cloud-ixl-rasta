import { DEFAULT_SIGNAL_ASPECT } from '../domain/types'
import type { InfrastructureState } from '../domain/types'

export const mvpInfrastructureState = {
  signals: [
    {
      id: 'LS_01',
      aspect: DEFAULT_SIGNAL_ASPECT,
      defaultAspect: DEFAULT_SIGNAL_ASPECT,
    },
  ],
  points: [
    {
      id: 'P1',
      position: 'NORMAL',
    },
  ],
  trackCircuits: [
    { id: 'CV_ENTRY', state: 'CLEAR' },
    { id: 'CV_1', state: 'CLEAR' },
    { id: 'CV_2', state: 'CLEAR' },
  ],
  routes: [
    {
      id: 'R_MAIN',
      entrySignalId: 'LS_01',
      destination: 'VIA_1',
      requiredClearTrackCircuits: ['CV_ENTRY', 'CV_1'],
      requiredPointPositions: [{ pointId: 'P1', position: 'NORMAL' }],
      conflictsWith: ['R_DIVERGING'],
      commandedAspect: 'PROCEED_MAIN',
      state: 'FREE',
    },
    {
      id: 'R_DIVERGING',
      entrySignalId: 'LS_01',
      destination: 'VIA_2',
      requiredClearTrackCircuits: ['CV_ENTRY', 'CV_2'],
      requiredPointPositions: [{ pointId: 'P1', position: 'REVERSE' }],
      conflictsWith: ['R_MAIN'],
      commandedAspect: 'PROCEED_DIVERGING',
      state: 'FREE',
    },
  ],
  trains: [],
  eventLog: [],
} as const satisfies InfrastructureState
