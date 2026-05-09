# Wayland Scroll Factor (WSF)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <img src="data/icons/hicolor/512x512/apps/io.github.danielgrasso.WaylandScrollFactor.png"
       alt="WSF icon" width="180" height="180">
</p>

<p align="center">
  <b>Tune touchpad gesture feel on Wayland</b><br>
  User-level touchpad scroll and gesture sensitivity control for modern Wayland desktops.<br>
  <i>Status: testing. GNOME support is the primary target; Hyprland scroll support is experimental.</i>
</p>

Release notes: [`CHANGELOG.md`](CHANGELOG.md)

---

## What WSF Does

WSF (Wayland Scroll Factor) adjusts touchpad gesture sensitivity where desktop environments do not expose enough user-facing controls.

Supported controls:

- Two-finger scroll factor.
- Vertical and horizontal scroll config keys.
- Pinch zoom factor.
- Pinch rotate factor.
- Native Hyprland touchpad scroll factor application.

WSF is intentionally small, user-level, and reversible. It does not use `/etc/ld.so.preload`.

---

## Why It Exists

On Wayland, touchpad behavior is compositor-owned. Depending on compositor, distro, hardware, and libinput version, users may get fast scrolling, inconsistent horizontal scroll, hard-to-control pinch zoom, or no practical UI to tune these behaviors.

WSF provides a pragmatic workaround while upstream desktops mature their own settings.

Design goals:

- Safe by default.
- Per-user configuration.
- Easy rollback.
- No global preload.
- Clear diagnostics.
- Small, auditable code.

---

## Backend Model

WSF uses different backends depending on compositor support.

### GNOME Wayland

GNOME uses the WSF preload backend.

The preload library is installed per-user and injected through:

```text
~/.config/environment.d/wayland-scroll-factor.conf
```

The library is guarded so it only modifies behavior inside `gnome-shell`. Other processes load no active behavior.

GNOME behavior:

- `wsf enable` enables the preload for the next login.
- `wsf disable` removes the preload environment file.
- Login/logout is required when enabling or disabling.
- Once WSF is already loaded in `gnome-shell`, factor changes reload live.
- Scroll scaling is touchpad-oriented and avoids mouse wheel scaling.

### Hyprland

Hyprland uses the native backend.

Hyprland already exposes:

```text
input:touchpad:scroll_factor
```

WSF applies that setting live with:

```bash
hyprctl keyword input:touchpad:scroll_factor <factor>
```

Hyprland behavior:

- `wsf set <factor>` writes the WSF config and applies Hyprland scroll live.
- `wsf apply` reapplies saved WSF config to the running Hyprland session.
- No logout is required for live Hyprland scroll changes.
- Hyprland currently exposes one touchpad scroll factor shared by vertical and horizontal scroll.
- Pinch zoom and pinch rotate are not available as native Hyprland client sensitivity settings.
- WSF does not activate its preload backend inside Hyprland by default, avoiding double scaling.

See [`docs/hyprland.md`](docs/hyprland.md).

---

## Install

### One-Shot Install

This script installs dependencies, clones the repository, and runs the per-user install.

```bash
curl -fsSL https://raw.githubusercontent.com/daniel-g-carrasco/wayland-scroll-factor/main/scripts/bootstrap.sh | bash
```

Notes:

- Uses `sudo` only for dependency installation.
- Installs WSF files under `~/.local`.
- Installs the GUI launcher under `~/.local/share/applications`.
- GUI requires GTK4 and libadwaita 1.4 or newer.
- The CLI can still work where the GUI stack is too old.

### Manual Build

```bash
git clone https://github.com/daniel-g-carrasco/wayland-scroll-factor.git
cd wayland-scroll-factor
meson setup build --prefix="$HOME/.local"
ninja -C build
meson install -C build
```

Or use:

```bash
./scripts/install.sh
```

---

## Quick Start

Set a shared scroll factor:

```bash
wsf set 0.35
```

Check status:

```bash
wsf status
```

