# Aegis SDK for PHP

Official client for the Aegis Authentication API. PHP 8.1+, `ext-curl` and
`ext-json` only — no third-party packages.

## Contents

```
src/Aegis.php           Client with every API operation
src/AegisException.php  Typed error codes
composer.json           PSR-4 autoloading
```

## Install

No registry needed — unzip and autoload the folder:

```json
{
  "repositories": [{ "type": "path", "url": "./aegis-sdk-php" }],
  "require": { "aegis/sdk": "*" }
}
```

Or simply `require` the two files directly.

## Quickstart

```php
use Aegis\Aegis;
use Aegis\AegisException;

$aegis = new Aegis('https://your-aegis-host', $appKey, version: '1.0.0');

$info = $aegis->init();
$auth = $aegis->login('ada', $password);
echo "signed in as {$auth['user']['username']}\n";

$check = $aegis->validateLicense('AEGS-4K7P-2M9X-QT31');
$aegis->setVariable('last_level', '12');
print_r($aegis->getVariables('user')['variables']);

$aegis->logout();
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`checkSession`, `isAuthenticated`, `useSession`, `userData`,
`validateLicense`, `activateLicense`, `getVariables`, `setVariable`,
`checkVersion`, `triggerWebhook`, plus `request()` for any endpoint added later.

## Error handling

```php
try {
    $aegis->login($username, $password);
} catch (AegisException $error) {
    if ($error->errorCode === 'hwid_mismatch') { /* locked to another machine */ }
    elseif ($error->isNetworkError()) { /* retry */ }
    else { throw $error; }
}
```

## License

MIT — see `LICENSE`.
