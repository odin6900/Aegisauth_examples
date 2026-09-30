// Runnable sample: `AEGIS_APP_KEY=... node examples/quickstart.mjs`
import { Aegis, AegisError } from "../src/aegis.js";

const envOr = (key, fallback) => process.env[key] || fallback;

const aegis = new Aegis({
  baseUrl: envOr("AEGIS_BASE_URL", "http://localhost:8080"),
  appKey: envOr("AEGIS_APP_KEY", ""),
  version: "1.0.0",
});

try {
  const info = await aegis.init();
  console.log("initialized:", info.status ?? "ok");

  if (info.version?.update_required) {
    console.log("mandatory update:", info.version.latest);
    process.exit(0);
  }

  const auth = await aegis.login(
    envOr("AEGIS_USERNAME", "demo"),
    envOr("AEGIS_PASSWORD", "demo-password"),
  );
  console.log("signed in as", auth.user?.username);

  await aegis.setVariable("last_seen", new Date().toISOString());
  aegis.startHeartbeat({ onRevoked: (error) => console.log("session ended:", error.message) });

  console.log("authenticated:", await aegis.isAuthenticated());
  await aegis.logout();
  console.log("signed out.");
} catch (error) {
  if (error instanceof AegisError) {
    console.error(`Aegis error [${error.code}] ${error.message}`);
    process.exit(1);
  }
  throw error;
}
