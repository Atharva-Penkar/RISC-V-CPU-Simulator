// Entry point of the simulator page (simulator/index.html).
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "./styles/base.js";
import App from "./App.jsx";
import { persistProgram } from "./state/savedProgram.js";
import { appStore } from "./state/store.js";

// Restores the program typed before a refresh, and keeps saving it.
persistProgram(appStore);

createRoot(document.getElementById("root")).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
