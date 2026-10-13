import { useId } from "react";

import styles from "./Panel.module.css";

/**
 * A titled region of the page.
 * @param {object} props
 * @param {string} props.title
 * @param {import("react").ReactNode} [props.actions]  controls shown at the right of the title
 * @param {import("react").ReactNode} [props.caption]  a line under the title, e.g. what the panel shows
 * @param {string} [props.className]
 * @param {import("react").ReactNode} props.children
 */
export default function Panel({ title, actions, caption, className, children }) {
  const titleId = useId();
  return (
    <section className={[styles.panel, className].filter(Boolean).join(" ")} aria-labelledby={titleId}>
      <header className={styles.header}>
        <h2 id={titleId} className={`mono ${styles.title}`}>
          {title}
        </h2>
        {actions && <div className={styles.actions}>{actions}</div>}
      </header>
      {caption && <p className={styles.caption}>{caption}</p>}
      <div className={styles.body}>{children}</div>
    </section>
  );
}
