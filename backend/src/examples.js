import { readdir, readFile } from "node:fs/promises";
import path from "node:path";

/**
 * Loads the example programs (*.s) once. The first comment line of a file is its description.
 * Returns [] when the directory does not exist.
 */
export async function loadExamples(directory) {
  let files;
  try {
    files = (await readdir(directory)).filter((file) => file.endsWith(".s")).sort();
  } catch (error) {
    if (error.code === "ENOENT") return [];
    throw error;
  }
  return Promise.all(
    files.map(async (file) => {
      const source = await readFile(path.join(directory, file), "utf8");
      const firstLine = source.split("\n", 1)[0].trim();
      return {
        id: path.basename(file, ".s"),
        description: firstLine.startsWith("#") ? firstLine.slice(1).trim() : "",
        source,
      };
    }),
  );
}
