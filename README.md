# Commander Markers (Nexus)

Nexus addon port of the [BlishHUD Commander Markers](https://github.com/manlaan/BlishHud-CommanderMarkers) module.

Place squad markers with a click, or auto-place saved marker sets near triggers. Includes a community marker library synced from the Blish static host.

## Features

- **Clickable markers panel** - ground markers (select icon, click world/minimap) and object markers (instant placement)
- **AutoMarker** - saved marker sets with map triggers, interact placement, and in-game world markers when the map is closed
- **Dedicated settings window** - full UI in an owned ImGui window (opened from Nexus addon config or the corner icon); Nexus config page shows a launcher only
- **Marker set editor** - separate editor window for local library sets: per-slot placement, Base64 import/export, optional RTAPI squad-marker import, unsaved-close and delete confirmations (hold **Shift** to skip)
- **Community library** - browse, search, and import marker sets from shared JSON; **Only Show available** hides sets already in your library; import shows a Nexus alert on success
- **Local library** - search filter, enable/disable sets, create/edit/delete, Base64 import/export, nuclear reset (Ctrl+Shift)
- **Real-Time API (optional)** - when [Real-Time API](https://raidcore.gg/gw2/nexus) is installed and active, squad commander/lieutenant roles augment mumble commander checks; editor can import live squad marker positions
- **Corner icon** - quick access to settings, library, and lieutenant mode toggle
- **Quick access keybind** - `CM_INTERACT` (default `F`) for AutoMarker interact placement

Squad marker placement uses **Guild Wars 2 in-game keybinds** (via Nexus `GameBinds_*`). This addon does not provide a keybind configuration UI - assign squad marker binds in GW2 Controls. If required binds are missing, a warning is shown and placement is blocked.

## Requirements

- [Nexus](https://raidcore.gg/gw2/nexus) in Guild Wars 2
- Windows x64 at runtime (DLL is cross-compiled from Linux)
- GW2 squad marker keybinds assigned in Controls (ground and object markers 1–8, clear)

## Installation

1. Copy `CommanderMarkers.dll` to `<GW2>/addons/` (directly in `addons/`, not a subfolder)
2. Launch GW2 with Nexus enabled
3. Enable **Commander Markers** in Nexus addon settings
4. Click **Open Commander Markers Settings** on the addon page (or use the corner icon) to configure options and libraries
5. Assign squad marker keybinds in GW2 if you have not already

## Build & deploy

### Toolchain (Fedora/Nobara)

```bash
sudo dnf install mingw64-gcc-c++ mingw64-winpthreads-static cmake ninja git python3
```

### Build

```bash
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/CommanderMarkers.dll`

Stripped release export (~5 MB):

```bash
./scripts/build-release.sh
```

Output: `dist/CommanderMarkers.dll`

### Deploy to local GW2

```bash
./scripts/deploy-to-gw2.sh
./scripts/deploy-to-gw2.sh --release    # stripped dist/ export from build-release.sh
```

Set `GW2_ADDONS_DIR` to override the default addons path.

### Nexus does not list the DLL

Rebuild with the provided `CMakeLists.txt` (static MinGW runtime). The DLL should only import Windows system libraries:

```bash
x86_64-w64-mingw32-objdump -p dist/CommanderMarkers.dll | rg "DLL Name"
```

Expect `KERNEL32.dll`, `USER32.dll`, `WINHTTP.dll`, `msvcrt.dll` - **not**
`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, or `libwinpthread-1.dll`.

Also confirm:

- Filename is `CommanderMarkers.dll`
- File is x64 and sits directly in `addons/`

## Manual test checklist

- [ ] Addon loads without Nexus errors
- [ ] Settings window opens from Nexus addon config and corner icon; all six tabs render (About shows RTAPI status)
- [ ] Clickable panel: ground select+click, object instant, commander/LtMode gates
- [ ] AutoMarker settings: Map Icons / In Game World Icons sections match expected toggles
- [ ] AutoMarker: near trigger → interact key → full set placed with delays
- [ ] Map closed: 2m trigger bubble + in-game world markers visible when enabled in settings
- [ ] Corner icon: configured left-click actions work
- [ ] Local library: search filter, enable/disable (Active/Disabled), open editor, save validation
- [ ] Marker set editor: unsaved-close prompt, delete confirm (Shift skips both), Base64 import/export
- [ ] RTAPI (if installed): About tab shows active; editor **Import** / **Import active squad markers** work in squad
- [ ] Community library: search, **Only Show available**, import disabled for duplicates, Nexus alert on import
- [ ] Community sync: second launch only checks `lastEdit` (~1 KB if unchanged); Ctrl+Shift redownload works
- [ ] Windowed mode: marker placement not offset (Win32 client origin fix)
- [ ] Unbound squad marker binds: warning shown; placement blocked until GW2 binds assigned

## Static data

Community marker sets are fetched from:

`https://bhm.blishhud.com/Manlaan.CommanderMarkers/Community/Markers.json`

Some UI icons may be loaded from `https://assets.gw2dat.com/`. Both hosts receive an identifiable `User-Agent: COMM-MARKERS-Nexus/<version>`.

To add or update community marker sets, see **[CONTRIBUTING.md](CONTRIBUTING.md)**.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build notes, project layout, and the community JSON PR workflow.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## Credits

- **Soeed** - BlishHUD Commander Markers, and Nexus port
- **vaon** - Simplified Chinese localization
- **Manlaan** - original clickable markers in BlishHUD
- **Freesnöw** - static data hosting (`bhm.blishhud.com`, `assets.gw2dat.com`)
- **Raidcore** - Nexus addon platform

## Links

- [BlishHUD Commander Markers](https://github.com/manlaan/BlishHud-CommanderMarkers)
- [Community static branch](https://github.com/manlaan/BlishHud-CommanderMarkers/tree/bhud-static/Manlaan.CommanderMarkers)
- [Patch notes](https://pkgs.blishhud.com/Manlaan.CommanderMarkers.html)
- [Nexus](https://raidcore.gg/gw2/nexus)

## License

MIT - see [LICENSE](LICENSE). Nexus API headers: Raidcore.GG MIT license.
