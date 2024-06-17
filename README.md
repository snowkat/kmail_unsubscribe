# One-Click Unsubscribe for KMail

This is an initial attempt to provide support for [RFC 8058](https://www.rfc-editor.org/rfc/rfc8058.html) One-Click Unsubscribe to KMail.

This is still a work in progress, but appears to work. Sadly, One-Click Unsubscribe is difficult to meaningfully test.

## Requirements

This was built using Qt6 and the following KDE Frameworks:

- extra-cmake-modules
- KConfig
- KCoreAddons
- KGuiAddons
- KXmlGui
- KParts
- KIO

Additionally, the following KDE PIM libraries are required:

- Libkdepim
- PimCommon
- Messagelib

## Building

This plugin is built using CMake.

```
$ cmake -Bbuild .
$ cmake --build build
$ cmake --install build
```

## Usage

Once installed, the Unsubscribe button can be added to the KMail Toolbar by right clicking it and selecting "Configure Toolbars...".
