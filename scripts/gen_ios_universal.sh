#!/bin/sh
set -e
luavm=$1
for f in `find builds/ios32/$luavm -name "*.a"`; do
    f64=`echo $f | sed 's/ios32/ios64/'`
    fsim=`echo $f | sed 's/ios32/iossim/'`
    out=`echo $f | sed 's/ios32/ios/'`
    mkdir -p `dirname $out`
    TARGETED_DEVICE_FAMILY=1,2 lipo -create $f $f64 $fsim -output $out
done
for f in `find builds/ios32/$luavm -name "amulet"`; do
    f64=`echo $f | sed 's/ios32/ios64/'`
    fsim=`echo $f | sed 's/ios32/iossim/'`
    out=`echo $f | sed 's/ios32/ios/'`
    mkdir -p `dirname $out`
    TARGETED_DEVICE_FAMILY=1,2 lipo -create $f $f64 $fsim -output $out
done
