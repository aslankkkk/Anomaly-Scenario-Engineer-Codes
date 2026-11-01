import asyncio
from aiocoap import *

async def send_single_burst(i):
    try:
        # Membuat context baru untuk mensimulasikan client yang berbeda
        ctx = await Context.create_client_context()
        request = Message(code=GET, uri="coap://192.168.12.236:5683/motion")
        
        # Kirim request
        await ctx.request(request).response
        print(f"Burst ke-{i+1} sukses")
        
        # Tutup context
        await ctx.shutdown()
    except Exception as e:
        print(f"Request {i+1} gagal: {e}")

async def main():
    print("Memulai CoAP Connection Burst...")
    
    # Menyiapkan 100 task untuk dieksekusi secara serentak
    tasks = [send_single_burst(i) for i in range(100)]
    
    # Menjalankan semua task bersamaan (menghasilkan efek burst sesungguhnya)
    await asyncio.gather(*tasks)
    
    print("Selesai.")

if _name_ == '_main_':
    asyncio.run(main())