# Trackle Library Diagnostics

Diagnostics are numeric health metrics that the device keeps locally and sends to Trackle Cloud as health checks.

They cover three areas:

| Area | What it describes | Who usually fills it |
|------|-------------------|----------------------|
| **System** | Uptime, memory, battery, reset reason, … | Application / platform wrapper |
| **Network** | RSSI, IP, Wi‑Fi/cellular status, disconnects, … | Application / platform wrapper |
| **Cloud** | Connection attempts, disconnects, CoAP latency, rate limits, … | Trackle library (mostly automatic) |

Setting a diagnostic only updates local state. The cloud receives values when a health check is published or when the cloud requests it.

---

## How publishing works

Diagnostics are sent in these cases:

1. **On cloud connect / resume (cloud-requested)**  
   After a successful handshake, Trackle Cloud automatically asks the device for diagnostics. The device replies with the current metrics.  
   This does **not** depend on `setPublishHealthCheckInterval()`: it happens whenever the device connects and the cloud requests them.

2. **Periodically (device-initiated)**  
   If you set an interval with `setPublishHealthCheckInterval(N)` where `N > 0`, the device also posts diagnostics every N milliseconds while connected.  
   Default is `0` → no periodic device-initiated publish.  
   On connect the library resets the periodic timer, so the next interval-based send is counted from the connection time.

3. **On demand**  
   Call `publishHealthCheck()` / `tracklePublishHealthCheck()` to push the current map immediately.

4. **On protocol error**  
   If a new protocol error occurs while connected, the library may force a diagnostics publish (best effort; a disconnect may happen first).

Typical sequence:

```text
connect / resume
  → handshake (system/application describe)
  → cloud requests diagnostics
  → device replies with current metrics

while connected, if interval > 0
  → device posts diagnostics every N ms
```

The handshake describe (functions, variables, system info) is separate from diagnostics.

---

## How to use it

### 1. Keep values up to date

Call the diagnostic setters whenever local information changes (boot, Wi‑Fi events, timers, …).

```cpp
// C++
trackle.diagnosticSystem(SYSTEM_FREE_MEMORY, free_heap);
trackle.diagnosticSystem(SYSTEM_UPTIME, uptime_seconds);
trackle.diagnosticNetwork(NETWORK_RSSI, rssi);

// C
trackleDiagnosticSystem(trackle, SYSTEM_FREE_MEMORY, free_heap);
trackleDiagnosticNetwork(trackle, NETWORK_RSSI, rssi);
```

Because the cloud asks for diagnostics right after connect, keep important System/Network values reasonably up to date beforehand (or continuously refreshed by the platform wrapper).

Cloud keys such as disconnect count, connection attempts, CoAP round-trip and rate-limited publishes are updated by the library; you normally do not set them yourself.

### 2. Optional periodic upload

```cpp
trackle.setPublishHealthCheckInterval(60 * 60 * 1000); // every hour while connected

// C
trackleSetPublishHealthCheckInterval(trackle, 60 * 60 * 1000);
```

Use `0` to disable periodic device-initiated publishes. Cloud-requested diagnostics on connect still work.

### 3. Call `loop()` regularly

`loop()` / `trackleLoop()` drives connection, reconnection, cloud requests handling, and periodic health checks.

### 4. Force a publish when needed

```cpp
trackle.publishHealthCheck();

// C
tracklePublishHealthCheck(trackle);
```

Useful after updating several keys at once, or when you want an immediate cloud view without waiting for the interval or the next connect.

---

## Available keys

Keys are defined in `defines.h` as the `System`, `Network`, and `Cloud` enums. Each key has a short cloud name in the comments next to the enum.

### System (`diagnosticSystem`)

| Key | Cloud name | Typical meaning |
|-----|------------|-----------------|
| `SYSTEM_LAST_RESET_REASON` | `sys:reset` | Last reset / reboot reason |
| `SYSTEM_FREE_MEMORY` | `mem:free` | Free heap / RAM |
| `SYSTEM_BATTERY_CHARGE` | `batt:soc` | Battery state of charge |
| `SYSTEM_SYSTEM_LOOPS` | `sys:loops` | System loop counter |
| `SYSTEM_APPLICATION_LOOPS` | `app:loops` | Application loop counter |
| `SYSTEM_UPTIME` | `sys:uptime` | Uptime (seconds) |
| `SYSTEM_BATTERY_STATE` | `batt:state` | Battery state |
| `SYSTEM_POWER_SOURCE` | `pwr::src` | Power source |
| `SYSTEM_TOTAL_RAM` | `sys:tram` | Total RAM |
| `SYSTEM_USED_RAM` | `sys:uram` | Used RAM |

### Network (`diagnosticNetwork`)

