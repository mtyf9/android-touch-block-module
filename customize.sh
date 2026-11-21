#!/system/bin/sh
[ -s "$MODPATH/inputs.conf" ] || abort "Prepare the module for this device first."
set_perm "$MODPATH/touch-window" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755
