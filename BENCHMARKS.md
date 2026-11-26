# Performance benchmarks — Freia Thiwi v0.6.0

This report records server-side measurements for the self-hosted Freia Thiwi project. See the [companion client repository](https://github.com/vanPThomas/FreiaThiwiClient) for the graphical client.

## Hazuki — Dell Inspiron 1545 (2009)

Freia Thiwi is designed to make self-hosted messaging practical on modest hardware. This benchmark records the actual resource use of the full server release, not a reduced test build.

> **154 KiB server executable · 20 active users · 240,000 verified deliveries in 10 minutes · 2.89% average server CPU · 11.38 MiB peak RSS · 14.83 ms p95 peer latency.**

On **26 November 2026 (UTC)**, the unmodified server release `v0.6.0` (commit `c1959c5`) was compiled natively and tested on Hazuki, a 2009 Dell Inspiron 1545. The initial manual test used two external graphical clients; subsequent automated runs used 50 and 20 simulated clients, respectively. All exercised the normal server and SQLite implementation. No compatibility patches or reduced-feature build were used. All resource measurements in this report refer to the server, not the graphical client.

### Test hardware and software

| Component | Configuration |
|---|---|
| System | Dell Inspiron 1545 (2009; 17 years old at test time), BIOS A07 (2009-05-13) |
| CPU | Intel Celeron 900 (Penryn), 2.20 GHz maximum, 1 core / 1 thread, x86-64 |
| CPU cache | 32 KiB L1 instruction + 32 KiB L1 data, 1 MiB L2 |
| Instruction support | No AES-NI or AVX; AES is executed without dedicated AES hardware instructions |
| RAM | 4 GB DDR2-800 (2 × 2 GB); 3.8 GiB usable, 2 GiB swap configured |
| Storage | Hitachi HTS54503-series mechanical SATA HDD, 320 GB nominal (298.1 GiB reported) |
| OS / kernel | Ubuntu 24.04.5 LTS; Linux `6.8.0-139-generic` (manual test), `6.8.0-151-generic` (automated tests) |
| Build tools | GCC/G++ 13.3.0; CMake 3.28.3; Release build, `-j1` |
| Libraries | OpenSSL 3.0.13; SQLite 3.45.1 |
| Server executable | 154 KiB (rounded `ls -lh` output); 64-bit ELF PIE, dynamically linked, not stripped |

The machine had about 538 MiB of system memory in use before server startup, 3.3 GiB available, and no swap in use. The executable size does not include dynamically linked libraries or the database.

### Server memory and CPU measurements

| Workload | RSS | PSS | Private memory (USS) | CPU usage |
|---|---:|---:|---:|---|
| Idle, no clients | 9,240 KiB (9.02 MiB) | 3,079 KiB (3.01 MiB) | 1,412 KiB (1.38 MiB) | 0.0% in 10 `top` samples |
| Two authenticated clients | 9,708 KiB (9.48 MiB) | — | — | 0.0% lifetime average (`ps`) |
| One room, two clients, after exchanging messages | 9,712 KiB (9.48 MiB) | 3,264 KiB (3.19 MiB) | 1,564 KiB (1.53 MiB) | 0.1% lifetime average (`ps`) |
| 20-second manual busy-traffic test, two clients | 9,600 KiB (9.38 MiB) | — | — | **0.10% average; 1.00% highest one-second sample** (`pidstat`) |

The first three stages reported one server thread. Process swap was zero in both `smaps_rollup` snapshots. Between the idle and post-messaging snapshots, RSS increased by only 472 KiB, PSS by 185 KiB and private memory by 152 KiB. RSS includes shared library pages; PSS apportions shared pages across processes, and USS here is the sum of `Private_Clean` and `Private_Dirty`. A dash indicates a metric not measured at that stage.

### 20-second manual messaging test

Two authenticated clients exchanged repeated encrypted messages in the same room while the server process was sampled once per second (`pidstat -u -r -d -p <PID> 1 20`). The server processes transport encryption, room-specific delivery and SQLite writes of encrypted room-message bodies; room-content encryption and decryption take place on the clients.

| Metric | Observed result |
|---|---:|
| Average CPU (userspace + kernel) | **0.10%** (0.05% + 0.05%) |
| Highest one-second CPU sample | 1.00% |
| CPU samples reported as 0.00% | 18 of 20 |
| CPU scheduling wait | 0.00% average |
| RSS (minimum / maximum / average) | 9,600 / 9,600 / 9,600 KiB |
| Observed RSS growth | 0 KiB |
| Virtual memory (VSZ) | 13,832 KiB throughout |
| Minor / major page faults | 0.10 / 0.00 per second on average |
| Average process-attributed disk writes | 17.99 KiB/s |
| Highest one-second disk-write sample | 48 KiB/s |
| Approximate process-attributed writes | 360 KiB over the test |
| Process-attributed disk reads | 0 KiB/s |
| Reported process I/O delay | 0 (as reported by `pidstat`) |

### Automated stress tests — 26 November 2026

A private, headless C++ stress tester (`FreiaThiwiStressTest v0.1.0`) ran on a separate LAN workstation (Marilyn), using independent TCP connections, the real encrypted protocol, synthetic accounts and password-protected room membership. It verified decrypted message contents, delivery counts and room membership. The server ran on Hazuki with Linux `6.8.0-151-generic` for these runs. The tool is internal, not a public release or part of the server/client dependencies.

Both workloads used a single room, 256-byte message bodies and one submitted message per second per simulated user. Each submission is delivered to every room member, including its sender.

| Metric | Sustained benchmark | Heavy broadcast benchmark |
|---|---:|---:|
| Concurrent clients | **20** | **50** |
| Active send phase | 600 s | 60 s |
| Submitted messages | 12,000/12,000 | 3,000/3,000 |
| Verified peer deliveries | 228,000/228,000 | 147,000/147,000 |
| Verified sender echoes | 12,000/12,000 | 3,000/3,000 |
| Total verified deliveries | **240,000** | **150,000** |
| Achieved submitted messages/s | 20 | 50 |
| Requested outgoing deliveries/s (including echoes) | 400 | 2,500 |
| Average server CPU during send phase | **2.89%** | 15.45% |
| Highest sampled server CPU during send phase | 4.00% | 29.00% |
| Peak sampled RSS | **11,648 KiB (11.38 MiB)** | 11,264 KiB (11.00 MiB) |
| Mean peer latency | 10.01 ms | 697.08 ms |
| Median peer latency | 9.39 ms | 13.21 ms |
| p95 peer latency | **14.83 ms** | 3,672.27 ms |
| p99 peer latency | 33.89 ms | 4,756.26 ms |
| Maximum peer latency | 114.63 ms | 5,042.82 ms |
| Final delivery drain | 0.30 s | 1.98 s |
| Setup time | 14.428 s | 22.343 s |
| Completed reconnect/login rounds per client | 1 | 0 |
| Missing, duplicate, invalid or wrong-room deliveries | 0 | 0 |
| Protocol errors, unexpected disconnects or failed sends | 0 | 0 |

Both runs passed the tester's delivery-integrity checks. The 20-user run additionally completed a reconnect/login round for all 20 clients before messaging (40 TCP handshakes and room create/connect operations in total). The 50-user run exposed multi-second tail latency and transient backlogs; its delivery-integrity pass is **not** a low-latency capacity claim.

#### Sustained 20-user benchmark: resource details

| Measurement | Result |
|---|---:|
| Setup time (including reconnect/login round) | 14.428 s |
| Highest sampled CPU during setup | 91% |
| CPU during send phase (userspace / kernel) | 1.16% / 1.73% average |
| CPU during send phase (minimum / maximum) | 2% / 4% |
| RSS during send phase (minimum / average / maximum) | 11,264 / 11,546 / 11,648 KiB |
| RSS growth during send phase | 384 KiB |
| RSS plateau | 11,648 KiB for the final 430 one-second samples (~7 min 10 s) |
| Average / peak process-attributed writes | 370.51 / 604 KiB/s |
| Approximate process-attributed writes | 217 MiB over 600 s |
| Major page faults / process-attributed disk reads | 0 / 0 KiB/s |
| Reported process I/O delay | 0 in all traffic samples |
| System swap usage / swap-in / swap-out | 0 throughout recorded `vmstat` samples |
| System-wide I/O wait (`vmstat`, entire recorded window) | 14.61% average; 36% highest sample |
| Maximum outstanding delivery copies at traffic sample points | 40 |
| Sender echo latency (p95 / maximum) | 52.54 / 135.63 ms |

#### Setup and authentication timing

These are end-to-end times measured by the tester. Setup operations were sequential, not a simultaneous login burst.

| Operation | 20-user mean / p95 | 50-user mean / p95 |
|---|---:|---:|
| TCP connection and handshake | 2.34 / 3.70 ms | 1.27 / 1.37 ms |
| Account creation | 49.57 / 54.70 ms | 81.72 / 88.82 ms |
| Account login | 41.72 / 42.32 ms | Not tested |
| Room creation or connection | 282.87 / 289.11 ms | 304.84 / 303.06 ms |

#### Heavy 50-user benchmark: resource details

| Measurement | Result |
|---|---:|
| Highest sampled CPU during setup | 86% |
| CPU during send phase (userspace / kernel) | 5.15% / 10.30% average |
| Average / peak process-attributed writes | 893.17 / 1,628 KiB/s |
| Major page faults / process-attributed disk reads | 0 / 0 KiB/s |
| Reported process I/O delay | 0 in all traffic samples |

The 50-user run built a backlog during the first eight seconds, caught up by approximately 16 seconds, and experienced another backlog near the end. All expected deliveries were verified after the drain period. Blocking socket operations, synchronous database transactions, network behavior and load-generator scheduling are possible contributors; the measurements do not establish a root cause.

### Measurement methodology and limitations

The 20-user run followed the 50-user run on the same recorded server PID and started at 11 MiB RSS, so it is a warm-process measurement rather than an independent fresh-start baseline. CPU/RSS and process I/O figures for each send phase are calculated from the 600 or 60 one-second `pidstat` samples ending after the recorded traffic start through the traffic end (setup and drain excluded). The timestamps appear aligned; clock synchronization was not independently verified. System-wide `vmstat` was recorded without timestamps, so its I/O-wait figures cover the entire recording (excluding the initial since-boot report), not an exactly aligned traffic-only interval. Process-attributed writes are not necessarily physical disk writes or final database size.

The server process remained observable in `pidstat` for eight seconds after the 20-user send phase, with the final samples at 0% CPU and unchanged RSS. Subsequent process-memory snapshots were unavailable because the server was no longer running; the manner of shutdown was not recorded. No process PSS/USS measurement is claimed for the automated tests.

Latency is measured end-to-end by the load generator, not solely within the server. CPU maxima are sampled interval averages, not instantaneous peaks. The results characterize these specific LAN workloads; they are not a security audit, maximum supported user count, guarantee of long-term memory stability or a benchmark of the graphical client's resource consumption.

### Reproducing the workload

Build the server from the release tag on Linux, using its normal dependencies:

```bash
git checkout v0.6.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1
server="$PWD/bin/freia-thiwi"
mkdir -p "$HOME/freia-benchmark-v060"
cd "$HOME/freia-benchmark-v060"
"$server"
```

With one server instance running, use a separate terminal on the server host:

```bash
pid=$(pgrep -x freia-thiwi)
ps -p "$pid" -o pid,pcpu,pmem,rss,vsz,nlwp,etime
cat "/proc/$pid/smaps_rollup"
top -b -d 1 -n 10 -p "$pid"
pidstat -u -r -d -p "$pid" 1 20
```

`pidstat` is provided by the `sysstat` package. Use a dedicated test database and test accounts. Record the idle baseline first, then connect two clients from another machine, authenticate both, create/join a room, and exchange messages during the 20-second sampling window.

For the automated runs, the internal tester used `--clients 20 --rooms 1 --rate 1 --duration 600 --message-size 256 --reconnect-rounds 1` and `--clients 50 --rooms 1 --rate 1 --duration 60 --message-size 256`, respectively. Monitor the server using `pidstat -u -r -d -p "$pid" 1` and `vmstat -w 1` while generating the workload from a separate machine. For future measurements, use synchronized clocks and timestamped `vmstat` output (e.g. `vmstat -w -t 1`). Keep synthetic records in a disposable database. The internal tool is not distributed with either public repository; the parameters document the workload rather than provide a publicly runnable automated test. A compatible load generator can reproduce the traffic pattern.

### Interpretation and next steps

The sustained 20-user run met the proposed targets for delivery integrity, CPU, RSS and peer latency. Across both automated runs, 15,000 submissions resulted in 390,000 verified deliveries (including sender echoes), with no missing, duplicate or invalid deliveries.

The manual two-client workload did not record message counts, exact sizes, traffic rate or delivery latency. The automated workloads provide controlled message rates and verified deliveries, but the 50-user result shows that tail latency can become unacceptable even with complete delivery. Further testing should investigate the source of intermittent delays and include longer runs, multiple rooms, cold-start comparisons and concurrent authentication under traffic.

Freia Thiwi targets small, independently hosted groups, rather than a city-scale centralized service. These results demonstrate practical performance on the tested 2009 laptop. They do not establish universal minimum requirements. A 32-bit machine from around 2000 (Celeron-chan, 651 MHz, 384 MB RAM) remains an experimental compatibility target; it has **not yet** run Freia Thiwi.

## Other hardware targets

| System | Status |
|---|---|
| Yuriko — Dell OptiPlex 780 | Planned; no benchmark results yet |
| Celeron-chan — 651 MHz Celeron, 384 MB RAM | Experimental 32-bit compatibility target; no benchmark results yet |

Future hardware and release results can be added here without expanding the main README.
