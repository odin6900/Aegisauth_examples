# Aegis SDK for Go

Official client for the Aegis Authentication API. Standard library only,
Go 1.21+, context-aware and safe for concurrent use.

## Contents

```
client.go     Client with every API operation
errors.go     *Error with typed error codes
hwid.go       Stable hardware id
types.go      Request/response types
examples/     Runnable quickstart
```

## Install

No registry needed — unzip and reference the folder from your module:

```
// go.mod
require github.com/aegis/aegis-sdk-go v1.0.0
replace github.com/aegis/aegis-sdk-go => ./aegis-sdk-go
```

## Quickstart

```go
client, err := aegis.New(aegis.Options{
    BaseURL: "https://your-aegis-host",
    AppKey:  os.Getenv("AEGIS_APP_KEY"),
    Version: "1.0.0",
})
if err != nil { log.Fatal(err) }

ctx := context.Background()
info, err := client.Init(ctx)
if err != nil { log.Fatal(err) }
if info.Version != nil && info.Version.UpdateRequired {
    log.Fatalf("update to %s", info.Version.Latest)
}

auth, err := client.Login(ctx, "ada", password)
if err != nil { log.Fatal(err) }
log.Println("signed in as", auth.User.Username)

check, _ := client.ValidateLicense(ctx, "AEGS-4K7P-2M9X-QT31")
log.Println(check.Valid, check.Status)

_ = client.SetVariable(ctx, "last_level", "12", "user", "")
vars, _ := client.GetVariables(ctx, "user", "")
log.Println(vars.Variables)

client.StartHeartbeat(ctx, time.Minute, func(reason string) { app.Lock(reason) })
defer client.StopHeartbeat()
_ = client.Logout(ctx)
```

## Supported operations

`Init`, `Status`, `AppData`, `Register`, `Login`, `Logout`, `Heartbeat`,
`CheckSession`, `IsAuthenticated`, `UseSession`, `UserData`,
`ValidateLicense`, `ActivateLicense`, `GetVariables`, `SetVariable`,
`CheckVersion`, `TriggerWebhook`, plus `Request` for any endpoint added later.

## Error handling

```go
var aerr *aegis.Error
if errors.As(err, &aerr) {
    switch {
    case aerr.Code == "hwid_mismatch":
        ui.Show("This license is locked to another machine.")
    case aerr.IsNetworkError():
        ui.Show("Aegis is unreachable — retrying.")
    }
}
```

## License

MIT — see `LICENSE`.
