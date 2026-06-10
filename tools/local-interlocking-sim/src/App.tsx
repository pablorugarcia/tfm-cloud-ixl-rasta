import { mvpInfrastructureState } from './data/mvpLayout'
import { InterlockingSimulator } from './ui/InterlockingSimulator'
import './styles.css'

function App() {
  return <InterlockingSimulator layout={mvpInfrastructureState} />
}

export default App
