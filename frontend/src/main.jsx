// Entry point of the simulator page (simulator/index.html).
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "./styles/base.js";
import App from "./App.jsx";

createRoot(document.getElementById("root")).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
