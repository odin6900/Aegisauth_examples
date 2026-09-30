# Aegis SDK for JavaScript

Official client for the Aegis Authentication API. Plain ES modules — no build
step, no dependencies. Runs in browsers, Node 18+, Deno, Bun and edge workers.

> Using TypeScript? The dedicated TypeScript SDK ships full generics and
> typed response models. This package is for vanilla JS projects (it still
> includes `aegis.d.ts` for editor autocomplete).

## Contents

```
src/aegis.js              Client, errors, hardware id
src/aegis.d.ts            Type declarations for editors
examples/quickstart.mjs   Node sample application
examples/browser.html     Browser sample application
package.json              Package manifest
```

## Install

No package registry — unzip and import the file directly:

```html
<script type="module">
  import { Aegis } from "./vendor/aegis-javascript/src/aegis.js";
</script>
```

```js
// Node
import { Aegis } from "./vendor/aegis-javascript/src/aegis.js";
```

## Quickstart

```js
const aegis = new Aegis({ baseUrl: "https://your-aegis-host", appKey: APP_KEY, version: "1.0.0" });
await aegis.init();

const auth = await aegis.login("ada", password);
await aegis.validateLicense("AEGS-4K7P-2M9X-QT31");
aegis.startHeartbeat({ onRevoked: (error) => signOut(error.message) });
await aegis.logout();
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`startHeartbeat`/`stopHeartbeat`, `checkSession`, `isAuthenticated`,
`useSession`, `userData`, `validateLicense`, `activateLicense`,
`getVariables`, `setVariable`, `checkVersion`, `downloads`, `triggerWebhook`,
plus `request()` for any endpoint added later.

## Error handling

```js
try {
  await aegis.login(username, password);
} catch (error) {
  if (error.isLicenseError) show("License is not valid for this machine.");
  else if (error.isNetworkError) show("Aegis is unreachable — retrying.");
  else throw error;
}
```

## Security

Only ever ship the public `appKey` in client code. `apiKey` is for trusted
server processes.

## License

MIT — see `LICENSE`.
