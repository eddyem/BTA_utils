# Dome Control System over SSL-sockets

## 1. Overview

The product is a two‑part system for controlling BTA dome driven by a set of AC motors with
**ESQ‑770** frequency converters and a **Modbus I/O board** (relays, discrete inputs, ADC):

* **`dome_server`** — runs on the machine connected to the RS‑485 bus. It talks Modbus RTU to the
frequency converters and the I/O board, and serves an SSL/TLS socket with a simple ASCII line
protocol.
* **`dome_client`** — connects to the server, keeps the dome shared‑memory block (`bta_shdata`) up
to date, drives the motors according to commands from the higher‑level control system, and can also
be used interactively in *terminal mode*.

Both binaries share the same argument parser, daemonization code, and SSL socket layer. The client
and server are mutually exclusive builds of the same sources (`-DCLIENT` / `-DSERVER`).

Key properties:

| Property | Value |
|---|---|
| Transport | TCP + TLS (min. TLS 1.2), mutual authentication |
| Default port | 4444 |
| Protocol | newline‑terminated ASCII, `key` (getter) or `key=value` (setter) |
| Motor bus | Modbus RTU over RS‑485, 10 drives, IDs 1…10 |
| Max SSL clients | 10 (`BACKLOG`) |
| Network timeout | 1 s (configurable) |


## 2. Repository layout

| File | Purpose |
|---|---|
| `main.c` | Entry point: init, parse args, start daemon |
| `cmdlnopts.c/.h` | Command line / config file parsing, global `G` structure |
| `daemon.c/.h` | Daemonization, PID file, signal handling, child supervision |
| `sslsock.c/.h` | SSL context creation, socket open/connect, `read_string()` |
| `client.c/.h` | Client main loop, terminal mode, motor polling, speed control |
| `server.c/.h` | Server poll loop, connection handling, command dispatch, handlers |
| `motors.c/.h` | Modbus motor control, state cache, emulation, logging |
| `io.c/.h` | Modbus I/O board: relays, inputs, ADC, safety interlock logic |
| `esq770.h` | ESQ‑770 register map, command/status codes, unit conversions |
| `handlers_list.h` | X‑macro list of protocol commands (must stay sorted!) |
| `Makefile` | Build rules for `dome_client` / `dome_server` |
| `bta_shdata.c/.h` | shared memory with the dome control subsystem |


## 3. Building

```sh
make            # release build (adds -flto); the default target
make debug      # debug build (-Werror, -DEBUG)
make clean      # remove object dirs and build.target
make xclean     # clean + remove binaries
```

Dependencies:

