#!/bin/bash
card_num=$(grep -m 1 "TFA9912" /proc/asound/cards | awk '{print $1}')
[ -z "$card_num" ] && echo "No hay bocina" && exit 1
vol=$(amixer -c $card_num get Softmaster | grep -oP '\[\d{1,3}%\]' | head -1 | tr -d '[]%')
new=$((vol + 10)); [ $new -gt 100 ] && new=100
amixer -c $card_num sset Softmaster ${new}% >/dev/null
echo "Volumen: ${vol}% -> ${new}%"