Run diagnostics:

```bash
wsf doctor
```

### GNOME Quick Start

```bash
wsf set 0.35
wsf enable
```

Then log out and log back in.

Disable:

```bash
wsf disable
```

Then log out and log back in.

### Hyprland Quick Start

```bash
wsf set 0.35
wsf apply
hyprctl getoption input:touchpad:scroll_factor
```

`wsf set` already applies live when Hyprland is running. `wsf apply` is useful at session startup or after a Hyprland reload.

---

## CLI Commands

```bash
wsf get
wsf get --json
wsf set <factor>
wsf set --scroll-vertical <factor>
wsf set --scroll-horizontal <factor>
wsf set --pinch-zoom <factor>
wsf set --pinch-rotate <factor>
wsf apply
wsf enable
wsf disable
wsf status
wsf status --json
wsf doctor
wsf doctor --json
```

Recommended factor range:

```text
0.05 - 5.0
```

Hyprland native scroll is clamped to its supported native range when applied through `hyprctl`.

---

## GUI

WSF includes a GTK4/libadwaita GUI:

```bash
wsf-gui
```

The GUI calls the `wsf` CLI. It does not duplicate backend logic.

GUI behavior:

- Reads values through `wsf status --json`.
- Applies values through `wsf set`.
- Shows diagnostics from `wsf doctor`.
- On Hyprland, reads the live `input:touchpad:scroll_factor`.
- On Hyprland, keeps vertical and horizontal scroll sliders synchronized because the compositor exposes one shared touchpad scroll factor.

<p align="center">
  <img src="docs/screenshots/gui.png" alt="WSF GUI screenshot" width="860">
</p>

---

## Configuration

WSF config path:

```text
~/.config/wayland-scroll-factor/config
```

Example:

```ini
factor=0.35
scroll_vertical_factor=0.35
scroll_horizontal_factor=0.35
pinch_zoom_factor=1.00
pinch_rotate_factor=1.00
```

Compatibility behavior:

- `factor` is the legacy shared value.
- If per-axis scroll keys are missing, `factor` is used for scroll.
- If per-axis scroll keys exist, they override `factor`.
- Hyprland has one native touchpad scroll factor, so WSF uses one value at runtime there.

Environment overrides:

```bash
WSF_FACTOR=0.35
WSF_SCROLL_VERTICAL_FACTOR=0.35
WSF_SCROLL_HORIZONTAL_FACTOR=0.35
WSF_PINCH_ZOOM_FACTOR=1.00
WSF_PINCH_ROTATE_FACTOR=1.00
WSF_LIB_PATH=/custom/path/libwsf_preload.so
WSF_DEBUG=1
```

---

## Hyprland Persistence

Hyprland runtime settings can be overwritten by static config on reload/login. If WSF should be the source of truth, do not keep an active static touchpad `scroll_factor` in Hyprland config.

Recommended `50-input.conf` pattern:

```ini
input {
    touchpad {
        natural_scroll = true

        # Touchpad scroll speed is managed by Wayland Scroll Factor (WSF).
        #
        # Hyprland exposes one native `scroll_factor` for touchpad scrolling,
        # shared by vertical and horizontal axes. WSF writes that same setting
        # live via:
        #
        #   wsf set 0.3
        #   wsf apply
        #
        # Keep the static Hyprland value commented out so `hyprctl reload` or a
        # new login does not overwrite the value chosen in WSF/WSF GUI.
        #
        # scroll_factor = 0.3

        tap-to-click = true
        clickfinger_behavior = true
    }
}
```

Recommended `20-autostart.conf` pattern:

```ini
# Reapply WSF's saved Hyprland touchpad scroll factor at session startup.
# This keeps scroll speed controlled from WSF/WSF GUI instead of a static
# `input:touchpad:scroll_factor` value in `50-input.conf`.
exec-once = sh -lc 'if command -v wsf >/dev/null 2>&1; then wsf apply; elif [ -x "$HOME/.local/bin/wsf" ]; then "$HOME/.local/bin/wsf" apply; fi'
```

