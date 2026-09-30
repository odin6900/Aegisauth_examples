# Aegis SDK for TypeScript / JavaScript

Official client for the Aegis Authentication API. Works in Node.js 18+, Bun,
Deno, Electron main processes and modern browsers (any runtime with `fetch`).

## Contents

```
src/client.ts   Aegis client with every API operation
src/errors.ts   AegisError with typed error codes
src/hwid.ts     Stable hardware id (Node + browser)
src/types.ts    Request/response types
examples/       Runnable quickstart
```

## Install

No registry needed — unzip and use the folder directly:

```bash
npm install ./aegis-sdk-typescript
# or build it in place
npm install && npm run build
```

You can also copy `src/` into your project and import `./aegis/index.ts`.

## Quickstart

```ts
import { Aegis, AegisError } from "@aegis/sdk";

const aegis = new Aegis({
  baseUrl: "https://your-aegis-host",
  appKey: process.env.AEGIS_APP_KEY!,
  version: "1.0.0",
});

const init = await aegis.init();
if (init.version?.update_required) throw new Error(`Update to ${init.version.latest}`);

const { user, license } = await aegis.login({ username: "ada", password: secret });
console.log(`Signed in as ${user.username}`, license?.status);

const check = await aegis.validateLicense("AEGS-4K7P-2M9X-QT31");
if (!check.valid) console.error(check.status);

await aegis.setVariable("last_level", "12");
const { variables } = await aegis.getVariables("user");

const stop = aegis.startHeartbeat({ intervalMs: 60_000, onRevoked: (r) => app.lock(r) });
// later
stop();
await aegis.logout();
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`checkSession`, `isAuthenticated`, `useSession`, `userData`, `validateLicense`,
`activateLicense`, `getVariables`, `setVariable`, `checkVersion`, `downloads`,
`triggerWebhook`, plus `request()` for any endpoint added later.

## Error handling

```ts
try {
  await aegis.login({ username, password });
} catch (error) {
  if (error instanceof AegisError && error.code === "hwid_mismatch") {
    ui.show("This license is locked to another machine.");
  } else if (error instanceof AegisError && error.isNetworkError) {
    ui.show("Aegis is unreachable — retrying.");
  } else {
    throw error;
  }
}
```

## Browser note

Never ship a server-side API key to a browser bundle. The application public
key (`appKey`) is safe to embed; `apiKey` is for trusted backends only.

## License

MIT — see `LICENSE`.
