export const TRACK_CIRCUIT_STATES = ['CLEAR', 'OCCUPIED', 'RESERVED'] as const
export type TrackCircuitState = (typeof TRACK_CIRCUIT_STATES)[number]

export const POINT_POSITIONS = ['NORMAL', 'REVERSE'] as const
export type PointPosition = (typeof POINT_POSITIONS)[number]

export const SIGNAL_ASPECTS = [
  'STOP',
  'PROCEED_MAIN',
  'PROCEED_DIVERGING',
] as const
export type SignalAspect = (typeof SIGNAL_ASPECTS)[number]

export const DEFAULT_SIGNAL_ASPECT: SignalAspect = 'STOP'

export const ROUTE_STATES = [
  'FREE',
  'REQUESTED',
  'LOCKED',
  'REJECTED',
  'RELEASED',
] as const
export type RouteState = (typeof ROUTE_STATES)[number]

export type SignalId = 'LS_01'
export type PointId = 'P1'
export type TrackCircuitId = 'CV_ENTRY' | 'CV_1' | 'CV_2'
export type RouteId = 'R_MAIN' | 'R_DIVERGING'
export type RouteDestination = 'VIA_1' | 'VIA_2'

export const ROUTE_REQUEST_REJECTION_REASONS = [
  'ROUTE_NOT_FOUND',
  'TRACK_CIRCUIT_OCCUPIED',
  'TRACK_CIRCUIT_RESERVED',
  'CONFLICTING_ROUTE_LOCKED',
  'POINT_LOCKED',
] as const
export type RouteRequestRejectionReasonCode =
  (typeof ROUTE_REQUEST_REJECTION_REASONS)[number]

export type RouteRequestRejectionReason =
  | {
      readonly code: 'ROUTE_NOT_FOUND'
    }
  | {
      readonly code: 'TRACK_CIRCUIT_OCCUPIED'
      readonly trackCircuitId: TrackCircuitId
    }
  | {
      readonly code: 'TRACK_CIRCUIT_RESERVED'
      readonly trackCircuitId: TrackCircuitId
      readonly reservedByRouteId?: RouteId
    }
  | {
      readonly code: 'CONFLICTING_ROUTE_LOCKED'
      readonly conflictingRouteId: RouteId
    }
  | {
      readonly code: 'POINT_LOCKED'
      readonly pointId: PointId
      readonly lockedByRouteId?: RouteId
    }

export const EVENT_LOG_ENTRY_TYPES = ['ROUTE_REJECTED', 'ROUTE_LOCKED'] as const
export type EventLogEntryType = (typeof EVENT_LOG_ENTRY_TYPES)[number]

export interface TrackCircuit {
  readonly id: TrackCircuitId
  readonly state: TrackCircuitState
  readonly reservedByRouteId?: RouteId
}

export interface Point {
  readonly id: PointId
  readonly position: PointPosition
  readonly lockedByRouteId?: RouteId
}

export interface Signal {
  readonly id: SignalId
  readonly aspect: SignalAspect
  readonly defaultAspect: SignalAspect
}

export interface RequiredPointPosition {
  readonly pointId: PointId
  readonly position: PointPosition
}

export interface Route {
  readonly id: RouteId
  readonly entrySignalId: SignalId
  readonly destination: RouteDestination
  readonly requiredClearTrackCircuits: readonly TrackCircuitId[]
  readonly requiredPointPositions: readonly RequiredPointPosition[]
  readonly conflictsWith: readonly RouteId[]
  readonly commandedAspect: SignalAspect
  readonly state: RouteState
}

export interface EventLogEntry {
  readonly id: string
  readonly sequence: number
  readonly type: EventLogEntryType
  readonly routeId: string
  readonly reason?: RouteRequestRejectionReason
}

export interface InfrastructureState {
  readonly signals: readonly Signal[]
  readonly points: readonly Point[]
  readonly trackCircuits: readonly TrackCircuit[]
  readonly routes: readonly Route[]
  readonly eventLog: readonly EventLogEntry[]
}

export type RouteRequestResult = RouteRequestAccepted | RouteRequestRejected

export interface RouteRequestAccepted {
  readonly accepted: true
  readonly routeId: RouteId
  readonly commandedAspect: SignalAspect
  readonly state: InfrastructureState
}

export interface RouteRequestRejected {
  readonly accepted: false
  readonly routeId: string
  readonly reason: RouteRequestRejectionReason
  readonly state: InfrastructureState
}
