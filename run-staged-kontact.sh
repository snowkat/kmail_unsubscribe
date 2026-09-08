#!/usr/bin/env bash
set -euo pipefail

if (( $# != 1 )); then
    printf 'Usage: %s STAGE_DIRECTORY\n' "$0" >&2
    exit 64
fi

stage_directory=$(realpath -- "$1")
plugin_directory="$stage_directory/lib64/qt6/plugins"
for plugin in \
    pim6/messageviewer/viewerplugin/kmail_unsubscribe.so \
    pim6/kmail/mainview/kmail_unsubscribe_actionplugin.so \
    pim6/kmail/plugineditorinit/kmail_unsubscribe_editorinitplugin.so; do
    if [[ ! -f "$plugin_directory/$plugin" ]]; then
        printf 'Missing staged plugin: %s\n' "$plugin_directory/$plugin" >&2
        exit 1
    fi
done

if pgrep -u "$(id -u)" -x kontact >/dev/null || pgrep -u "$(id -u)" -x kmail >/dev/null; then
    printf 'Kontact or KMail is still running. Use File > Quit (Ctrl+Q) in both before launching this build.\n' >&2
    printf 'QT_PLUGIN_PATH cannot change the plugins in an already-running application.\n' >&2
    exit 1
fi

launch_log="$stage_directory/kontact.log"
printf 'Plugin directory: %s\nStartup log: %s\n' "$plugin_directory" "$launch_log"
QT_PLUGIN_PATH="$plugin_directory" QT_FORCE_STDERR_LOGGING=1 kontact 2>&1 | tee "$launch_log"
