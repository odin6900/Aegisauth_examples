// Runnable quickstart for the Aegis Go SDK.
//
//	AEGIS_BASE_URL=https://your-aegis-host AEGIS_APP_KEY=pk_live_... go run ./examples/quickstart
package main

import (
	"context"
	"errors"
	"log"
	"os"
	"time"

	aegis "github.com/aegis/aegis-sdk-go"
)

func main() {
	baseURL := os.Getenv("AEGIS_BASE_URL")
	if baseURL == "" {
		baseURL = "http://localhost:8080"
	}

	client, err := aegis.New(aegis.Options{
		BaseURL: baseURL,
		AppKey:  os.Getenv("AEGIS_APP_KEY"),
		Version: "1.0.0",
	})
	if err != nil {
		log.Fatal(err)
	}

	ctx := context.Background()

	info, err := client.Init(ctx)
	if err != nil {
		fail(err)
	}
	log.Println("initialized:", info.Status)
	if info.Version != nil && info.Version.UpdateRequired {
		log.Println("mandatory update:", info.Version.Latest, info.Version.DownloadURL)
		return
	}

	auth, err := client.Login(ctx, envOr("AEGIS_USERNAME", "demo"), envOr("AEGIS_PASSWORD", "demo-password"))
	if err != nil {
		fail(err)
	}
	log.Println("signed in as", auth.User.Username)

	vars, err := client.GetVariables(ctx, "user", "")
	if err == nil {
		log.Println("user variables:", vars.Variables)
	}
	_ = client.SetVariable(ctx, "last_seen", time.Now().UTC().Format(time.RFC3339), "user", "")

	client.StartHeartbeat(ctx, time.Minute, func(reason string) {
		log.Println("session revoked:", reason)
	})
	time.Sleep(2 * time.Second)
	client.StopHeartbeat()

	if err := client.Logout(ctx); err != nil {
		fail(err)
	}
	log.Println("signed out.")
}

func envOr(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

func fail(err error) {
	var aerr *aegis.Error
	if errors.As(err, &aerr) {
		log.Fatalf("aegis error [%s] %s", aerr.Code, aerr.Message)
	}
	log.Fatal(err)
}