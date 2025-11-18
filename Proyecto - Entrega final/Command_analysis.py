import soundfile as sf
import numpy as np

audio, sr = sf.read("Command.wav")
print("sr =", sr)
print("shape =", audio.shape)
print("max =", np.max(audio), "min =", np.min(audio))
print("unique values (first 10):", np.unique(audio)[:10])