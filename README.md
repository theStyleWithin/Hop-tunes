# Hop-tunes
An open source version of Microsoft Zune 

## Docs

- [`SPEC.md`](SPEC.md): product and technical spec: a local-first, login-free music player with local profiles.

- [`design/`](design/): mockups of the five core screens (PNG screenshots + clickable HTML pages).

## Developing

Open the repo in a GitHub Codespace (or VS Code Dev Containers). `.devcontainer/` installs Qt 6, GStreamer, TagLib and SQLite, plus a browser-viewable desktop on port 6080 for running the app.

## Research

- [`zune-teardown/`](zune-teardown/): structural teardown of the final Zune 4.8 software: installer format, component architecture, device and service map, and the open-source tools used to produce it. Groundwork for a clean-room implementation.
