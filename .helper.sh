# Build & test helper used by the makefiles. You should not need to
# change this file; put local overrides (compiler, flags) into
# ‹local.mk› in the top-level directory of the bundle instead.

log() { echo "$@" ; "$@" ; }

announce()
{
    printf "%-16s " $(basename "$1" .cpp)
}

cxx()
{
    src=$1 ; shift
    xflags="$(sed -ne 's,^// cxxflags: ,,p' < $src)"

    if ! log $CXX $CXXFLAGS $src "$@" $xflags > .build.out 2>&1; then
        cat .build.out
        echo 'build failed' ; return 1
    fi

    return 0
}

grind()
{
    if test -z "$VALGRIND" || ! command -v $VALGRIND > /dev/null; then
        if ! "$@"; then echo 'tests failed' ; return 1 ; fi
        echo 'ok (without valgrind)' ; return 0
    fi

    if ! $VALGRIND -q --error-exitcode=1 --leak-check=full \
                   --log-file=.valgrind.out "$@"; then
        cat .valgrind.out
        echo 'tests failed' ; return 1
    fi

    echo 'ok' ; return 0
}

action=$1 ; shift
$action "$@"
