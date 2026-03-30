# SafeScript

A clean, fast code snippet manager for Linux built with Qt6.

![SafeScript Dark Mode](screenshot-dark.png)
![SafeScript Light Mode](screenshot-light.png)

## Features
- Organize snippets into folders
- Syntax-aware code editor with line numbers
- Notes field for each snippet
- Dark and light mode
- Search snippets instantly
- Data stored locally in SQLite

## Installation

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
sudo rm /usr/share/icons/hicolor/256x256/apps/safescript.png
sudo rm /usr/local/share/metainfo/com.brainscanmedia.SafeScript.metainfo.xml
sudo gtk-update-icon-cache -f /usr/share/icons/hicolor/
sudo update-desktop-database /usr/local/share/applications/
rm -rf ~/SafeScript
```

Then log out and back in to complete removal.
Then log out and back in to complete removal.

## Data Storage
Snippets are saved locally at `~/.local/share/BrainScanMedia/SafeScript/storage.sqlite3`.

## License
MIT — © BrainScanMedia.com, Inc.
