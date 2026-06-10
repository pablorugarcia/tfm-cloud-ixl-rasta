import { DEFAULT_SIGNAL_ASPECT } from '../domain/types'
import type { InfrastructureState } from '../domain/types'
import { TrackLayoutSvg } from './TrackLayoutSvg'

interface InterlockingSimulatorProps {
  readonly layout: InfrastructureState
}

export function InterlockingSimulator({ layout }: InterlockingSimulatorProps) {
  return (
    <main className="app-shell">
      <header className="app-header">
        <h1>Local Interlocking Simulator</h1>
        <p>Laboratory SVG testbench for the MVP layout.</p>
      </header>

      <section className="simulator" aria-label="Interlocking simulator">
        <div className="track-layout">
          <TrackLayoutSvg layout={layout} />
        </div>

        <aside className="side-panel" aria-label="Simulator status">
          <section className="panel-section">
            <h2>Signal</h2>
            <p className="status-row">
              <span>{layout.signals[0].id}</span>
              <strong className="aspect-stop">{DEFAULT_SIGNAL_ASPECT}</strong>
            </p>
          </section>

          <section className="panel-section">
            <h2>Routes</h2>
            {layout.routes.map((route) => (
              <p className="status-row" key={route.id}>
                <span>{route.id}</span>
                <strong>{route.commandedAspect}</strong>
              </p>
            ))}
          </section>
        </aside>
      </section>
    </main>
  )
}