| Key | Cloud name | Typical meaning |
|-----|------------|-----------------|
| `NETWORK_CONNECTION_STATUS` | `net:stat` | Network / link status |
| `NETWORK_CONNECTION_ERROR_CODE` | `net:err` | Last network error |
| `NETWORK_DISCONNECTS` | `net:dconn` | Network disconnect count |
| `NETWORK_CONNECTION_ATTEMPTS` | `net:connatt` | Network connect attempts |
| `NETWORK_DISCONNECTION_REASON` | `net:dconnrsn` | Why the network dropped |
| `NETWORK_IPV4_ADDRESS` | `net:ip:addr` | IPv4 address |
| `NETWORK_IPV4_GATEWAY` | `net.ip:gw` | IPv4 gateway |
| `NETWORK_FLAGS` | `net:flags` | Network flags |
| `NETWORK_COUNTRY_CODE` | `net:cntry` | Country / operator code |
| `NETWORK_RSSI` | `net:rssi` | Signal RSSI |
| `NETWORK_SIGNAL_STRENGTH` | `net:sigstr` | Signal strength (%) |
| `NETWORK_SIGNAL_STRENGTH_VALUE` | `net:sigstrv` | Signal strength (raw) |
| `NETWORK_SIGNAL_QUALITY` | `net:sigqual` | Signal quality (%) |
| `NETWORK_SIGNAL_QUALITY_VALUE` | `net:sigqualv` | Signal quality (raw) |
| `NETWORK_ACCESS_TECNHOLOGY` | `net:at` | Access technology (Wi‑Fi, LTE, …) |
| `NETWORK_CELLULAR_*` | `net:cell:cgi:*` | Cellular CGI fields (MCC/MNC/LAC/CI) |
| `NETWORK_MAC_ADDRESS_OUI` | `net:mac:oui` | MAC OUI |
| `NETWORK_MAC_ADDRESS_NIC` | `net:mac:nic` | MAC NIC |

### Cloud (`diagnosticCloud`) — mostly filled by the library

| Key | Cloud name | How it is set |
|-----|------------|---------------|
| `CLOUD_CONNECTION_STATUS` | `cloud:stat` | Cloud connection status — **currently not set by the library** |
| `CLOUD_DISCONNECTS` | `cloud:dconn` | +1 on each real cloud disconnect |
| `CLOUD_DISCONNECTION_REASON` | `cloud:dconnrsn` | Last disconnect reason |
| `CLOUD_CONNECTION_ATTEMPTS` | `cloud:connatt` | +1 per connect attempt; reset after disconnect |
| `CLOUD_UNACKNOWLEDGED_MESSAGES` | `coap:unack` | +1 when a confirmable CoAP request times out |
| `CLOUD_REPEATED_MESSAGES` | `coap:resend` | CoAP retransmit count — **currently not set by the library** |
| `CLOUD_PROTOCOL_ERROR_CODE` | `cloud:err` | Last protocol error code |
| `CLOUD_COAP_ROUND_TRIP` | `coap:roundtrip` | CoAP ACK round-trip time (ms) |
| `CLOUD_RATE_LIMITED_EVENTS` | `pub:throttle` | +1 when a publish is rate-limited |

### Value semantics worth knowing

- **Counters** (`CLOUD_DISCONNECTS`, `CLOUD_CONNECTION_ATTEMPTS`, `CLOUD_UNACKNOWLEDGED_MESSAGES`, `CLOUD_RATE_LIMITED_EVENTS`, `NETWORK_DISCONNECTS`, `NETWORK_CONNECTION_ATTEMPTS`): pass a positive value to increment, `0` to reset.
- **Fractional values** such as battery charge, RSSI and signal strength/quality are accepted as `double`; the library stores them in a fixed-point form suitable for the cloud.
- Only keys that have been set at least once are included in a health check.

### Cloud disconnect reasons

Stored in `CLOUD_DISCONNECTION_REASON`:

| Value | Name | Meaning |
|------:|------|---------|
| 0 | `CLOUD_DISCONNECT_REASON_NONE` | Not set |
| 1 | `CLOUD_DISCONNECT_REASON_SOCKET` | Socket failure |
| 2 | `CLOUD_DISCONNECT_REASON_SEND` | Send failure |
| 3 | `CLOUD_DISCONNECT_REASON_RECEIVE` | Receive failure |
| 4 | `CLOUD_DISCONNECT_REASON_USER` | User called `disconnect()` |
| 5 | `CLOUD_DISCONNECT_REASON_PROTOCOL` | Protocol / handshake failure |
| 6 | `CLOUD_DISCONNECT_REASON_AUTH` | Authentication failure |
| 7 | `CLOUD_DISCONNECT_REASON_TIMEOUT` | Timeout |

Detailed protocol errors go to `CLOUD_PROTOCOL_ERROR_CODE` (`Cloud_Protocol_Error` in `defines.h`).

---

## Minimal example

```cpp
// Fill platform metrics (often done in a wrapper on boot / Wi-Fi events / a timer)
trackle.diagnosticSystem(SYSTEM_TOTAL_RAM, total_ram);
trackle.diagnosticSystem(SYSTEM_FREE_MEMORY, free_heap);
trackle.diagnosticNetwork(NETWORK_ACCESS_TECNHOLOGY, NET_ACCESS_TECHNOLOGY_WIFI);

// Optional: also send diagnostics every hour while connected
trackle.setPublishHealthCheckInterval(60 * 60 * 1000);

// Must be called regularly
trackle.loop();

// Optional: push now
// trackle.publishHealthCheck();
```

With this setup you get:

- a diagnostics snapshot after each successful cloud connect/resume (requested by the cloud)  
- hourly updates while connected (device-initiated, because the interval is `> 0`)  
- cloud-side counters (disconnects, attempts, round-trip, …) maintained by the library
