#!/usr/bin/env bash

set -euo pipefail

qt_version="$(qmake -query QT_VERSION)"

qt_module_root() {
    local module="$1"
    local pri_file="$2"
    local match

    match="$(compgen -G "/nix/store/*-${module}-${qt_version}-dev/mkspecs/modules/${pri_file}" | head -n 1 || true)"
    if [[ -z "$match" ]]; then
        echo "Unable to locate Qt ${module} development metadata for Qt ${qt_version}." >&2
        exit 1
    fi

    dirname "$(dirname "$(dirname "$match")")"
}

qtbase_dev="$(qt_module_root qtbase qt_lib_core.pri)"
qtdeclarative_dev="$(qt_module_root qtdeclarative qt_lib_qml.pri)"
qtcharts_dev="$(qt_module_root qtcharts qt_lib_charts.pri)"

export QMAKEPATH="${qtbase_dev}:${qtdeclarative_dev}:${qtcharts_dev}${QMAKEPATH:+:${QMAKEPATH}}"

qt_module_library_dir() {
    local metadata_file="$1"
    local module_key="$2"

    awk -F ' = ' -v key="QT.${module_key}.libs" '$1 == key { print $2; exit }' "$metadata_file"
}

qtbase_libs="$(qt_module_library_dir "${qtbase_dev}/mkspecs/modules/qt_lib_core.pri" core)"
qtdeclarative_libs="$(qt_module_library_dir "${qtdeclarative_dev}/mkspecs/modules/qt_lib_qml.pri" qml)"
qtcharts_libs="$(qt_module_library_dir "${qtcharts_dev}/mkspecs/modules/qt_lib_charts.pri" charts)"

qt_gui_library="$(qmake -query QT_INSTALL_LIBS)/libQt6Gui.so"
gl_dispatch_library="$(ldd "$qt_gui_library" | awk '/libGLdispatch\.so/{print $3; exit}')"
gl_runtime_library="$(ldd "$qt_gui_library" | awk '/libOpenGL\.so/{print $3; exit}')"

if [[ -z "$gl_dispatch_library" || -z "$gl_runtime_library" ]]; then
    echo "Unable to locate the OpenGL libraries required by Qt." >&2
    exit 1
fi

gl_version="$(basename "$(dirname "$(dirname "$gl_dispatch_library")")" | sed -E 's/^[^-]+-libglvnd-//')"
gl_header="$(compgen -G "/nix/store/*-libglvnd-${gl_version}-dev/include/GL/gl.h" | head -n 1 || true)"

if [[ -z "$gl_header" ]]; then
    echo "Unable to locate OpenGL development headers for libglvnd ${gl_version}." >&2
    exit 1
fi

export CPATH="$(dirname "$(dirname "$gl_header")")${CPATH:+:${CPATH}}"
export LIBRARY_PATH="${qtbase_libs}:${qtdeclarative_libs}:${qtcharts_libs}:$(dirname "$gl_runtime_library")${LIBRARY_PATH:+:${LIBRARY_PATH}}"

# An interrupted parallel compile can leave empty object files with fresh
# timestamps. Remove only those invalid outputs before resuming the build.
find . -maxdepth 1 -type f -name '*.o' -size 0 -delete

qmake porymap.pro
make -j"${JOBS:-$(nproc)}"

matcher_test_build_dir="build/tests/studio"
mkdir -p "$matcher_test_build_dir"
qmake tests/studio/imagemetatilematcher_test.pro -o "$matcher_test_build_dir/Makefile"
make -C "$matcher_test_build_dir" -j"${JOBS:-$(nproc)}"
"$matcher_test_build_dir/imagemetatilematcher_test"

porytiles_test_build_dir="build/tests/porytiles"
mkdir -p "$porytiles_test_build_dir"
qmake tests/studio/porytilesprocess_test.pro -o "$porytiles_test_build_dir/Makefile"
make -C "$porytiles_test_build_dir" -j"${JOBS:-$(nproc)}"
"$porytiles_test_build_dir/porytilesprocess_test"