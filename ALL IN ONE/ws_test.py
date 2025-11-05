import asyncio
import websockets

async def main():
    uri = 'ws://localhost:3000'
    try:
        async with websockets.connect(uri) as ws:
            print('connected')
            count = 0
            async for msg in ws:
                print('MSG:', msg)
                count += 1
                if count >= 3:
                    break
    except Exception as e:
        print('error', e)

if __name__ == '__main__':
    asyncio.run(main())
