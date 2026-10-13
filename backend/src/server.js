import { createApp } from "./app.js";
import { loadConfig } from "./config.js";
import { loadExamples } from "./examples.js";
import { Simulator } from "./simulator.js";

const config = loadConfig();
const simulator = new Simulator(config);
const examples = await loadExamples(config.examplesDir);

const available = await simulator.available();
if (!available.rvsim || !available.rvasm) {
  console.warn(
    `Simulator executables not found (rvsim: ${config.rvsimPath}, rvasm: ${config.rvasmPath}).\n` +
      "Build them with `npm run build:simulator` or set RVSIM_PATH and RVASM_PATH.",
  );
}

createApp({ config, simulator, examples }).listen(config.port, () => {
  console.log(`RISC-V simulator API listening on http://localhost:${config.port}`);
});
