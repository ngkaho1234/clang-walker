find_path(LibClang_INCLUDE_DIR clang-c/Index.h
    HINTS ${LIBCLANG_INCLUDE_PATH}
    PATHS
    # The way I install it...
    $ENV{HOME}/local/include
    # LLVM Debian/Ubuntu nightly packages: http://llvm.org/apt/
    /usr/lib/llvm-3.1/include
    /usr/lib/llvm-3.2/include
    /usr/lib/llvm-3.3/include
    /usr/lib/llvm-3.4/include
    /usr/lib/llvm-3.5/include
    /usr/lib/llvm-3.6/include
    # LLVM MacPorts
    /opt/local/libexec/llvm-3.1/include
    /opt/local/libexec/llvm-3.2/include
    /opt/local/libexec/llvm-3.3/include
    /opt/local/libexec/llvm-3.4/include
    /opt/local/libexec/llvm-3.5/include
    /opt/local/libexec/llvm-3.6/include
    # LLVM Homebrew
    /usr/local/Cellar/llvm/3.1/include
    /usr/local/Cellar/llvm/3.2/include
    /usr/local/Cellar/llvm/3.3/include
    /usr/local/Cellar/llvm/3.4/include
    /usr/local/Cellar/llvm/3.5/include
    /usr/local/Cellar/llvm/3.6/include
    )

find_library(LibClang_LIBRARY NAMES clang
    HINTS ${LIBCLANG_LIBRARY_PATH}
    PATHS
    # The way I install it...
    $ENV{HOME}/local/lib
    # LLVM Debian/Ubuntu nightly packages: http://llvm.org/apt/
    /usr/lib/llvm-3.1/lib/
    /usr/lib/llvm-3.2/lib/
    /usr/lib/llvm-3.3/lib/
    /usr/lib/llvm-3.4/lib/
    /usr/lib/llvm-3.5/lib/
    /usr/lib/llvm-3.6/lib/
    # LLVM MacPorts
    /opt/local/libexec/llvm-3.1/lib
    /opt/local/libexec/llvm-3.2/lib
    /opt/local/libexec/llvm-3.3/lib
    /opt/local/libexec/llvm-3.4/lib
    /opt/local/libexec/llvm-3.5/lib
    /opt/local/libexec/llvm-3.6/lib
    # LLVM Homebrew
    /usr/local/Cellar/llvm/3.1/lib
    /usr/local/Cellar/llvm/3.2/lib
    /usr/local/Cellar/llvm/3.3/lib
    /usr/local/Cellar/llvm/3.4/lib
    /usr/local/Cellar/llvm/3.5/lib
    /usr/local/Cellar/llvm/3.6/lib
    # LLVM Fedora
    /usr/lib/llvm

    PATH_SUFFIXES llvm
    )

set(LibClang_LIBRARIES ${LibClang_LIBRARY})
set(LibClang_INCLUDE_DIRS ${LibClang_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibClang DEFAULT_MSG LibClang_LIBRARY LibClang_INCLUDE_DIR)

mark_as_advanced(LibClang_INCLUDE_DIR LibClang_LIBRARY)
