# Local Interlocking Simulator

Laboratory visual testbench for simplified Cloud IXL route experiments.

This is a local proof-of-concept simulator. It is not a real railway
interlocking, not SIL4, and not an EULYNX-compliant implementation.

## Scripts

- `npm run dev`
- `npm run build`
- `npm run test`
- `npm run test:watch`

## Structure

- `src/domain/`: pure TypeScript domain types and future logic.
- `src/data/`: static MVP layout data.
- `src/ui/`: React and SVG UI components.

The MVP explicitly includes `STOP` as the default safe `SignalAspect`.
