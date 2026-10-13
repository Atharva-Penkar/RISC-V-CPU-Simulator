// Entry point of the landing page (index.html).
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "../styles/base.js";
import Landing from "./Landing.jsx";

createRoot(document.getElementById("root")).render(
  <StrictMode>
    <Landing />
  </StrictMode>,
);
