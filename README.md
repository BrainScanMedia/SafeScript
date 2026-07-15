# SafeScript

A clean, fast code snippet manager for Linux built with Qt6.

![SafeScript Light Mode](screenshot-light.png)
![SafeScript Dark Mode](screenshot-dark.png)

## Features
- Organize snippets into folders with drag-to-reorder
- Syntax-aware code editor with optional line numbers
- Notes field for each snippet
- Dark and light mode
- Search snippets instantly
- Backup your database to any location, and import one to restore or migrate
- Remembers your layout — window size, column widths, and the code/notes divider
- Reopens the last folder and snippet you were working on
- Keyboard shortcuts for creating and saving snippets
- Data stored locally in SQLite — nothing leaves your device

## Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `Ctrl+N` | New snippet in the selected folder |
| `Ctrl+S` | Save the current snippet |
| `Ctrl+X` / `Ctrl+C` / `Ctrl+V` | Cut / Copy / Paste |
| `Ctrl+A` | Select all |
| `Ctrl+Q` | Quit |

## Installation

<a href='https://flathub.org/en/apps/com.brainscanmedia.SafeScript'><img width='200' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-en.png'/></a>

### Flatpak (recommended)
```bash
flatpak install flathub com.brainscanmedia.SafeScript
```

### Build from source

Requires Qt6 with Widgets and SQL modules.
```bash
git clone https://github.com/BrainScanMedia/SafeScript.git
cd SafeScript
qmake6 SafeScript.pro
make
sudo make install
sudo cp safescript.png /usr/share/icons/hicolor/256x256/apps/safescript.png
sudo gtk-update-icon-cache -f /usr/share/icons/hicolor/
sudo update-desktop-database /usr/local/share/applications/
```

This installs the binary, desktop entry, icon, and metainfo system-wide so SafeScript appears in your app launcher with its icon. Log out and back in if the icon does not appear immediately.

To uninstall:
```bash
sudo rm /usr/local/bin/SafeScript
sudo rm /usr/local/share/applications/safescript.desktop
sudo rm /usr/local/share/icons/hicolor/scalable/apps/safescript.svg
sudo rm /usr/local/share/metainfo/com.brainscanmedia.SafeScript.metainfo.xml
sudo gtk-update-icon-cache -f /usr/share/icons/hicolor/
sudo update-desktop-database /usr/local/share/applications/
rm -rf ~/SafeScript
```
Then log out and back in to complete removal.

## Data Storage

Snippets are saved locally depending on how you installed SafeScript:

**Flatpak:**
`~/.var/app/com.brainscanmedia.SafeScript/data/BrainScanMedia/SafeScript/storage.sqlite3`

**Build from source:**
`~/.local/share/BrainScanMedia/SafeScript/storage.sqlite3`

### Backup and Import

Use **Database → Backup Database** to save a copy of your snippets anywhere on disk, and **Database → Import Database** to load one back in.

Importing replaces your current database entirely, so SafeScript will ask you to confirm first. The selected file is validated before anything is overwritten, and your existing data is restored automatically if the import fails.

This is also the recommended way to move your snippets between the Flatpak and source builds, since each uses its own storage location.

## Preferences

The **Options** menu controls code wrapping, line numbers, and dark mode. Your layout — window size, column widths, and the code/notes divider — is saved automatically when you exit. **Options → Reset Window Size** restores the default layout without touching your snippets.

## Developer
BrainScanMedia.com, Inc.
[https://www.brainscanmedia.com](https://www.brainscanmedia.com)

## License
MIT — © BrainScanMedia.com, Inc.
