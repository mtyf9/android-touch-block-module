#!/system/bin/sh
cd "${0%/*}" || exit 1
until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 2; done
while :; do
    ./touch-window
    sleep 3
done
