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
./SafeScript
```

## Data Storage
Snippets are saved locally at `~/Documents/SafeScript/storage.sqlite3`.

## License
MIT — © BrainScanMedia.com, Inc.
