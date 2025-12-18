from openai import OpenAI
from dotenv import load_dotenv
import os
import io
import numpy as np
import soundfile as sf

load_dotenv()


BASE_URL = "https://mkp-api.fptcloud.com"
API_KEY = os.getenv("FPT_token")
MODEL_NAME = "whisper-large-v3-turbo"
LANGUAGE = 'en'

class call_api:
    def __init__(self):
        if not API_KEY:
            raise ValueError("API Key 'FPT_token' is missing. Please check your .env file.")
        self.client = OpenAI(api_key=API_KEY, base_url=BASE_URL)

    def call_api(self, wav_path: str) -> str:

        file = wav_path
        # file = "speech.mp3"
        with open(file, "rb") as audio_file:
            response = self.client.audio.transcriptions.create(
                model=MODEL_NAME,
                file=audio_file,
                language=LANGUAGE,
                timeout=60,
                response_format="json"
            )

            return response.text

    def call_api_from_chunks(self, chunks: list, sample_rate: int = 16000) -> str:
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
            language=LANGUAGE,
            timeout=60,
            response_format="json"
        )
        return response.text

api = call_api()
