# Hyprland Backend

WSF has an experimental Hyprland backend for touchpad scroll tuning.

## What WSF Uses

Hyprland already exposes a native touchpad scroll setting:

```bash
hyprctl keyword input:touchpad:scroll_factor 0.35
```

WSF uses that native runtime setting for scroll instead of patching Hyprland.
The preload library is not loaded inside Hyprland by default.

## Current Behavior

- `wsf set <factor>` writes the WSF config and applies
  `input:touchpad:scroll_factor` live when a running Hyprland session is
  detected.
- `wsf set --scroll-vertical <factor>` applies the same native Hyprland setting.
- `wsf set --scroll-horizontal <factor>` also applies the same native Hyprland
  setting because Hyprland does not currently expose separate vertical and
  horizontal touchpad scroll factors.
- `wsf apply` reapplies the saved WSF config to supported live compositor
  backends. On Hyprland it uses `scroll_vertical_factor` as the canonical value
  if vertical and horizontal differ.
- `wsf status` and `wsf doctor` report the current Hyprland
  `input:touchpad:scroll_factor` value.

## Limits

- Hyprland native scroll tuning is one touchpad factor for both axes.
- Hyprland's native touchpad scroll factor range is treated as `0.0-2.0`; WSF
  clamps higher values when applying through `hyprctl`.
- Pinch zoom and pinch rotate sensitivity are not exposed as general native
  Hyprland settings for client applications.
- WSF can test pinch zoom/rotate through a targeted gestures-only preload in
  the `Hyprland` process.
- WSF's scroll preload hooks stay disabled on Hyprland by default to avoid
  double scaling, because scroll is already handled by Hyprland's native
  `scroll_factor`.

## Experimental Pinch Support

To test pinch zoom/rotate, launch Hyprland with WSF loaded only in the
compositor process and only for gesture hooks:

```bash
wsf_lib="${WSF_LIB_PATH:-$HOME/.local/lib/wayland-scroll-factor/libwsf_preload.so}"
if [ -r "$wsf_lib" ]; then
  export WSF_TARGETS="${WSF_TARGETS:-Hyprland}"
  export WSF_HYPRLAND_GESTURES_ONLY="${WSF_HYPRLAND_GESTURES_ONLY:-1}"
  export WSF_DEFER_PRUNE_UNTIL_TARGET="${WSF_DEFER_PRUNE_UNTIL_TARGET:-1}"
  case ":${LD_PRELOAD:-}:" in
    *":$wsf_lib:"*) ;;
    *) export LD_PRELOAD="$wsf_lib${LD_PRELOAD:+:$LD_PRELOAD}" ;;
  esac
fi
exec Hyprland
```

This does not use `/etc/ld.so.preload`; it only affects the launched Hyprland
process. WSF removes itself from `LD_PRELOAD` after loading, so child
applications should not inherit the preload setting.

After restarting Hyprland, check:

```bash
wsf doctor
```

Look for:

```text
hyprland gesture preload: active
runtime gesture reload: active via Hyprland gestures-only preload
```

## Persistence

`hyprctl keyword ...` changes the running Hyprland session. To make WSF the
source of truth, keep any static Hyprland `touchpad.scroll_factor` line
commented out and apply WSF at session start.

Recommended input config pattern:

```ini
touchpad {
    natural_scroll = true

    # Touchpad scroll speed is managed by Wayland Scroll Factor (WSF).
    # Hyprland exposes one native `scroll_factor` shared by vertical and
    # horizontal axes. Keep this commented so reload/login does not overwrite
    # the value chosen in WSF/WSF GUI.
    #
    # scroll_factor = 0.3

    tap-to-click = true
    clickfinger_behavior = true
}
```

Recommended startup config pattern:

```ini
exec-once = sh -lc 'if command -v wsf >/dev/null 2>&1; then wsf apply; elif [ -x "$HOME/.local/bin/wsf" ]; then "$HOME/.local/bin/wsf" apply; fi'
```

For system images or preconfigured desktops, prefer launching `wsf apply` as
part of the compositor/session startup path after Hyprland has exported its
runtime environment.

## Source Audit

Audited on 2026-05-09 against:

- Local Hyprland `0.54.3`
- Upstream Hyprland master commit `11bd00c`
- Upstream Aquamarine commit `813c1e8`

Findings:

- Hyprland applies `input:touchpad:scroll_factor` to pointer axis events coming
  from finger scrolling.
- The same Hyprland factor is used for both vertical and horizontal scroll axes.
- Hyprland does not provide a comparable general-purpose pinch zoom/rotate
  sensitivity setting for client gestures.
- Aquamarine still reads libinput scroll and pinch values through libinput
  getter functions, but WSF intentionally does not activate its preload hooks in
  Hyprland by default.
