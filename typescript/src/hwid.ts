import { createHash } from "node:crypto";

let cached: string | null = null;

/**
 * Stable, non-reversible machine identifier.
 * Uses OS facts on Node and a persisted random id in the browser.
 */
export function hardwareId(): string {
  if (cached) return cached;

  const g = globalThis as Record<string, any>;

  if (typeof g.window !== "undefined" && g.localStorage) {
    const stored = g.localStorage.getItem("aegis.hwid");
    if (stored) return (cached = stored);
    const bytes = new Uint8Array(32);
    g.crypto.getRandomValues(bytes);
    const generated = Array.from(bytes, (b: number) => b.toString(16).padStart(2, "0")).join("");
    g.localStorage.setItem("aegis.hwid", generated);
    return (cached = generated);
  }

  const os = g.process?.platform ?? "unknown";
  const arch = g.process?.arch ?? "unknown";
  const user = g.process?.env?.USER ?? g.process?.env?.USERNAME ?? "";
  const host = g.process?.env?.HOSTNAME ?? g.process?.env?.COMPUTERNAME ?? "";
  cached = createHash("sha256").update([os, arch, user, host].join("|")).digest("hex");
  return cached;
}
