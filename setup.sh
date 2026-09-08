#!/usr/bin/env bash
set -Eeuo pipefail

source_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_directory="${KMAIL_UNSUBSCRIBE_BUILD_DIR:-${source_directory}/build}"
build_jobs="${KMAIL_UNSUBSCRIBE_BUILD_JOBS:-$(nproc)}"
install_prefix="${KMAIL_UNSUBSCRIBE_INSTALL_PREFIX:-/usr}"

if [[ "${install_prefix}" != /* ]]; then
    echo "KMAIL_UNSUBSCRIBE_INSTALL_PREFIX must be an absolute path." >&2
    exit 1
fi

if [[ ! -r /etc/os-release ]]; then
    echo "Cannot identify this Linux distribution." >&2
    exit 1
fi

# shellcheck source=/dev/null
source /etc/os-release
if [[ "${ID:-}" != "fedora" || "${VERSION_ID:-}" != "44" ]]; then
    echo "This setup script is validated only on Fedora 44." >&2
    echo "Detected: ${PRETTY_NAME:-unknown Linux distribution}" >&2
    exit 1
fi

if ! command -v dnf >/dev/null 2>&1; then
    echo "dnf is required to install the Fedora build dependencies." >&2
    exit 1
fi

run_as_root() {
    if (( EUID == 0 )); then
        "$@"
    else
        sudo "$@"
    fi
}

dependencies=(
    cmake
    extra-cmake-modules
    gcc-c++
    kf6-kcodecs-devel
    kf6-kconfig-devel
    kf6-kcoreaddons-devel
    kf6-kguiaddons-devel
    kf6-ki18n-devel
    kf6-kio-devel
    kf6-kparts-devel
    kf6-syntax-highlighting-devel
    kf6-kxmlgui-devel
    kidentitymanagement-devel
    libkdepim-devel
    mailcommon-devel
    messagelib-devel
    ninja-build
    pimcommon-devel
    kpimtextedit-devel
    qt6-qtbase-devel
)

echo "Installing Fedora 44 build dependencies (sudo may ask for your Linux password)..."
run_as_root dnf install -y "${dependencies[@]}"

echo "Configuring in ${build_directory}..."
cmake \
    -S "${source_directory}" \
    -B "${build_directory}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${install_prefix}" \
    -DBUILD_TESTING=ON

echo "Building and testing..."
cmake --build "${build_directory}" --parallel "${build_jobs}"
ctest --test-dir "${build_directory}" --output-on-failure

echo "Installing the KMail plugins..."
if [[ "${install_prefix}" == "/usr" ]]; then
    run_as_root cmake --install "${build_directory}"
else
    cmake --install "${build_directory}"
fi

plugins=(
    "${install_prefix}/lib64/qt6/plugins/pim6/messageviewer/viewerplugin/kmail_unsubscribe.so"
    "${install_prefix}/lib64/qt6/plugins/pim6/kmail/mainview/kmail_unsubscribe_actionplugin.so"
    "${install_prefix}/lib64/qt6/plugins/pim6/kmail/plugineditorinit/kmail_unsubscribe_editorinitplugin.so"
)
for plugin in "${plugins[@]}"; do
    if [[ ! -f "${plugin}" ]]; then
        echo "Installation failed: missing ${plugin}" >&2
        exit 1
    fi
done

echo
echo "KMail Unsubscribe is installed. Fully quit and reopen Kontact and KMail."
