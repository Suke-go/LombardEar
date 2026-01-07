
import asyncio
import websockets
import json

async def test_control():
    uri = "ws://localhost:8000/ws"
    async with websockets.connect(uri) as websocket:
        # Send a control parameter update
        msg = {
            "type": "control",
            "active": True, # This might be needed depending on implementation, checking web_server.c would be good
            "alpha": 0.25,
            "leak_lambda": 0.005,
            "mu_max": 0.1
        }
        await websocket.send(json.dumps(msg))
        print("Sent update message")
        
        # Wait a bit to ensure server processes it
        await asyncio.sleep(1)

if __name__ == "__main__":
    asyncio.run(test_control())
