#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#define VR_NUM_FEATURES 6

// Returns 0=on, 1=off, 2=open, 3=close, or -1 on error
int VR_Recognize_Command(const char *wav_path);

#endif
