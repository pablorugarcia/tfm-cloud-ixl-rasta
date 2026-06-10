import type { InfrastructureState } from '../domain/types'

interface TrackLayoutSvgProps {
  readonly layout: InfrastructureState
}

export function TrackLayoutSvg({ layout }: TrackLayoutSvgProps) {
  const signal = layout.signals[0]
  const point = layout.points[0]

  return (
    <svg
      role="img"
      aria-labelledby="track-layout-title track-layout-description"
      viewBox="0 0 760 300"
    >
      <title id="track-layout-title">MVP railway layout</title>
      <desc id="track-layout-description">
        Signal LS_01, point P1, and track circuits CV_ENTRY, CV_1, CV_2.
      </desc>

      <rect width="760" height="300" fill="#f9fbfb" />

      <line x1="70" y1="150" x2="320" y2="150" stroke="#263238" strokeWidth="10" />
      <line x1="320" y1="150" x2="660" y2="150" stroke="#263238" strokeWidth="10" />
      <line x1="320" y1="150" x2="660" y2="80" stroke="#263238" strokeWidth="10" />

      <circle cx="96" cy="118" r="12" fill="#b42318" />
      <line x1="96" y1="130" x2="96" y2="182" stroke="#263238" strokeWidth="6" />

      <circle cx="320" cy="150" r="12" fill="#f0b429" stroke="#263238" strokeWidth="3" />

      <text x="78" y="98" fill="#172026" fontSize="20" fontWeight="700">
        {signal.id}
      </text>
      <text x="282" y="132" fill="#172026" fontSize="20" fontWeight="700">
        {point.id}
      </text>

      <text x="145" y="180" fill="#53616a" fontSize="18">
        CV_ENTRY
      </text>
      <text x="475" y="180" fill="#53616a" fontSize="18">
        CV_1
      </text>
      <text x="475" y="58" fill="#53616a" fontSize="18">
        CV_2
      </text>

      <text x="670" y="156" fill="#53616a" fontSize="18">
        VIA_1
      </text>
      <text x="670" y="86" fill="#53616a" fontSize="18">
        VIA_2
      </text>
    </svg>
  )
}
