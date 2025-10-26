#!/bin/bash
card_num=$(grep -m 1 "SPH0645" /proc/asound/cards | awk '{print $1}')


./audio_capture mensaje.wav plughw:$card_num