import { useEffect } from "react";

import { health } from "../api/endpoints.js";
import { TYPICAL_LATENCIES } from "../state/store.js";
import { INSTRUCTION_GROUPS, SHORTCUTS } from "./instructions.js";
import styles from "./Landing.module.css";

const SIMULATOR_URL = `${import.meta.env.BASE_URL}simulator/`;
const REPOSITORY_URL = "https://github.com/Atharva-Penkar/RISC-V-CPU-Simulator";

/** The figure in the hero: a load followed by a dependent add, which waits one cycle. */
const FIGURE_ROWS = [
  { text: "lw   x5, 0[x1]", cells: ["IF", "ID", "EX", "MEM", "WB", "", ""] },
  { text: "add  x6, x5, x2", cells: ["", "IF", "ID", "stall", "EX", "MEM", "WB"] },
  { text: "sub  x7, x6, x3", cells: ["", "", "IF", "stall", "ID", "EX", "MEM"] },
];

function PipelineFigure() {
  return (
    <figure className={styles.figure}>
      <div className={styles.figureGrid} role="img" aria-label="Pipeline diagram: the add waits one cycle for the load">
        <span className={styles.figureCycle} style={{ textAlign: "left" }}>
          cycle
        </span>
        {FIGURE_ROWS[0].cells.map((_, i) => (
          <span key={i} className={styles.figureCycle}>
            {i + 1}
          </span>
        ))}
        {FIGURE_ROWS.map((row) => (
          <div key={row.text} className={styles.figureRow}>
            <span className={styles.figureText}>{row.text}</span>
            {row.cells.map((cell, i) => (
              <span key={i} className={cell === "stall" ? styles.figureStall : cell ? styles.figureStage : undefined}>
                {cell}
              </span>
            ))}
          </div>
        ))}
      </div>
      <figcaption>
        The add needs x5 from the load, which only reads memory in MEM, so it waits one cycle in ID. The simulator
        draws this chart for your own program.
      </figcaption>
    </figure>
  );
}

const FEATURES = [
  {
    title: "Forwarding",
    swatch: "var(--forward)",
    text: "A result goes straight from the EX/MEM or MEM/WB register into the next instruction's ALU input, so dependent instructions don't wait for write back.",
  },
  {
    title: "Load-use stalls",
    swatch: "var(--stall)",
    text: "An instruction that needs a loaded value right away waits one cycle in ID while a bubble moves through EX.",
  },
  {
    title: "Branch flushes",
    swatch: "var(--flush)",
    text: "Branches and jumps resolve in EX. When one is taken, the two instructions fetched behind it are thrown away.",
  },
  {
    title: "Slow operations",
    swatch: "var(--ink)",
    text: "Make multiply, divide, load or store take several cycles and watch the stages behind them hold.",
  },
];

const PANELS = [
  ["Program", "The editor. Errors are underlined as you type, and the IF/ID/EX/MEM/WB markers beside each line show where it is in the pipeline this cycle."],
  ["Pipeline", "The five stages and the instruction in each, with bubbles, stalls, flushes, forwarding and the next PC."],
  ["Timing chart", "One row per executed instruction and one column per cycle, as in a textbook pipeline diagram. Click any cell to go to that cycle."],
  ["What happened", "Every event of the current cycle in plain English: write backs, memory accesses, forwarding, stalls, flushes."],
  ["State", "Registers and data memory at the current cycle, in hex, signed or unsigned. Registers the program has written are marked, and what changed this cycle is highlighted."],
  ["Statistics", "Cycles, instructions, CPI, stalls, flushes, and how the program ended."],
];

