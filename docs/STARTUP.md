# Startup and recovery

<!-- Intended audience: users enabling login startup or troubleshooting activation. -->

Most users should follow the package instructions below. The final section is only
for existing source-installed services.

## Release packages

Run these commands in Konsole as your normal desktop user, **not** with `sudo`.
Save your work before turning on the effect.

| What you want | Command |
| --- | --- |
| Try it for this session | `natural-desktop-slide-session start` |
| Undo a session-only trial | `natural-desktop-slide-session stop` |
| Turn it on now and at future logins | `natural-desktop-slide-session enable` |
| Turn it off and disable future startup | `natural-desktop-slide-session disable` |
| Check its state | `natural-desktop-slide-session status` |

Login startup needs a systemd user session. Session-only `start`/`stop` also work
without systemd. Do not mix the two modes: stop a manual trial before enabling
login startup. If login startup is enabled, use `disable` to turn it off completely.

The helper restores the recorded desktop animation and gesture handler. KDE's
original Slide effect stays installed. InputActions is optional and its bindings
are never edited. Never enable both desktop-switching effects yourself.

## If startup fails

For login startup, check the service and its log:

```sh
systemctl --user status natural-desktop-slide-session.service
journalctl --user -u natural-desktop-slide-session.service -b
```

`active (exited)` is normal: the service hands control to KWin and then finishes.
If activation reports a version mismatch, install a matching package rather than
forcing it. If an updated plugin still behaves like the old one, save your work
and log out/in before trying it again.

Recovery records normally live in
`~/.local/state/kwin-natural-desktop-slide/` (or under `XDG_STATE_HOME` if set).
**Do not delete a live recovery record** to bypass an error: it records what
should be restored. Conflicting or malformed state is rejected rather than guessed.

Helpers cannot recover a crashed or unresponsive KWin. If that happens, the desktop
session may already be lost. After returning to a usable session, disable login
startup before trying again. Report the error and system versions in the
[issue tracker](https://github.com/matthetherington/kwin-natural-desktop-slide/issues),
but check logs for private information before sharing them.

## Existing source-installed login services

This section is for advanced users who previously installed a private startup
helper, not users who installed a release package.

The source-installed service pins plugin identities and keeps private helper
snapshots, independent of the checkout. It waits for a known renderer and gesture
owner under the systemd Plasma Wayland graphical session.

If login starts with no desktop renderer loaded, the helper can activate the pinned
renderer/controller directly, but only with a valid recorded recovery baseline
and a known gesture owner. It never loads stock Slide as an intermediate step.
Without a baseline, or with competing renderers/controllers, it refuses to guess.
On managed stop/logout it saves `startup-baseline.json` beside the live recovery
record before restoring the original effects. This separate baseline survives
clean shutdown so the next login can resume even when stock Slide is disabled in
KWin's saved settings. It does not enable startup by itself; the service must
remain enabled. The live `renderer-session.json` is still consumed on recovery.

| Item | Default location |
| --- | --- |
| Settings | `~/.config/kwin-natural-desktop-slide/{panels,gestures}.json` |
| Private helpers/config | `~/.local/libexec/kwin-natural-desktop-slide/` |
| Recovery | `~/.local/state/kwin-natural-desktop-slide/renderer-session.json` |
| User unit | `~/.config/systemd/user/natural-desktop-slide-session.service` |

Pinned identities are recorded in the installed `startup.json`; they are not
necessarily the source-build defaults. Do not overwrite or retire pinned plugins
while that setup is in use. The template is
`tools/natural-desktop-slide-session.service.in`.

```sh
python3 ~/.local/libexec/kwin-natural-desktop-slide/startup-session.py status
# Restore the baseline and disable future startup:
systemctl --user disable --now natural-desktop-slide-session.service
```

To pause only until next login, use `systemctl --user stop` with the service name;
use `start` to resume. If stopping fails but KWin responds, retry the private
helper with `stop`. Repository backups and build directories are not required
for this service's recovery.

Do not use the source-tree manual test helper to recover this managed service:
it has a separate baseline. For source-tree tests, see
[development recovery](DEVELOPMENT.md#recovery).

### Migrate a source service to a release package

1. Stop and disable the source-installed service using its existing recovery
   command above. Confirm that the previous renderer and gesture owner are restored.
2. Archive/move `~/.config/systemd/user/natural-desktop-slide-session.service`
   outside systemd's unit directories. This user unit would otherwise override
   the package's service.
3. Run `systemctl --user daemon-reload`.
4. Save your work and log out/in to clear previously loaded plugin identities.
5. Follow [package activation](BINARY_PACKAGES.md#configure-and-opt-in).

Do not delete a live recovery baseline. Existing private helpers can remain until
migration is confirmed. Installed package updates never restart KWin for you.
