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
                
                # Tự động đồng bộ lại nếu nhận được gói số 0 (Bắt đầu phiên mới đột ngột)
                if seq == 0 and expected_seq != 0:
                    expected_seq = 0

                if seq != expected_seq:
                    print(f"⚠️ Packet lost: expected {expected_seq}, got {seq}")
                expected_seq = seq + 1

                # In seq mỗi 10 gói tin để debug (dùng \r để cập nhật trên cùng 1 dòng)
                if seq % 10 == 0:
                    print(f"📡 Receiving seq: {seq}", end="\r", flush=True)

                pcm = np.frombuffer(raw[4:], dtype=np.int16)
                pcm_chunks.append(pcm)

            # ===== TEXT =====
            elif "text" in data:
                text_data = data["text"]
                if text_data.startswith("END"):
                    # Parse total seq sent by ESP32
                    parts = text_data.split()
                    total_sent = int(parts[1]) if len(parts) > 1 else 0
                    lang = parts[2] if len(parts) > 2 else "vi"
                    # Fix: Map numeric enum from ESP32 to language code
                    if lang == "0": lang = "en"
                    elif lang == "1": lang = "vi"

                    received_count = len(pcm_chunks)
                    
                    loss_rate = 0
                    if total_sent > 0:
                        loss_rate = (1 - received_count / total_sent) * 100
                    
                    print(f"🛑 END received. Sent: {total_sent}, Recv: {received_count}, Loss: {loss_rate:.1f}%")

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
                        stt_text, text = await loop.run_in_executor(
                            None,
                            lambda: api.call_api_from_chunks(audio_chunks, language=lang.lower())
                        )
                        
                        # Tạo title ngắn < 15 ký tự từ câu hỏi
                        title = stt_text.strip()
                        if len(title) > 12: title = title[:12] + ".."
                        
                        # Gửi theo format: RESP:Title|Content
                        await ws.send_text(f"RESP:{title}|{text}")
                        print(f"📝 STT: {stt_text} -> {text}")

                    except Exception as e:
                        print("❌ STT error:", e)
                        try:
                            await ws.send_text("STT error")
                        except:
                            break

    except WebSocketDisconnect:
        print("❌ ESP32 disconnected")
    except Exception as e:
        print("❌ Server error:", e)
