package aegis

import (
	"crypto/sha256"
	"encoding/hex"
	"net"
	"os"
	"runtime"
	"strings"
	"sync"
)

var (
	hwidOnce  sync.Once
	hwidValue string
)

// HardwareID returns a stable, non-reversible machine identifier.
func HardwareID() string {
	hwidOnce.Do(func() {
		host, _ := os.Hostname()
		parts := []string{host, runtime.GOOS, runtime.GOARCH, os.Getenv("USER") + os.Getenv("USERNAME"), macAddress()}
		sum := sha256.Sum256([]byte(strings.Join(parts, "|")))
		hwidValue = hex.EncodeToString(sum[:])
	})
	return hwidValue
}

func macAddress() string {
	ifaces, err := net.Interfaces()
	if err != nil {
		return ""
	}
	for _, iface := range ifaces {
		if iface.Flags&net.FlagLoopback != 0 || len(iface.HardwareAddr) == 0 {
			continue
		}
		return iface.HardwareAddr.String()
	}
	return ""
}