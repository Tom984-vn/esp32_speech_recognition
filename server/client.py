from openai import OpenAI
from dotenv import load_dotenv
import os
import io
import numpy as np
import soundfile as sf
import json

load_dotenv()


BASE_URL = "https://mkp-api.fptcloud.com"
API_KEY = os.getenv("FPT_token")
MODEL_NAME = "whisper-large-v3-turbo"
LANGUAGE = 'vi'

class call_api:
    def __init__(self):
        self.client = OpenAI(
            base_url=BASE_URL,
            api_key=API_KEY
        )
    def call_api_from_chunks(self, chunks: list, sample_rate: int = 16000, language: str = LANGUAGE) -> tuple[str, str]:
        if not chunks:
            return ""

        # Concatenate chunks
        audio_data = np.concatenate(chunks)
        
        if len(audio_data) == 0:
            return ""

        # Create in-memory file
        buffer = io.BytesIO()
        buffer.name = "audio.wav"
        sf.write(buffer, audio_data, sample_rate, format='WAV', subtype='PCM_16')
        buffer.seek(0)

        response = self.client.audio.transcriptions.create(
            model=MODEL_NAME,
            file=("audio.wav", buffer, "audio/wav"),
            language=language,
            timeout=60,
            response_format="json"
        )
        print(response.text)
        llm_reply = self.LLM_call(response.text)
        return response.text, llm_reply
    
    def LLM_call(self, prompt: str) -> str:
        response = self.client.chat.completions.create(
            model="Llama-3.3-Swallow-70B-Instruct-v0.4",
            messages=[
                {"role": "system", "content": "Bạn là trợ lý giọng nói. Hãy trả lời ngắn gọn, súc tích. QUAN TRỌNG: Vì kết quả hiển thị trên màn hình OLED nhỏ, TUYỆT ĐỐI KHÔNG dùng định dạng Markdown (không dùng **, *, #, -). Chỉ trả về văn bản thô (plain text). Tránh dùng ký tự đặc biệt."},
                {"role": "user", "content": prompt}
            ],
            temperature=0.5
        )
        return response.choices[0].message.content
    
    def text_to_speech(self, text: str, output_file: str):
        try:
            response = self.client.audio.speech.create(
                model="tts-1",
                voice="alloy",
                input=text,
                response_format="wav"
            )
            response.stream_to_file(output_file)
        except Exception as e:
            print(f"TTS Error: {e}")

api = call_api()