export default function Landing() {
  // Cloud Run starts the backend on demand; waking it now makes the simulator's first run faster.
  useEffect(() => {
    const controller = new AbortController();
    health({ signal: controller.signal }).catch(() => {});
    return () => controller.abort();
  }, []);

  // A link such as …/#instructions arrives before React has drawn the section; scroll to it once it exists.
  useEffect(() => {
    if (window.location.hash) document.getElementById(window.location.hash.slice(1))?.scrollIntoView();
  }, []);

  return (
    <div className={styles.page}>
      <header className={styles.bar}>
        <a className={styles.brand} href={import.meta.env.BASE_URL}>
          RISC-V Pipeline Simulator
        </a>
        <nav className={`mono ${styles.nav}`} aria-label="Sections">
          <a href="#guide">guide</a>
          <a href="#instructions">instructions</a>
          <a href="#timing">timing</a>
          <a href={REPOSITORY_URL}>source</a>
        </nav>
      </header>

      <main>
        <section className={styles.hero}>
          <div className={styles.heroText}>
            <p className={`mono ${styles.kicker}`}>RV32IM · 5-stage pipeline · C++ simulator</p>
            <h1 className={styles.headline}>Watch a RISC-V program move through a pipelined CPU, one clock cycle at a time.</h1>
            <p className={styles.lede}>
              Write assembly, run it on a cycle-accurate model of the classic IF, ID, EX, MEM, WB pipeline, and step
              through what every stage does: forwarding, stalls, flushes and every register and memory write,
              explained in plain English.
            </p>
            <div className={styles.actions}>
              <a className={styles.primary} href={SIMULATOR_URL}>
                Open the simulator →
              </a>
              <a className={styles.secondary} href="#guide">
                Read the guide
              </a>
            </div>
          </div>
          <PipelineFigure />
        </section>

        <section className={styles.section} aria-labelledby="shows">
          <h2 id="shows" className={`mono ${styles.sectionTitle}`}>
            What it shows
          </h2>
          <div className={styles.features}>
            {FEATURES.map((feature) => (
              <article key={feature.title} className={styles.feature}>
                <h3>
                  <span className={styles.swatch} style={{ background: feature.swatch }} aria-hidden="true" />
                  {feature.title}
                </h3>
                <p>{feature.text}</p>
              </article>
            ))}
          </div>
        </section>

        <section id="guide" className={styles.section} aria-labelledby="guide-title">
          <h2 id="guide-title" className={`mono ${styles.sectionTitle}`}>
            Guide
          </h2>
          <ol className={styles.steps}>
            <li>
              <h3>Write or pick a program</h3>
              <p>
                Type RV32IM assembly in the Program panel, or choose one from the <span className="mono">examples</span>{" "}
                menu. The program runs on its own about half a second after you stop typing. If it does not assemble,
                the errors are underlined and listed under the editor, and the last run that worked stays on screen.
              </p>
            </li>
            <li>
              <h3>Step through the cycles</h3>
              <p>
                Use the controls at the bottom to play, pause or step, or drag the slider. Its coloured ticks mark the
                cycles with a stall (blue), forwarding (green) or a flush (red).
              </p>
              <table className={styles.shortcuts}>
                <caption className="visually-hidden">Keyboard shortcuts</caption>
                <tbody>
                  {SHORTCUTS.map((shortcut) => (
                    <tr key={shortcut.action}>
                      <td>
                        {shortcut.keys.map((key, i) => (
                          <span key={key}>
                            {i > 0 && " or "}
                            <kbd>{key}</kbd>
                          </span>
                        ))}
                      </td>
                      <td>{shortcut.action}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </li>
            <li>
              <h3>Read the panels</h3>
              <dl className={styles.panels}>
                {PANELS.map(([name, text]) => (
                  <div key={name}>
                    <dt className="mono">{name}</dt>
                    <dd>{text}</dd>
                  </div>
                ))}
              </dl>
              <p className={styles.note}>
                Every instruction keeps one colour across the page, and a repeated execution, such as a loop body,
                carries a badge (×2, ×3) so you can tell the trips around the loop apart.
              </p>
            </li>
            <li>
              <h3>Change the pipeline</h3>
              <p>
                The <span className="mono">pipeline</span> menu turns forwarding off, so every dependent instruction
                waits for write back, and sets how many cycles a multiply, a divide, a load and a store take. Run the
                same program both ways and compare the cycles and CPI to see what each piece of hardware is worth. See{" "}
                <a href="#timing">the timing model</a> below.
              </p>
            </li>
          </ol>
        </section>

        <section id="instructions" className={styles.section} aria-labelledby="instructions-title">
          <h2 id="instructions-title" className={`mono ${styles.sectionTitle}`}>
            Instructions and syntax
          </h2>
          <div className={styles.syntax}>
            <pre className={styles.code}>
              {`# Sums 4 + 3 + 2 + 1, then stores it.
        addi x1, x0, 4      # counter
        addi x2, x0, 0      # sum
loop:   add  x2, x2, x1
        addi x1, x1, -1
        bne  x1, x0, loop
        sw   x2, 0[x0]; ebreak`}
            </pre>
            <ul className={styles.rules}>
              <li>One instruction per line, or several separated by <code>;</code>. <code>#</code> starts a comment.</li>
              <li>
                Labels are <code>name:</code>, on their own line or before an instruction. Branches and <code>jal</code>{" "}
                jump to labels.
              </li>
              <li>
                Registers are <code>x0</code> to <code>x31</code>. <code>x0</code> always reads 0. ABI names such as{" "}
                <code>t0</code> or <code>sp</code> are not accepted.
              </li>
              <li>
                Immediates are decimal (<code>-2</code>) or hexadecimal (<code>0xff</code>). Memory operands are{" "}
                <code>imm[reg]</code> or <code>imm(reg)</code>.
              </li>
              <li>Mnemonics, registers and labels are not case-sensitive.</li>
              <li>
                Pseudo-instructions (<code>li</code>, <code>mv</code>, <code>j</code>, <code>ret</code>, <code>nop</code>)
                are not supported; write <code>addi x1, x0, 5</code> for <code>li x1, 5</code>.
              </li>
              <li>Data memory is 256 bytes, little-endian. Accesses must be aligned to their size.</li>
            </ul>
          </div>
          <div className={styles.tableScroll}>
            <table className={styles.instructions}>
              <thead>
                <tr>
                  <th scope="col">Group</th>
                  <th scope="col">Instructions</th>
                  <th scope="col">Syntax</th>
                  <th scope="col">What it does</th>
                </tr>
              </thead>
              <tbody>
                {INSTRUCTION_GROUPS.map((group) => (
                  <tr key={group.name}>
                    <th scope="row">{group.name}</th>
                    <td className="mono">{group.mnemonics.join(" ")}</td>
                    <td className="mono">
                      {group.syntax}
                      <span className={styles.example}>{group.example}</span>
                    </td>
                    <td>{group.description}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>

        <section id="timing" className={styles.section} aria-labelledby="timing-title">
          <h2 id="timing-title" className={`mono ${styles.sectionTitle}`}>
            Timing model
          </h2>
          <div className={styles.timing}>
            <div>
              <p>
                By default every stage takes one cycle, as in the textbook 5-stage pipeline. Real hardware is slower
                for some operations: a multiplier takes a few cycles, a divider works out one bit or two per cycle, and
                memory can take longer than one cycle to answer.
              </p>
              <p>
                In the <span className="mono">pipeline</span> menu you can make a multiply or divide stay in EX, and a load
                or store stay in MEM, for up to 100 cycles. While it is busy, the stages behind it hold their
                instructions and bubbles move on to the stages ahead, so the cost appears in the timing chart, the
                statistics and the CPI. The results never change, only when they arrive.
              </p>
            </div>
            <table className={styles.latencies}>
              <caption>Typical latencies in a simple in-order core</caption>
              <thead>
                <tr>
                  <th scope="col">Operation</th>
                  <th scope="col">Stage</th>
                  <th scope="col">Typical cycles</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <th scope="row">Multiply</th>
                  <td className="mono">EX</td>
                  <td>3 to 5 (pipelined multiplier)</td>
                </tr>
                <tr>
                  <th scope="row">Divide, remainder</th>
                  <td className="mono">EX</td>
                  <td>about 32 (one bit per cycle), 16 to 20 with a faster divider</td>
                </tr>
                <tr>
                  <th scope="row">Load, store</th>
                  <td className="mono">MEM</td>
                  <td>1 to 3 on a cache hit, tens to hundreds on a miss</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p className={styles.note}>
            The menu&apos;s <span className="mono">typical</span> preset uses {TYPICAL_LATENCIES.multiply} for multiply,{" "}
            {TYPICAL_LATENCIES.divide} for divide and {TYPICAL_LATENCIES.load} for loads and stores. Try the{" "}
            <span className="mono">multiply</span> example with it.
          </p>
        </section>

        <section className={styles.closing}>
          <p>Ready to try it?</p>
          <a className={styles.primary} href={SIMULATOR_URL}>
            Open the simulator →
          </a>
        </section>
      </main>

      <footer className={`mono ${styles.footer}`}>
        <span>Assembler and simulator in C++20 · web interface in React</span>
        <a href={REPOSITORY_URL}>Source on GitHub</a>
      </footer>
    </div>
  );
}
