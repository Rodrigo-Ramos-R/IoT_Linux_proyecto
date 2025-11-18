import os
import numpy as np
import soundfile as sf
from sklearn.neighbors import KNeighborsClassifier
from sklearn.model_selection import train_test_split
import json

DATA_DIR = r"C:\Keyword_spotting_model\VoiceData"
labels = ["on", "off", "open", "close"]

# ------------------------------------------
# FAST FEATURE EXTRACTION (NO MFCC)
# ------------------------------------------
def extract_features(filepath, target_sr=16000):
    audio, sr = sf.read(filepath)

    # If stereo → convert to mono
    if len(audio.shape) > 1:
        audio = audio[:, 0]

    # Resample to 16 kHz if needed
    if sr != target_sr:
        # Simple downsample by integer factor
        factor = sr // target_sr
        audio = audio[::factor]
        sr = target_sr

    # Normalize
    if np.max(np.abs(audio)) > 0:
        audio = audio / np.max(np.abs(audio))

    N = len(audio)
    if N == 0:
        return [0, 0, 0, 0, 0, 0]

    # 1. Energy
    energy = np.sum(audio ** 2) / N

    # 2. Zero-crossing rate
    zcr = np.mean(np.abs(np.diff(np.sign(audio))))

    # 3. Mean absolute amplitude
    mean_amp = np.mean(np.abs(audio))

    # 4. Std amplitude
    std_amp = np.std(audio)

    # 5. Envelope slope
    frame = max(1, N // 100)
    env = np.array([np.mean(np.abs(audio[i:i+frame])) for i in range(0, N, frame)])
    slope = (env[-1] - env[0]) / len(env) if len(env) > 1 else 0

    # 6. Duration
    duration = N / sr

    return [energy, zcr, mean_amp, std_amp, slope, duration]


# ------------------------------------------
# LOAD DATASET
# ------------------------------------------
print("Loading audio files...")

X = []
y = []

for label_idx, label in enumerate(labels):
    folder = os.path.join(DATA_DIR, label)
    for filename in os.listdir(folder):
        if filename.endswith(".wav"):
            path = os.path.join(folder, filename)
            feats = extract_features(path)
            X.append(feats)
            y.append(label_idx)

X = np.array(X)
y = np.array(y)

# ------------------------------------------
# TRAIN / TEST SPLIT
# ------------------------------------------
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42
)

# ------------------------------------------
# TRAIN MODEL
# ------------------------------------------
clf = KNeighborsClassifier(n_neighbors=3)
clf.fit(X_train, y_train)

acc = clf.score(X_test, y_test)
print(f"Accuracy: {acc*100:.2f}%")

# ------------------------------------------
# SAVE FOR PICO (JSON model)
# ------------------------------------------
model_data = {
    "X": X_train.tolist(),
    "y": y_train.tolist(),
    "labels": labels,
    "n_neighbors": 3
}

with open("model.json", "w") as f:
    json.dump(model_data, f)

print("Saved portable model → model.json")

# ------------------------------------------
# TEST FUNCTION ON PC
# ------------------------------------------
def predict_keyword(path):
    feats = extract_features(path)
    pred = clf.predict([feats])[0]
    return labels[pred]

print("\nExample test:")
print("Detected:", predict_keyword("Command.wav"))
