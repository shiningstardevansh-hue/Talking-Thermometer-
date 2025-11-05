#!/usr/bin/env python3
import websockets
import asyncio
import json
import platform
import argparse
from datetime import datetime

# Platform-specific TTS setup
if platform.system() == 'Darwin':  # macOS
    import os
    def speak(text):
        os.system(f'say "{text}"')
elif platform.system() == 'Windows':
    import win32com.client
    speaker = win32com.client.Dispatch("SAPI.SpVoice")
    def speak(text):
        speaker.Speak(text)
else:  # Linux/Others - requires espeak
    import os
    def speak(text):
        os.system(f'espeak "{text}"')

class TempMonitor:
    def __init__(self, ws_url='ws://localhost:3000', tts_enabled=True):
        self.ws_url = ws_url
        self.tts_enabled = tts_enabled
        self.last_temp = None
        self.last_speak = 0  # timestamp of last spoken update
        
    def format_temp(self, temp_c, temp_f):
        return f"Temperature is {temp_c:.1f}°C or {temp_f:.1f}°F"
    
    async def handle_message(self, msg):
        try:
            data = json.loads(msg)
            if data.get('type') == 'reading':
                reading = data['data']
                temp_c = reading.get('tempC')
                temp_f = reading.get('tempF')
                
                if temp_c is not None and temp_f is not None:
                    self.last_temp = (temp_c, temp_f)
                    now = datetime.now()
                    text = self.format_temp(temp_c, temp_f)
                    print(f"[{now:%H:%M:%S}] {text}")
                    
                    # Speak temperature if enabled and enough time has passed
                    if self.tts_enabled and (now.timestamp() - self.last_speak) >= 10:
                        speak(text)
                        self.last_speak = now.timestamp()
                        
        except json.JSONDecodeError:
            print(f"Error decoding message: {msg}")
        except Exception as e:
            print(f"Error processing message: {e}")

    async def connect(self):
        while True:
            try:
                async with websockets.connect(self.ws_url) as ws:
                    print(f"Connected to {self.ws_url}")
                    speak("Connected to temperature monitor") if self.tts_enabled else None
                    
                    while True:
                        try:
                            message = await ws.recv()
                            await self.handle_message(message)
                        except websockets.ConnectionClosed:
                            break
                            
            except Exception as e:
                print(f"Connection error: {e}")
                await asyncio.sleep(5)  # Wait before retrying
                continue

def main():
    parser = argparse.ArgumentParser(description='Blind Temperature Monitor')
    parser.add_argument('--url', default='ws://localhost:3000',
                      help='WebSocket server URL (default: ws://localhost:3000)')
    parser.add_argument('--no-tts', action='store_true',
                      help='Disable text-to-speech')
    args = parser.parse_args()
    
    monitor = TempMonitor(ws_url=args.url, tts_enabled=not args.no_tts)
    
    print(f"Starting temperature monitor...")
    print(f"Server URL: {args.url}")
    print(f"Text-to-speech: {'disabled' if args.no_tts else 'enabled'}")
    print("Press Ctrl+C to exit")
    
    try:
        asyncio.run(monitor.connect())
    except KeyboardInterrupt:
        print("\nShutting down...")
        speak("Shutting down temperature monitor") if not args.no_tts else None

if __name__ == "__main__":
    main()