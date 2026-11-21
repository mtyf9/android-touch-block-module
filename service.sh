#!/system/bin/sh
cd "${0%/*}" || exit 1
until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 2; done
{
    IFS= read -r panel && IFS= read -r power &&
    IFS= read -r up && IFS= read -r down
} < inputs.conf || exit 1
while :; do
    ./touch-window "$panel" "$power" "$up" "$down"
    sleep 3
done
