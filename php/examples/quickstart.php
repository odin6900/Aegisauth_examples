<?php

/**
 * Runnable quickstart for the Aegis PHP SDK.
 *
 *   AEGIS_APP_KEY=... php examples/quickstart.php
 */

declare(strict_types=1);

require __DIR__ . '/../src/AegisException.php';
require __DIR__ . '/../src/Aegis.php';

use Aegis\Aegis;
use Aegis\AegisException;

$aegis = new Aegis(
    getenv('AEGIS_BASE_URL') ?: 'http://localhost:8080',
    getenv('AEGIS_APP_KEY') ?: '',
    version: '1.0.0',
);

try {
    $info = $aegis->init();
    echo "initialized: {$info['status']}\n";

    if (!empty($info['version']['update_required'])) {
        echo "mandatory update: {$info['version']['latest']}\n";
        exit(0);
    }

    $auth = $aegis->login(getenv('AEGIS_USERNAME') ?: 'demo', getenv('AEGIS_PASSWORD') ?: 'demo-password');
    echo "signed in as {$auth['user']['username']}\n";

    // Multi-subscription check example:
    if ($aegis->hasSubscription('VIP')) {
        echo "[Subscription] VIP tier active — Premium unlocked!\n";
    } else {
        echo "[Subscription] Standard tier active.\n";
    }

    $aegis->setVariable('last_seen', date(DATE_ATOM));
    print_r($aegis->getVariables('user')['variables'] ?? []);

    print_r($aegis->downloads());

    $aegis->heartbeat();
    $aegis->logout();
    echo "signed out.\n";
} catch (AegisException $error) {
    fwrite(STDERR, "Aegis error [{$error->errorCode}] {$error->getMessage()}\n");
    exit(1);
}