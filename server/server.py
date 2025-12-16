from fastapi import FastAPI, Request
import numpy as np
import soundfile as sf
import requests

app = FastAPI()

SAMPLE_RATE = 16000

@app.post("/upload-audio")
async def upload_audio(req: Request):
    raw = await req.body()

    # ESP32 gửi int16 PCM
    audio = np.frombuffer(raw, dtype=np.int16)

    # Lưu WAV tạm
    sf.write("audio.wav", audio, SAMPLE_RATE)

    # ===== GỌI API AI (ví dụ OpenAI / Whisper / bất kỳ bên thứ 3) =====
    # Ví dụ minh hoạ (giả lập)
    text = fake_speech_to_text("audio.wav")

    return {
        "text": text
    }

def fake_speech_to_text(wav):
    # Tạm giả lập
    return "xin chao toi la esp ba hai"
