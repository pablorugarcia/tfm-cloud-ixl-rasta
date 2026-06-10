import { appendEvent } from '../domain/events'
import type {
  InfrastructureState,
  InfrastructureValidationResult,
} from '../domain/types'
import { validateInfrastructureState } from '../domain/validateInfrastructureState'

export type ParseInfrastructureStateJsonResult =
  | ParseInfrastructureStateJsonAccepted
  | ParseInfrastructureStateJsonRejected

export interface ParseInfrastructureStateJsonAccepted {
  readonly accepted: true
  readonly state: InfrastructureState
  readonly validation: InfrastructureValidationResult
}

export interface ParseInfrastructureStateJsonRejected {
  readonly accepted: false
  readonly error: string
  readonly validation?: InfrastructureValidationResult
}

export type ImportInfrastructureStateJsonResult =
  | ImportInfrastructureStateJsonAccepted
  | ImportInfrastructureStateJsonRejected

export interface ImportInfrastructureStateJsonAccepted {
  readonly accepted: true
  readonly state: InfrastructureState
  readonly validation: InfrastructureValidationResult
}

export interface ImportInfrastructureStateJsonRejected {
  readonly accepted: false
  readonly state: InfrastructureState
  readonly error: string
  readonly validation?: InfrastructureValidationResult
}

export function serializeInfrastructureState(state: InfrastructureState): string {
  return `${JSON.stringify(state, null, 2)}\n`
}

export function parseInfrastructureStateJson(
  json: string,
): ParseInfrastructureStateJsonResult {
  let parsed: unknown

  try {
    parsed = JSON.parse(json)
  } catch (error) {
    return {
      accepted: false,
      error: formatParseError(error),
    }
  }

  const candidateState = parsed as InfrastructureState
  let validation: InfrastructureValidationResult

  try {
    validation = validateInfrastructureState(candidateState)
  } catch (error) {
    return {
      accepted: false,
      error: `Parsed JSON is not an InfrastructureState: ${formatParseError(error)}`,
    }
  }

  if (!validation.valid) {
    return {
      accepted: false,
      error: 'Infrastructure state validation failed.',
      validation,
    }
  }

  return {
    accepted: true,
    state: candidateState,
    validation,
  }
}

export function importInfrastructureStateJson(
  currentState: InfrastructureState,
  json: string,
): ImportInfrastructureStateJsonResult {
  const parsed = parseInfrastructureStateJson(json)

  if (!parsed.accepted) {
    return {
      accepted: false,
      state: currentState,
      error: parsed.error,
      validation: parsed.validation,
    }
  }

  return {
    accepted: true,
    state: appendEvent(parsed.state, {
      type: 'STATE_IMPORTED',
    }),
    validation: parsed.validation,
  }
}

function formatParseError(error: unknown): string {
  if (error instanceof Error) {
    return error.message
  }

  return 'Invalid JSON.'
}
