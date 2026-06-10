export const SIGNAL_ASPECTS = [
  'STOP',
  'PROCEED_MAIN',
  'PROCEED_DIVERGING',
] as const

export type SignalAspect = (typeof SIGNAL_ASPECTS)[number]

export const DEFAULT_SIGNAL_ASPECT: SignalAspect = 'STOP'

export type SignalId = 'LS_01'
export type PointId = 'P1'
export type TrackCircuitId = 'CV_ENTRY' | 'CV_1' | 'CV_2'
export type RouteId = 'R_MAIN' | 'R_DIVERGING'
export type RouteEndpoint = 'VIA_1' | 'VIA_2'
export type PointPosition = 'NORMAL' | 'REVERSE'

export interface SignalDefinition {
  readonly id: SignalId
  readonly defaultAspect: SignalAspect
}

export interface PointDefinition {
  readonly id: PointId
  readonly positions: readonly PointPosition[]
}

export interface TrackCircuitDefinition {
  readonly id: TrackCircuitId
}

export interface RouteDefinition {
  readonly id: RouteId
  readonly fromSignal: SignalId
  readonly to: RouteEndpoint
  readonly requiredClearTrackCircuits: readonly TrackCircuitId[]
  readonly requiredPointPosition: PointPosition
  readonly conflictsWith: readonly RouteId[]
  readonly commandedAspect: SignalAspect
}

export interface LayoutDefinition {
  readonly signals: readonly SignalDefinition[]
  readonly points: readonly PointDefinition[]
  readonly trackCircuits: readonly TrackCircuitDefinition[]
  readonly routes: readonly RouteDefinition[]
}
