import os
import numpy as np
import soundfile as sf
from sklearn.neighbors import KNeighborsClassifier

DATA_DIR = r"C:\Keyword_spotting_model\VoiceData"
labels = ["on", "off", "open", "close"]

NUM_FEATURES = 6
K = 3

def extract_features(filepath, target_sr=16000):
    audio, sr = sf.read(filepath)

    # Mono
    if audio.ndim > 1:
        audio = audio[:, 0]

    # Downsample using integer factor
    if sr != target_sr:
        factor = sr // target_sr
        audio = audio[::factor]
        sr = target_sr

    # Normalize
    if np.max(np.abs(audio)) > 0:
        audio = audio / np.max(np.abs(audio))

    N = len(audio)
    if N < 10:
        return [0.0] * NUM_FEATURES

    # Features
    energy = np.sum(audio**2) / N
    signs = np.sign(audio)
    zc = np.sum(np.abs(np.diff(signs))) / 2.0
    zcr = zc / (N - 1)
    mean_abs = np.mean(np.abs(audio))
    mean_val = np.mean(audio)
    std_amp = np.sqrt(np.mean((audio - mean_val)**2))

    num_seg = 50
    seg_len = max(1, N // num_seg)
    env = np.array([np.mean(np.abs(audio[i:i+seg_len]))
                    for i in range(0, N, seg_len)])
    slope = env[-1] - env[0] if len(env) >= 2 else 0.0

    duration = N / sr

    return [float(energy), float(zcr), float(mean_abs),
            float(std_amp), float(slope), float(duration)]


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

print(f"Loaded {len(X)} samples.")

# Train & report accuracy
clf = KNeighborsClassifier(n_neighbors=K)
clf.fit(X, y)
acc = clf.score(X, y)
print(f"Training-set accuracy: {acc*100:.2f}%")

# Export for Pico: <feat1> <feat2> ... <feat6> <label>
model_path = "model_knn.txt"
with open(model_path, "w") as f:
    num_samples = len(X)
    f.write(f"{num_samples} {NUM_FEATURES} {K}\n")
    for i in range(num_samples):
        row = [f"{v:.8f}" for v in X[i]] + [str(y[i])]
        f.write(" ".join(row) + "\n")

print(f"Saved KNN model → {model_path}")
