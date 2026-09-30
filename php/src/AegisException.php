<?php

declare(strict_types=1);

namespace Aegis;

/** Every Aegis API failure surfaces as this exception. */
class AegisException extends \RuntimeException
{
    public function __construct(
        public readonly string $errorCode,
        string $message,
        public readonly int $status = 0,
        ?\Throwable $previous = null,
    ) {
        parent::__construct($message, $status, $previous);
    }

    public function isNetworkError(): bool
    {
        return $this->status === 0;
    }

    public function isAuthError(): bool
    {
        return in_array($this->errorCode, ['unauthorized', 'invalid_credentials'], true);
    }

    public function isLicenseError(): bool
    {
        return str_starts_with($this->errorCode, 'license') || $this->errorCode === 'hwid_mismatch';
    }
}