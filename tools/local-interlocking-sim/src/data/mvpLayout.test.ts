import { describe, expect, it } from 'vitest'
import { mvpInfrastructureState } from './mvpLayout'
import type { Route, RouteId } from '../domain/types'

function routeById(routeId: RouteId): Route {
  const route = mvpInfrastructureState.routes.find(({ id }) => id === routeId)

  if (route === undefined) {
    throw new Error(`Missing route ${routeId}`)
  }

  return route
}

describe('MVP infrastructure state', () => {
  it('contains exactly 3 track circuits', () => {
    expect(mvpInfrastructureState.trackCircuits.map(({ id }) => id)).toEqual([
      'CV_ENTRY',
      'CV_1',
      'CV_2',
    ])
  })

  it('contains exactly 1 point', () => {
    expect(mvpInfrastructureState.points.map(({ id }) => id)).toEqual(['P1'])
  })

  it('contains exactly 1 signal', () => {
    expect(mvpInfrastructureState.signals.map(({ id }) => id)).toEqual(['LS_01'])
  })

  it('contains exactly 2 routes', () => {
    expect(mvpInfrastructureState.routes.map(({ id }) => id)).toEqual([
      'R_MAIN',
      'R_DIVERGING',
    ])
  })

  it('sets LS_01 default aspect to STOP', () => {
    expect(mvpInfrastructureState.signals[0].defaultAspect).toBe('STOP')
  })

  it('defines R_MAIN requirements', () => {
    const route = routeById('R_MAIN')

    expect(route.entrySignalId).toBe('LS_01')
    expect(route.destination).toBe('VIA_1')
    expect(route.requiredPointPositions).toEqual([
      {
        pointId: 'P1',
        position: 'NORMAL',
      },
    ])
    expect(route.requiredClearTrackCircuits).toEqual(['CV_ENTRY', 'CV_1'])
  })

  it('defines R_DIVERGING requirements', () => {
    const route = routeById('R_DIVERGING')

    expect(route.entrySignalId).toBe('LS_01')
    expect(route.destination).toBe('VIA_2')
    expect(route.requiredPointPositions).toEqual([
      {
        pointId: 'P1',
        position: 'REVERSE',
      },
    ])
    expect(route.requiredClearTrackCircuits).toEqual(['CV_ENTRY', 'CV_2'])
  })

  it('defines R_MAIN as conflicting with R_DIVERGING', () => {
    expect(routeById('R_MAIN').conflictsWith).toEqual(['R_DIVERGING'])
  })

  it('defines R_DIVERGING as conflicting with R_MAIN', () => {
    expect(routeById('R_DIVERGING').conflictsWith).toEqual(['R_MAIN'])
  })
})
