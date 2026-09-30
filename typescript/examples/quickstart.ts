/**
 * Runnable quickstart for the Aegis TypeScript SDK.
 *
 *   AEGIS_BASE_URL=https://your-aegis-host AEGIS_APP_KEY=pk_live_... npx tsx examples/quickstart.ts
 */
import { Aegis, AegisError } from "../src/index";

const aegis = new Aegis({
  baseUrl: process.env.AEGIS_BASE_URL ?? "http://localhost:8080",
  appKey: process.env.AEGIS_APP_KEY ?? "",
  version: "1.0.0",
});

async function main() {
  const init = await aegis.init();
  console.log("Initialized:", init.status);

  if (init.version?.update_required) {
    console.log("Mandatory update:", init.version.latest, init.version.download_url);
    return;
  }

  const username = process.env.AEGIS_USERNAME ?? "demo";
  const password = process.env.AEGIS_PASSWORD ?? "demo-password";

  const { user, license } = await aegis.login({ username, password });
  console.log("Signed in as", user.username, "license:", license?.status ?? "none");

  // Multi-subscription check example:
  if (aegis.hasSubscription("VIP")) {
    console.log("[Subscription] VIP tier active — Premium features unlocked!");
  } else {
    console.log("[Subscription] Standard tier active.");
  }
  console.log("Active subscriptions:", aegis.getSubscriptions().map((s) => s.name));

  const vars = await aegis.getVariables("user");
  console.log("User variables:", vars.variables);
  await aegis.setVariable("last_seen", new Date().toISOString());

  const stop = aegis.startHeartbeat({
    intervalMs: 60_000,
    onRevoked: (reason) => console.warn("Session revoked:", reason),
  });

  await new Promise((resolve) => setTimeout(resolve, 2_000));
  stop();
  await aegis.logout();
  console.log("Signed out.");
}

main().catch((error) => {
  if (error instanceof AegisError) {
    console.error(`Aegis error [${error.code}] ${error.message}`);
    process.exit(1);
  }
  throw error;
});
