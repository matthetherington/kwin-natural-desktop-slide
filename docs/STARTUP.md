# Plasma login startup and recovery

The approved user service is **`natural-swipe-session.service`**, enabled under
`graphical-session.target`. It runs only in the KDE Wayland environment, after
`plasma-kwin_wayland.service`, and waits up to 45 seconds for KWin's effects and
the installed gesture owner to be ready. Source helpers now also support systems
without InputActions; the original deployed snapshots on this machine are unchanged.
There is no persistent test timer or compositor restart.

It uses the same guarded handoff as the live tests: save the original stock/project
renderer, unload it before loading the preview, then load the controller while
InputActions still blocks ownership and unload InputActions. Saved gesture values
are used without a temporary background override. Bindings and `kwinrc` startup
preferences are not rewritten. Failures attempt recovery of both original layers.

In the portable source helper, InputActions is optional: its presence is recorded
in the recovery baseline, and native KWin handling resumes when the controller is
unloaded on systems without it. An installed-but-inactive InputActions plugin is
still refused rather than guessing ownership. Legacy baselines mean InputActions
was active. See [platform limits](COMPATIBILITY.md), particularly the systemd and
KWin version requirements, before deploying this service elsewhere.

The current approved installation pins these installed plugin identities:

- `kwin_natural_slide_preview3`
- `kwin_natural_swipe_controller_preview16`

Do not retire those files while startup uses them. New development builds do not
implicitly retarget the service. The private helper snapshots and `startup.json`
live in `~/.local/libexec/kwin-natural-swipe/`, independent of the source checkout
and CMake build folders. Recovery state lives under
`~/.local/state/kwin-natural-swipe/` (or the configured XDG state directory).

The systemd user unit lives at
`~/.config/systemd/user/natural-swipe-session.service`. Its template and tested
runner are `tools/natural-swipe-session.service.in` and `tools/startup-session.py`.
The runner can safely adopt an already-active pinned pair only with a valid
managed recovery baseline. The approved installation used that path: no live
effects were reloaded, the former timed-test rollback was stopped, and the prior
manual baseline was archived under ignored `local/controller-session/`.

## Check status

```sh
systemctl --user status natural-swipe-session.service
python3 ~/.local/libexec/kwin-natural-swipe/startup-session.py status
journalctl --user -u natural-swipe-session.service -b
```

`active (exited)` is expected: a oneshot handoff remains managed with
`RemainAfterExit=yes`; it does not require a background polling process.

## Restore the original setup

```sh
# Restore the original renderer/InputActions now and disable future login startup:
systemctl --user disable --now natural-swipe-session.service
```

Stopping the service calls its full recovery helper. Saved panel/gesture choices
are retained. To pause only until the next login, use `stop` instead of `disable
--now`; to resume an enabled service, use `start`. Avoid direct/manual effect
switching while the service owns the handoff. In particular, source-tree
`overscroll-rollback` is for timed manual tests, not this installed service's
separate recovery-state file.

If service stopping fails but KWin still responds, the managed helper can retry:

```sh
python3 ~/.local/libexec/kwin-natural-swipe/startup-session.py stop
```

Do not delete recovery state while its preview is active. If logout/crash prevents
normal service stopping, the next login may archive the old state only when the
recorded original renderer and InputActions are actually active again. Mismatched
or malformed state is refused rather than guessed. If pinned plugins are missing,
the handoff times out without modifying the original setup. KWin remains pinned to
6.8.2; updates require ABI review and rebuilding, not silent reuse.

Startup/recovery logic has mocked tests and the live no-reload adoption and unit
enablement were verified. A full reboot/login cycle has not yet been tested.
