# ntrip-caster-pub

Standalone, single-mountpoint **NTRIP v2.0 caster** designed to run on a public VPS.
Acts as a relay: one base station POSTs RTCM3 corrections to the configured
mountpoint, and every connected rover client receives the stream over plain TCP
or TLS.

Extracted and stripped down from the [GnssHat](../README.md) library — **no
hardware HAT dependencies**. Builds and runs anywhere with a C++20 compiler,
`pthreads`, and (optionally) OpenSSL.

## Features

- NTRIP v2.0 caster — `GET /mount` for rovers, `POST /mount` for the source
- HTTP Basic auth (optional)
- Sourcetable advertisement with configurable lat/lon
- Optional TLS (server cert + key, PEM)
- Live stats (bytes, frames, last-frame age, uptime, clients)
- Graceful shutdown on `SIGINT` / `SIGTERM`
- Single static binary, no runtime config files required

## Build

```bash
cd ntrip-caster-pub
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

CMake options:

| Option | Default | Description |
|---|---|---|
| `NTRIP_CASTER_TLS` | `OFF` | Link against OpenSSL and enable TLS |
| `NTRIP_CASTER_STATIC` | `OFF` | Produce a fully static binary |
| `NTRIP_CASTER_TESTS` | `OFF` | Build smoke tests |

Install:

```bash
sudo cmake --install build --prefix /usr/local
```

## Usage

```text
ntrip-caster [options]

  --host <addr>         Bind address           (default 0.0.0.0)
  --port <n>            Listen port            (default 2101)
  --mount <name>        Mountpoint name        (default CASTER)
  --max-clients <n>     Max concurrent clients (default 64)
  --user <name>         Basic-auth username    (default: open access)
  --pass <pwd>          Basic-auth password
  --lat <deg>           Advertised latitude    (default 0.0)
  --lon <deg>           Advertised longitude   (default 0.0)
  --tls-cert <path>     PEM certificate (enables TLS)
  --tls-key  <path>     PEM private key
  --log-level <lvl>     error|warning|info|debug (default info)
  --stats-interval <s>  Print stats every N seconds (0=off, default 30)
```

### Example

Plain TCP, open access:

```bash
ntrip-caster --port 2101 --mount BASE1 --lat 50.0647 --lon 19.9450
```

With auth + TLS:

```bash
ntrip-caster \
    --port 2102 --mount BASE1 \
    --user rover --pass s3cret \
    --tls-cert /etc/letsencrypt/live/example.com/fullchain.pem \
    --tls-key  /etc/letsencrypt/live/example.com/privkey.pem
```

### Pushing data from a base

Any tool that speaks the **NTRIP source protocol** (e.g. `str2str` from
RTKLIB, an `ntripserver`, or the `gnsshat-rtk-base` tool from GnssHat) can
feed this caster:

```bash
str2str -in serial://ttyACM0:115200 \
        -out ntrips://user:pass@your-vps:2101/BASE1
```

### Connecting a rover

Any standard NTRIP client works:

```bash
str2str -in ntrip://rover:s3cret@your-vps:2101/BASE1 \
        -out tcpsvr://:5555
```

## systemd

A unit file is shipped under `systemd/ntrip-caster.service` and (when present)
will be installed under `${prefix}/lib/systemd/system`. Example deployment:

```bash
sudo cp build/ntrip-caster /usr/local/bin/
sudo cp systemd/ntrip-caster.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now ntrip-caster
```

## Project layout

```
ntrip-caster-pub/
├── CMakeLists.txt
├── README.md
├── app/
│   └── ntrip-caster.cpp     # CLI entry point
├── src/
│   ├── Base64.hpp           # base64 decoder for Basic auth
│   ├── NtripCaster.hpp/.cpp # caster core
│   ├── NtripLog.hpp         # log mixin
│   ├── NtripStats.hpp       # stats mixin
│   └── NtripTls.hpp         # OpenSSL wrapper (optional)
└── systemd/
    └── ntrip-caster.service
```

## License

MIT — same as the parent GnssHat project.
