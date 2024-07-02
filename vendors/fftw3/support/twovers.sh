#! /bin/sh

# wrapper to generate two codelet versions, with and without
# fma

genfft=$1
shift

echo "#if defined(ARCH_PREFERS_FMA) || defined(ISA_EXTENSION_PREFERS_FMA)"
echo
  $genfft -fma $*
echo
echo "#else"
echo
  $genfft $*
echo
echo "#endif"
