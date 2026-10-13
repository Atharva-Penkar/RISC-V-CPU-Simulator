import Panel from "./components/Panel.jsx";
import RunStatus from "./features/controls/RunStatus.jsx";
import TransportBar from "./features/controls/TransportBar.jsx";
import Datapath from "./features/datapath/Datapath.jsx";
import AssemblyEditor from "./features/editor/AssemblyEditor.jsx";
import EventLog from "./features/event-log/EventLog.jsx";
import ExamplePicker, { ExampleCaption } from "./features/examples/ExamplePicker.jsx";
import MachinePanel from "./features/machine/MachinePanel.jsx";
import StatsPanel from "./features/stats/StatsPanel.jsx";
import TimingChart from "./features/timing-chart/TimingChart.jsx";
import AppLayout from "./layout/AppLayout.jsx";
import { useAppState } from "./state/store.js";
import { usePlayback } from "./state/usePlayback.js";
import { useSimulation } from "./state/useSimulation.js";

export default function App() {
  useSimulation();
  usePlayback();
  // Shown until the program is edited.
  const example = useAppState((s) => s.example);

  return (
    <AppLayout
      status={<RunStatus />}
      editor={
        <Panel title="Program" actions={<ExamplePicker />} caption={example && <ExampleCaption example={example} />}>
          <AssemblyEditor />
        </Panel>
      }
      datapath={
        <Panel title="Pipeline">
          <Datapath />
        </Panel>
      }
      timing={
        <Panel title="Timing chart">
          <TimingChart />
        </Panel>
      }
      events={
        <Panel title="What happened">
          <EventLog />
        </Panel>
      }
      machine={<MachinePanel />}
      stats={
        <Panel title="Statistics">
          <StatsPanel />
        </Panel>
      }
      controls={<TransportBar />}
    />
  );
}