* [`libusefull_macros`](https://github.com/eddyem/snippets_library) (logging, CLI parsing, config files, time helpers)
* OpenSSL (`libssl`, `libcrypto`)
* [`libmodbus`](https://github.com/stephane/libmodbus) (server only)
* `libcrypt` (client only)

Build details:

* The selected target is stored in `build.target`; if it contains `DEBUG`, a plain `make` also
builds the debug variant. Use `make DEF=...` to append extra defines.
* Binaries are `dome_server` and `dome_client`; objects go to `mkserver/` and `mkclient/`
respectively.
* The debug build passes `-DEBUG`, which GCC interprets as `-D EBUG`, i.e. it defines the `EBUG`
macro used throughout the code for verbose diagnostic printing.


## 4. Architecture

```
 higher-level control (GUI / SHM)          operator (terminal mode)
            |                                       |
       bta_shdata  <--- dome_client ---TLS---> dome_server
                                                   |
                                        Modbus RTU (RS-485)
                                       /                    \
                             ESQ-770 drives 1..10       I/O board
                             (motors)                   (relays/inputs/ADC)
```

* The **client** reads the shared memory block `sdat` (dome speed command, lock flags, telescope
position, server PID, etc.), polls motor state from the server in a round‑robin fashion, writes
averaged state and SEW data back into SHM, and sends status messages to the GUI via
`SendMessage()`.
* The **server** is a poll‑driven event loop: every iteration it advances the motor state machine,
processes the I/O board, and serves any pending SSL traffic.
* Both sides are non‑blocking; the server uses `poll()` with an 1 ms timeout, the client sleeps ~100
ms per cycle.


## 5. SSL layer (`sslsock.c`)

`open_socket()`:

1. Initializes the OpenSSL library and creates a context (`TLS_server_method()` /
`TLS_client_method()`).
2. Requires TLS ≥ 1.2.
3. Loads the CA file (`-a`, default `ca_cert.pem`); on the server side the CA list is also loaded
so the client certificate can be requested.
4. Loads the certificate (`-c`) and private key (`-k`), checks key/cert match.
5. Verification:
  * **Server**: `SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT` — a valid client certificate
  signed by the CA is mandatory.
  * **Client**: `SSL_VERIFY_PEER`.
  * Verification depth is limited to 1 (only certificates signed directly by the CA are accepted).
6. Opens the socket:
  * **Server**: `bind()` to `INADDR_ANY:<port>` with `SO_REUSEADDR`, then `listen(BACKLOG)`.
  * **Client**: resolves `--server` via `gethostbyname()` and `connect()`s.

`read_string(ssl, buf, l)` reads one `\n`‑terminated line:

* Uses `SSL_peek()` to locate the newline without consuming data.
* If the buffer fills up before a newline, the pending data is discarded (`SSL_read` of `l` bytes)
and `0` is returned.
* Returns the number of bytes read (the trailing `\n` is replaced by `\0`), `0` if nothing is
available or the line is incomplete, and `-1` on disconnect/error. `SSL_ERROR_WANT_READ/WRITE` are
treated as “nothing yet”.


## 6. Protocol

Text, one message per line, `\n` terminator.

### 6.1 Message forms

| Form | Meaning | Reply |
|---|---|---|
| `name` | getter | `name=value` |
| `name=value` | setter | `OK`, `BADVAL`, `BADKEY` or `FAIL` |
| `help` | list commands | one `command: description` line per command |
| empty line / comment | ignored | (no reply) |

The client side (`send_motor_command()`) distinguishes getters from setters by the presence of `=`,
and treats the four uppercase tokens above as setter results. In practice only `motnum`,
`motstatus`, `motspeed`, `motcurrent`, `speed`, `stop`, `nmotors`, `forbidden` are used by the
client.

### 6.2 Result codes

| Token | Enum | Meaning |
|---|---|---|
| `OK` | `RESULT_OK` | setter executed |
| `BADVAL` | `RESULT_BADVAL` | malformed or out‑of‑range argument |
| `BADKEY` | `RESULT_BADKEY` | unknown command |
| `FAIL` | `RESULT_FAIL` | command failed (e.g. motors not running) |
| *(no token)* | `RESULT_SILENCE` | internal: getter answer is sent as `key=value` or no answer at all |


## 7. Command reference

All commands are defined once in `handlers_list.h` and expanded into the server’s dispatch table.
**The list must be kept in alphabetical order** — the server looks commands up with a binary search
(`search_handler()`).

| Command | Access | Argument / answer | Description |
|---|---|---|---|
| `adc=N` | get | `N` → `value` | Read ADC channel `N` (0…7) |
| `forbidden` | get/set | `0`/`1` | Setter: `1` forbids dome operations (blocks motors), `0` permits. Getter: state of the `RELAY_BLOCK_MOTORS` relay (`1` = forbidden) |
| `inputs` | get | decimal mask | State of the 16 discrete inputs |
| `logprefix` | get/set | string ≤ 31 chars | Prefix of motor log files (`<prefix>_NN.log`) |
| `motcurrent` | get | amps | Current of the active motor |
| `motnum` | get/set | `0`…`9` | Number of the motor used by status/current/speed getters |
| `motspeed` | get | rev/min | Speed of the active motor |
| `motstatus` | get | `0`…`3` | Status of the active motor (see below) |
| `nmotors` | get | count | Number of motors whose status is not `OFF` |
| `relayon` | get/set | `N` / mask | Setter: turn relay `N` on. Getter: full relay state mask |
| `relayoff` | get/set | `N` / mask | Setter: turn relay `N` off. Getter: full relay state mask |
| `relayset` | get/set | mask | Setter: set/reset all relays at once by bit mask. Getter: state mask |
| `speed` | get/set | rev/min | Setter: speed setpoint, sign selects direction, `0` stops. Getter: current setpoint |
| `startlog` | action | — | Open per‑motor log files |
| `stop` | action | — | Stop all motors |
| `stoplog` | action | — | Close log files |

### Motor status values (`motors.h`)

| Value | Name | Meaning |
|---|---|---|
| 0 | `MOT_OFF` | no answer / disabled |
| 1 | `MOT_SLEEP` | stopped but ready |
| 2 | `MOT_RUN` | rotating |
| 3 | `MOT_ERROR` | drive fault |

`nmotors` counts all motors whose status differs from `MOT_OFF`.

### Relay / input bit masks (`io.h`)

Inputs (16 bits):

| Bit | Macro | Meaning |
|---|---|---|
| 0 | `INP_FLOOR1` | man on floor 1 |
| 1 | `INP_FLOOR2` | man on floor 2 |
| 2 | `INP_DOOR_OPENED` | door is open |
| 3 | `INP_OPEN_DOOR` | “open door” request |
| 4 | `INP_TEL_AT_NEST` | telescope is at the “nest” |

Relays (16 bits, only some are populated):

| Number | Macro | Meaning |
|---|---|---|
| 1 | `RELAY_OPEN_DOOR` | open the door |
| 2 | `RELAY_BLOCK_DOOR` | block the door |
| 3 | `RELAY_LIGHT` | turn on light |
| 4 | `RELAY_POWER` | turn on power socket |
| 5 | `RELAY_BLOCK_MOTORS` | block (inhibit) the motors |
| 10 | `RELAY_DOOR_UNLOCKED` | door is unlocked (turn on light) |


## 8. Server

### 8.1 Startup

`main()` → `sl_init()` → `parse_args()` → `start_daemon()` → `open_socket()` → `serverproc()`.

`start_daemon()` (in `daemon.c`) validates the port (1024…65535) and that certificate/key files can
be opened; optionally opens the log file; installs signal handlers; checks for an already‑running
instance via the PID file; then daemonizes and runs the supervision loop:

* The **master** forks a child, waits for it, logs the child’s lifetime, and respawns after 5 s.
Logging is throttled if children die faster than 30 s.
* The **child** sets `PR_SET_PDEATHSIG = SIGTERM` (it is killed when the master dies), sets
`isrunning = TRUE` and runs the main loop.

Signals: `SIGTERM`, `SIGINT`, `SIGQUIT` set `isrunning = FALSE` and exit (after removing the PID
file); `SIGTSTP` and `SIGHUP` are ignored.

### 8.2 Main loop (`serverproc()`)

1. `motors_process()` — advance the motor state machine (one motor per call plus queued commands).
2. `io_process()` — read inputs/relays/ADC from the I/O board and apply the safety logic.
3. `poll(..., 1 ms)`.
4. On the listening socket: `accept4(..., SOCK_NONBLOCK)`, then `SSL_new()` + timeout‑protected
`SSL_accept()` (bounded by `--timeout`). On failure the client is sent an error and disconnected.
Up to `BACKLOG` (10) clients are served.
5. For each readable client: `handle_connection()` — read one line, parse `key[=value]`, dispatch
to the handler, write the reply. On disconnect the last poll entry is moved into the freed slot.

On exit the server stops all motors and closes the Modbus connection.

### 8.3 Motor control (`motors.c`)

* One Modbus RTU context for the whole process, shared with `io.c` via `io_set_ctx()`.
* **Shared‑memory‑like state** in the server: `motstates[10]`, `n_working_motors`, speed/current
setpoints, and a bit‑flag word (`stop`, `change_speed`, `change_current`), all protected by
`statemutex`.
* **Setter path** (`motors_set_speedsetpoint()`, `motors_set_curntsetpoint()`, `motors_stop()`)
only sets flags; the actual bus traffic happens in `motors_process_m()`:
  * Commands are sent to the **broadcast** slave address 0 (`modbus_set_slave(ctx, 0)`) without
  checking the return code (what's impossible in broadcast mode).
  * `stop` → `CMD_STOP` on `REG_CMD`, setpoint zeroed.
  * Speed change: if `|speed| < SPEED_TOLERANCE` → `CMD_STOP`; otherwise write
  `CMD_FORWARD`/`CMD_REVERSE` to `REG_CMD` and `REVMIN2FREQ(|speed|)` to `REG_FREQ_SET`.
  * Setting a speed requires `n_working_motors ≥ MIN_WORKING_MOTORS` (1); otherwise `RESULT_FAIL`.
  * `|speed| > MAX_SPEED` (655 rev/min) → `RESULT_BADVAL`.
  * Changing the current setpoint is currently a stub (`change_current` is cleared without bus
  traffic).
* **Polling path**: each call selects one slave `MOTOR_ID(curN) = curN+1` and reads
`REG_STATUS_MAIN`, `REG_STATUS_EXT`, `REG_OUTPUT_FREQ`, `REG_OUTPUT_CURRENT`; converts to rev/min
and amperes; maps statuses:
  * `STATUS_CW`, `STATUS_CCW` → `MOT_RUN`
  * `STATUS_STOP` → `MOT_SLEEP`
  * anything else → `MOT_ERROR`
  * `STATUS_READY_MASK` absent in the extended status → forced `MOT_ERROR`; an attempt is made to
  reset the fault with `CMD_RESET_FAULT`.
  * After `MAX_ERRORS` (5) consecutive bus errors the motor is marked `MOT_OFF`.
* `count_motors()` recomputes `n_working_motors` after each full sweep.

### 8.4 Motor logging

`start_log()` opens `<mldir>/<prefix>_NN.log` (append mode) for each motor (default prefix `motor`,
default directory `/tmp`). Each poll writes `time<TAB>speed<TAB>current`, but only while the motor
is `MOT_RUN`. `stop_log()` closes them. Prefixes are validated (`set_logfile_prefix()` rejects `/`,
`%`, control characters and non‑ASCII, and limits length to 31).

### 8.5 Emulation mode

With `--emulation` the server skips the serial port and uses `motors_open_e()` /
`motors_process_e()`:

* All motors start in `MOT_SLEEP`; `n_working_motors = 10`.
* Speed ramps linearly toward the setpoint with `EMUL_ACCEL` (10000 min⁻²); current equals the
current setpoint while accelerating and 0.6 × setpoint when at speed.

### 8.6 I/O board (`io.c`)

* The board is addressed by `G.io_ID`; if it is < 1 or the Modbus context is missing,
`io_process()` does nothing.
* Each pass reads 16 inputs, 16 relay outputs and 8 ADC registers, updates the cached state under a
mutex, then runs the interlock logic.
* **Inputs → relays** (`chk_inputs()`):
  * Any of `FLOOR1`, `FLOOR2`, `DOOR_OPENED` → door unblocked and unlocked, motors blocked, light on.
  * `TEL_AT_NEST` → door unblocked/unlocked, and if `OPEN_DOOR` is asserted the door is opened and
  the motors blocked.
  * Otherwise the door is blocked and locked; if somebody is on the nest without the telescope, the
  open‑door request is honoured.
* **Relays → relays** (`chk_relays()`): motors blocked ⇒ light on, otherwise off. Pending user
requests (`OnFlags`/`OffFlags`) are applied and cleared.
* Public API: `io_read_inputs()`, `io_read_relays()`, `io_read_adc()`, `io_relay_on()`,
`io_relay_off()`, `io_set_relays()`.
* At server start, `io_set_relays(0)` turns everything off and then `RELAY_BLOCK_MOTORS` is
switched on (inhibit by default).


## 9. Client

### 9.1 Connection setup

`clientproc()` / `terminal()` create an SSL connection (`openConn()`), make the socket
non‑blocking, attach the shared memory block (`get_shm_block(&sdat, ClientSide)` — may fail if the
SHM block is not created by the higher‑level system), the command queues (`mcmd`, `ocmd`, `ucmd`),
and publish `DomeSEW_N = MOTORS_AMOUNT`.

### 9.2 Background (daemon) mode — `clientproc()`

Every ~100 ms:

1. `process_system(ssl)`:
  * **SHM watchdog**: if `M_time` lags more than `T_sync_lost` behind wall time, or
  `check_shm_block()` fails, the client tolerates 9 failures and exits on the 10th.
  * **Model mode**: if the SHM reports `UseModel == FullModel`, the client logs a warning once, sends
  an emergency stop, and suspends control until the model mode ends.
  * **Server liveness** (unless `--emulation`): `kill(ServPID, 0)`; two consecutive failures
  terminate the client.
  * **Motor polling**: round‑robin over 10 motors with 4 commands each — `motnum=N`, `motstatus`,
  `motspeed`, `motcurrent`. The active‑motor data feed the `SEWD` variables (`statusSEWD`,
  `vel_SEWD`, `currentSEWD`) for `indexSEWD`.
  * After the full sweep, the **common state** is computed: status = maximum over motors,
  speed/current = mean over `MOT_RUN` motors. Transitions between “all off” and “any running”
  generate `MesgFault`/`MesgInfor` messages to the GUI; an unresponsive motor in error state
  generates `MesgWarn`.
  * `chk_dome_speed()`: translates the SHM `Dome_Speed` state (`D_Off`, `D_Lplus/Lminus`,
  `D_Mplus/Mminus`, `D_Hplus/Hminus`) into ±`LSpeed`/`MSpeed`/`HSpeed` (±71/350/610 rev/min). The
  command is (re)sent every `speedchk_interval` seconds until the measured speed is within
  `SPEED_TOLERANCE` of the setpoint. `D_Locked` forces a stop.
2. Drain and log any unsolicited data from the server.

On exit: `stop_all()` sends `stop` up to 5 times (100 ms apart); if all attempts fail, an error is
logged; otherwise `Dome_Speed` is reset.

### 9.3 Terminal mode — `terminal()`

Enabled by `-T`. Before daemonizing (so it works without detaching), the client offers a raw line
pipe:

* `> ` prompt; each line read from stdin is written to the SSL connection.
* Every reply line is printed as `< reply`.
* `Ctrl‑D` or a write error ends the session.

The shared memory block and automatic motor polling are **not** used in this mode.


## 10. ESQ‑770 frequency converter interface (`esq770.h`)

| Register | Address | Access | Contents |
|---|---|---|---|
| `REG_CMD` | 0x2000 | RW | Command |
| `REG_FREQ_SET` | 0x3000 | RW | Frequency setpoint (0.01 Hz) |
| `REG_TORQUE_LIMIT` | 0x3006 | RW | Torque limit |
| `REG_BRAKE_TORQUE_LIMIT` | 0x3007 | RW | Brake torque limit |
| `REG_STATUS_MAIN` | 0x6000 | R | Main status |
| `REG_STATUS_EXT` | 0x6001 | R | Extended status |
| `REG_OUTPUT_FREQ` | 0xA201 | R | Output frequency × 100 |
| `REG_OUTPUT_CURRENT` | 0xA204 | R | Output current × 10 |
| `REG_MOTOR_SPEED` | 0xA205 | R | Speed (depends on settings) |

Commands (`REG_CMD`): `CMD_FORWARD` = 1, `CMD_REVERSE` = 2, `CMD_STOP` = 5, `CMD_FAST_STOP` = 6,
`CMD_RESET_FAULT` = 7.

Main status (`REG_STATUS_MAIN`): `STATUS_CW` = 1, `STATUS_CCW` = 2, `STATUS_STOP` = 3, `STATUS_ERR`
= 4, `STATUS_LOW_V` = 5. Extended status bit `STATUS_READY_MASK` = 0x01.

Conversions (4‑pole motor, 100× scale):

```c
REVMIN2FREQ(r) = (uint16_t)(100. * r / 15.)   /* rev/min → register */
FREQ2REVMIN(f) = (f * 15.) / 100.             /* register → rev/min */
current[A]     = register / CURRENT_SCALE     /* CURRENT_SCALE = 10 */
```

Because the frequency register is 16‑bit, the maximum representable speed is 655.35 rev/min
(`MAX_SPEED = 655`).


## 11. Command line options

Defined in `cmdlnopts.c`; configuration files (`-C`) use the same names and take precedence over
the command line. `-v`, `-v2`, `-v3` raise the log level to MSG, DBG and ANY respectively (default
WARN).

### Common

| Option | Argument | Default | Description |
|---|---|---|---|
| `-h`, `--help` | — | — | show help |
| `-l`, `--logfile` | file | — | write logs to this file |
| `-P`, `--pidfile` | file | `/tmp/domeserver.pid` / `/tmp/domeclt.pid` | PID file |
| `-c`, `--certificate` | file | `server_cert.pem` / `client_cert.pem` | SSL certificate |
| `-k`, `--key` | file | `server_key.pem` / `client_key.pem` | SSL private key |
| `-p`, `--port` | string | `4444` | TCP port |
| `-v`, `--verbose` | int (optional) | 0 | verbosity level |
| `-a`, `--ca` | file | `ca_cert.pem` | CA certificate |
| `-t`, `--timeout` | double | 1 s | network timeout (accept, command reply) |
| `-C`, `--conffile` | file | — | configuration file |

The certificate and private key files must have absolute path as are opened after daemonisation
and `chroot('/')`.

### Server only

| Option | Argument | Default | Description |
|---|---|---|---|
| `-e`, `--emulation` | — | off | emulation mode (no hardware) |
| `-d`, `--serialdev` | file | — | RS‑485 device path (absolute) |
| `-s`, `--serialspeed` | int | — | serial port speed |
| `-D`, `--mldir` | dir | `/tmp` | directory for motor log files (absolute) |
| `-i`, `--ioid` | int | −1 | Modbus ID of the I/O board (disabled if < 1) |

### Client only

| Option | Argument | Default | Description |
|---|---|---|---|
| `-M`, `--mottmout` | double | 10 s | interval for re‑sending the speed command until it is reached |
| `-s`, `--server` | host | — (required) | server name or IP |
| `-L`, `--lostsync` | double | 5 s | shared‑memory “lost synchronization” timeout |
| `-e`, `--emulation` | — | off | run even if the SHM reports emulation mode |
| `-T`, `--terminal` | — | off | interactive terminal mode |

Note that `-s` has different meanings in the server (`serialspeed`) and client (`server`) builds —
the option tables are compiled conditionally.


## 12. Tuning constants

`motors.h` / `esq770.h`:

| Constant | Value | Meaning |
|---|---|---|
| `MOTORS_AMOUNT` | 10 | number of drives |
| `START_ID` / `MOTOR_ID(n)` | 1 / `n+1` | Modbus slave addresses |
| `MIN_WORKING_MOTORS` | 1 | minimum motors to allow a speed setpoint |
| `MAX_ERRORS` | 5 | consecutive bus errors before a motor is `MOT_OFF` |
| `MAX_SPEED` | 655 | rev/min |
| `SPEED_TOLERANCE` | 0.01 | rev/min |
| `MAX_CURRENT` / `DEFAULT_CURRENT` | 15 / 10 | A |
| `LSpeed` / `MSpeed` / `HSpeed` | 71 / 350 / 610 | rev/min presets |
| `MODBUS_RESPONCE_TIMEOUT` | 100000 | µs |
| `EMUL_ACCEL` | 10000 | min⁻² |
| `BACKLOG` | 10 | max simultaneous SSL clients |
| `IOBUF_LEN` / `SL_VAL_LEN` / `SL_KEY_LEN` | 256 / (external) | protocol buffer sizes |


## 13. Client ↔ shared memory integration

The client depends on the `bta_shdata.[ch]` module that provides:

* `sdat` — the shared `struct` (mode, `atflag`, `Dome_Speed`, `D_Locked`, `DomeSEW_N`, `indexSEWD`,
  `statusSEWD`, `vel_SEWD`, `currentSEWD`, `speedSEWD`, `M_time`, `UseModel`, `ServPID`, …);
* command queues `mcmd`, `ocmd`, `ucmd` (motor / observer / user commands);
* helpers `get_shm_block()`, `check_shm_block()`, `get_cmd_queue()`;
* `SendMessage()` with `MesgWarn`, `MesgFault`, `MesgInfor` message classes for the GUI.

The client writes the shared block (`mode |= 0200`, i.e. write access, `atflag = 0`) and publishes
`DomeSEW_N`. If the block cannot be attached, the client logs the error and aborts.


## 14. Notes and caveats

* **Command order is significant.** `handlers_list.h` must remain alphabetically sorted; the server
uses binary search over the expanded table.
* **The `EBUG` macro** is produced by the Makefile’s `-DEBUG` flag (GCC parses it as `-D EBUG`).
Debug builds therefore enable extra `printf` traces in `io.c`, `motors.c`, etc., and disable
daemonization (`#ifndef EBUG` around `sl_daemonize()`), which is handy for running under a
debugger.
* **Setters vs. getters** are distinguished purely by the presence of `=`. `startlog`, `stop`,
`stoplog` are actions; any `=value` they receive is ignored.
* The relay getters of `relayon`, `relayoff`, `relayset` are identical (they all return the full
relay mask); the setter forms differ.
* The server **broadcasts** motion commands (slave 0) and therefore does not verify their
execution; confirmation happens only indirectly through the client’s periodic speed polling and
re‑sending.
* Motor speed commands are accepted only when at least `MIN_WORKING_MOTORS` drives are alive;
otherwise `RESULT_FAIL` is returned.
* `read_string()` silently drops a line that does not fit into the buffer; keep `IOBUF_LEN` in mind
for custom clients.
* The I/O board’s safety logic runs before the relay state is refreshed, so user relay requests
issued in the same iteration may be overwritten by the interlock rules on the next pass.


## 15. License

This repository is provided under the GNU General Public License v3 or later. Full text of the
GPLv3 is available at https://www.gnu.org/licenses/gpl-3.0.html.
