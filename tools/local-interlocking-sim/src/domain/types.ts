export const TRACK_CIRCUIT_STATES = ['CLEAR', 'OCCUPIED'] as const
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
      readonly reservedByRouteId?: string
    }
  | {
      readonly code: 'CONFLICTING_ROUTE_LOCKED'
      readonly conflictingRouteId: RouteId
    }
  | {
      readonly code: 'POINT_LOCKED'
      readonly pointId: PointId
      readonly lockedByRouteId?: string
    }

export const TRAIN_OPERATION_REJECTION_REASONS = [
  'CIRCUIT_NOT_FOUND',
  'TRACK_CIRCUIT_OCCUPIED',
  'TRAIN_NOT_FOUND',
  'TRAIN_ALREADY_EXISTS',
] as const
export type TrainOperationRejectionReasonCode =
  (typeof TRAIN_OPERATION_REJECTION_REASONS)[number]

export type TrainOperationRejectionReason =
  | {
      readonly code: 'CIRCUIT_NOT_FOUND'
      readonly circuitId: string
    }
  | {
      readonly code: 'TRACK_CIRCUIT_OCCUPIED'
      readonly trackCircuitId: string
    }
  | {
      readonly code: 'TRAIN_NOT_FOUND'
      readonly trainId: string
    }
  | {
      readonly code: 'TRAIN_ALREADY_EXISTS'
      readonly trainId: string
    }

export type RouteReleaseRejectionReason = {
  readonly code: 'ROUTE_NOT_FOUND'
}

export type FaultInjectionRejectionReason = {
  readonly code: 'CIRCUIT_NOT_FOUND'
  readonly circuitId: string
}

export const EVENT_LOG_ENTRY_TYPES = [
  'ROUTE_REJECTED',
  'ROUTE_LOCKED',
  'TRAIN_PLACED',
  'TRAIN_MOVED',
  'TRAIN_OPERATION_REJECTED',
  'ROUTE_RELEASED',
  'ROUTE_RELEASE_REJECTED',
  'FAULT_INJECTED',
  'FAULT_INJECTION_REJECTED',
  'STATE_IMPORTED',
] as const
export type EventLogEntryType = (typeof EVENT_LOG_ENTRY_TYPES)[number]

export interface TrackCircuit {
  readonly id: TrackCircuitId
  readonly state: TrackCircuitState
  readonly reservedByRouteId?: string
}

export interface Point {
  readonly id: PointId
  readonly position: PointPosition
  readonly lockedByRouteId?: string
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
  readonly routeId?: string
  readonly trainId?: string
  readonly circuitId?: string
  readonly fromCircuitId?: string
  readonly toCircuitId?: string
  readonly reason?:
    | RouteRequestRejectionReason
    | TrainOperationRejectionReason
    | RouteReleaseRejectionReason
    | FaultInjectionRejectionReason
}

export interface Train {
  readonly id: string
  readonly currentCircuitId: string
  readonly authorisedRouteId?: string
}

export interface InfrastructureState {
  readonly signals: readonly Signal[]
  readonly points: readonly Point[]
  readonly trackCircuits: readonly TrackCircuit[]
  readonly routes: readonly Route[]
  readonly trains: readonly Train[]
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

export type TrainOperationResult =
  | TrainOperationAccepted
  | TrainOperationRejected

export interface TrainOperationAccepted {
  readonly accepted: true
  readonly trainId: string
  readonly state: InfrastructureState
}

export interface TrainOperationRejected {
  readonly accepted: false
  readonly trainId?: string
  readonly reason: TrainOperationRejectionReason
  readonly state: InfrastructureState
}

export type RouteReleaseResult = RouteReleaseAccepted | RouteReleaseRejected

export interface RouteReleaseAccepted {
  readonly accepted: true
  readonly routeId: RouteId
  readonly state: InfrastructureState
}

export interface RouteReleaseRejected {
  readonly accepted: false
  readonly routeId: string
  readonly reason: RouteReleaseRejectionReason
  readonly state: InfrastructureState
}

export type FaultInjectionResult =
  | FaultInjectionAccepted
  | FaultInjectionRejected

export interface FaultInjectionAccepted {
  readonly accepted: true
  readonly circuitId: string
  readonly state: InfrastructureState
}

export interface FaultInjectionRejected {
  readonly accepted: false
  readonly circuitId: string
  readonly reason: FaultInjectionRejectionReason
  readonly state: InfrastructureState
}

export const INFRASTRUCTURE_VALIDATION_SEVERITIES = ['ERROR', 'WARNING'] as const
export type InfrastructureValidationSeverity =
  (typeof INFRASTRUCTURE_VALIDATION_SEVERITIES)[number]

export const INFRASTRUCTURE_VALIDATION_ISSUE_CODES = [
  'ROUTE_ENTRY_SIGNAL_NOT_FOUND',
  'ROUTE_REQUIRED_TRACK_CIRCUIT_NOT_FOUND',
  'ROUTE_REQUIRED_POINT_NOT_FOUND',
  'ROUTE_CONFLICT_NOT_FOUND',
  'TRACK_CIRCUIT_RESERVED_BY_UNKNOWN_ROUTE',
  'POINT_LOCKED_BY_UNKNOWN_ROUTE',
  'TRAIN_CURRENT_CIRCUIT_NOT_FOUND',
  'MULTIPLE_TRAINS_ON_CIRCUIT',
  'OCCUPIED_CIRCUIT_WITHOUT_EXACTLY_ONE_TRAIN',
  'TRAIN_ON_NON_OCCUPIED_CIRCUIT',
  'LOCKED_ROUTE_POINT_NOT_LOCKED',
  'LOCKED_ROUTE_TRACK_CIRCUIT_NOT_RESERVED',
  'SIGNAL_ASPECT_WITHOUT_LOCKED_ROUTE',
  'RELEASED_ROUTE_OWNS_RESERVATION_OR_LOCK',
] as const
export type InfrastructureValidationIssueCode =
  (typeof INFRASTRUCTURE_VALIDATION_ISSUE_CODES)[number]

export interface InfrastructureValidationIssue {
  readonly code: InfrastructureValidationIssueCode
  readonly severity: InfrastructureValidationSeverity
  readonly message: string
  readonly routeId?: string
  readonly signalId?: string
  readonly pointId?: string
  readonly trackCircuitId?: string
  readonly trainId?: string
  readonly relatedRouteId?: string
}

export interface InfrastructureValidationResult {
  readonly valid: boolean
  readonly issues: readonly InfrastructureValidationIssue[]
}