For OS images such as Margine OS, install WSF as part of the base desktop package set and keep `wsf apply` in Hyprland autostart.

---

## Files Installed

Per-user install places files under:

```text
~/.local/bin/wsf
~/.local/bin/wsf-gui
~/.local/lib/wayland-scroll-factor/libwsf_preload.so
~/.local/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop
~/.local/share/icons/hicolor/
~/.local/share/metainfo/
```

Runtime config may create:

```text
~/.config/wayland-scroll-factor/config
~/.config/environment.d/wayland-scroll-factor.conf
```

The `environment.d` file is only needed for GNOME preload activation.

---

## Uninstall

Disable GNOME preload:

```bash
wsf disable
```

Remove user-installed files:

```bash
rm -f ~/.local/bin/wsf ~/.local/bin/wsf-gui
rm -rf ~/.local/lib/wayland-scroll-factor
rm -f ~/.local/share/applications/io.github.danielgrasso.WaylandScrollFactor.desktop
rm -rf ~/.local/share/icons/hicolor/*/apps/io.github.danielgrasso.WaylandScrollFactor.png
rm -f ~/.local/share/metainfo/io.github.danielgrasso.WaylandScrollFactor.metainfo.xml
```

Remove runtime config:

```bash
rm -rf ~/.config/wayland-scroll-factor
rm -f ~/.config/environment.d/wayland-scroll-factor.conf
```

After disabling GNOME preload, log out and log back in.

---

## Compatibility

Known working:

- Arch Linux rolling with GNOME Wayland.
- Arch Linux rolling with Hyprland native scroll backend.
- Ubuntu 24.04 LTS for CLI and GUI.
- Recent Fedora for CLI and GUI.

CLI-only or limited GUI environments:

- Ubuntu 22.04 LTS, because libadwaita is too old for the current GUI.
- Debian 12, because libadwaita is too old for the current GUI.

Requirements:

- C compiler and Meson/Ninja for building from source.
- libinput runtime.
- Python 3 for the GUI launcher script.
- GTK4 and libadwaita for the GUI.
- `hyprctl` for Hyprland native scroll support.

---

## Limitations

- GNOME enable/disable still requires login/logout because the preload must be loaded by `gnome-shell`.
- Hyprland scroll applies live, but persistence requires `wsf apply` at session startup.
- Hyprland exposes one native touchpad scroll factor, not separate vertical/horizontal factors.
- Hyprland does not currently expose a general native pinch zoom/rotate sensitivity setting for client applications.
- WSF is a workaround, not a replacement for eventual upstream compositor settings.

---

## Troubleshooting

Useful commands:

```bash
wsf status
wsf doctor
wsf status --json
wsf doctor --json
```

Hyprland checks:

```bash
hyprctl getoption input:touchpad:scroll_factor
wsf apply
```

GNOME debug logging:

```bash
printf 'WSF_DEBUG=1\n' >> ~/.config/environment.d/wayland-scroll-factor.conf
# log out / log back in
journalctl --user -b -g "wsf:"
```

More details:

- [`docs/install.md`](docs/install.md)
- [`docs/troubleshooting.md`](docs/troubleshooting.md)
- [`docs/hyprland.md`](docs/hyprland.md)
- [`docs/design.md`](docs/design.md)

---

## Contributing

Issues and pull requests are welcome.

Please include:

- Distro.
- Compositor.
- Session type.
- libinput version.
- WSF version or commit.
- Output of `wsf doctor`.
- What you expected.
- What happened instead.

---

## Acknowledgements

WSF was inspired by the idea behind [`libinput-config`](https://github.com/lz42/libinput-config), but uses a narrower and safer architecture:

- No `/etc/ld.so.preload`.
- Per-user install and rollback.
- GNOME process guard.
- Hyprland native backend where available.
- Built-in diagnostics.

---

## License

MIT License. See [`LICENSE`](LICENSE).
