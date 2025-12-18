from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import numpy as np
import soundfile as sf
import os, time, asyncio
try:
    from .client import api
except ImportError:
    from client import api

app = FastAPI()

AUDIO_DIR = "audio"
SAMPLE_RATE = 16000
os.makedirs(AUDIO_DIR, exist_ok=True)


@app.websocket("/ws/audio")
async def ws_audio(ws: WebSocket):
    await ws.accept()
    print("🎤 ESP32 connected")

    pcm_chunks = []
    expected_seq = 0

    try:
        while True:
            data = await ws.receive()

            # ===== BINARY =====
            if "bytes" in data:
                raw = data["bytes"]

                if len(raw) < 4:
                    continue

                seq = int.from_bytes(raw[:4], "little")
                if seq != expected_seq:
                    print(f"⚠️ Packet lost: expected {expected_seq}, got {seq}")
                expected_seq = seq + 1

                pcm = np.frombuffer(raw[4:], dtype=np.int16)
                pcm_chunks.append(pcm)

            # ===== TEXT =====
            elif "text" in data:
                if data["text"] == "END":
                    print("🛑 END received")

                    if not pcm_chunks:
                        print("⚠️ No audio")
                        continue

                    # ---- COPY & RESET IMMEDIATELY ----
                    audio_chunks = pcm_chunks
                    pcm_chunks = []
                    expected_seq = 0

                    audio = np.concatenate(audio_chunks)
                    filename = f"{AUDIO_DIR}/session_{int(time.time())}.wav"
                    sf.write(filename, audio, SAMPLE_RATE)

                    print(f"💾 Saved {filename} ({len(audio)/SAMPLE_RATE:.2f}s)")

                    # ---- RUN STT OFF LOOP ----
                    loop = asyncio.get_running_loop()
                    try:
                        text = await loop.run_in_executor(
                            None,
                            api.call_api_from_chunks,
                            audio_chunks
                        )
                        text = await api.call_api_from_chunks(audio_chunks)
                        await ws.send_text(text)
                        print(f"📝 STT: {text}")
                    except Exception as e:
                        print("❌ STT error:", e)
                        try:
                            await ws.send_text("STT error")
                        except:
                            pass

    except WebSocketDisconnect:
        print("❌ ESP32 disconnected")
    except Exception as e:
        print("❌ Server error:", e)
