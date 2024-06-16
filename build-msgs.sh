#!/usr/bin/env bash

APPNAME="kmail_unsubscribe"

DEFAULT_XGETTEXT="$(command -v xgettext)"
XGETTEXT_PROGRAM="${XGETTEXT:-${DEFAULT_XGETTEXT}}"
if [ -z "$XGETTEXT_PROGRAM" ] ; then
    echo "error: Couldn't find xgettext. Set \$XGETTEXT to its path, or make sure you have gettext installed." >&2
    exit 1
fi

# Pulled from https://invent.kde.org/sysadmin/l10n-scripty/-/blob/master/extract-messages.sh
do_xgettext() {
    $XGETTEXT_PROGRAM --copyright-holder="snow flurry" \
    --package-name=$APPNAME \
    --msgid-bugs-address=https://git.2ki.xyz/snow/kmail_unsubscribe \
    --from-code=UTF-8 \
    -C --kde \
    -ci18n \
    -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
    -ki18nd:2 -ki18ndc:2c,3 -ki18ndp:2,3 -ki18ndcp:2c,3,4 \
    -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
    -kki18nd:2 -kki18ndc:2c,3 -kki18ndp:2,3 -kki18ndcp:2c,3,4 \
    -kxi18n:1 -kxi18nc:1c,2 -kxi18np:1,2 -kxi18ncp:1c,2,3 \
    -kxi18nd:2 -kxi18ndc:2c,3 -kxi18ndp:2,3 -kxi18ndcp:2c,3,4 \
    -kkxi18n:1 -kkxi18nc:1c,2 -kkxi18np:1,2 -kkxi18ncp:1c,2,3 \
    -kkxi18nd:2 -kkxi18ndc:2c,3 -kkxi18ndp:2,3 -kkxi18ndcp:2c,3,4 \
    -kkli18n:1 -kkli18nc:1c,2 -kkli18np:1,2 -kkli18ncp:1c,2,3 \
    -kklxi18n:1 -kklxi18nc:1c,2 -kklxi18np:1,2 -kklxi18ncp:1c,2,3 \
    -kI18N_NOOP:1 -kI18NC_NOOP:1c,2 \
    -kI18N_NOOP2:1c,2 -kI18N_NOOP2_NOSTRIP:1c,2 \
    -ktr2i18n:1 -ktr2xi18n:1 \
     "$@"

}

SRC_ROOT="$(dirname "${BASH_SOURCE[0]}")"

do_xgettext `find "${SRC_ROOT}" \( -name \*.cpp -o -name \*.h -o -name \*.qml \)` -o "${SRC_ROOT}/po/${APPNAME}.pot"