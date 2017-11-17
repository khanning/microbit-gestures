#include "MicroBit.h"
#include "MicroBitUARTService.h"
#include "MicroBitFlash.h"
#include "cvm.h"


MicroBitSerial pc(USBTX, USBRX, 200); 
MicroBitStorage storage;
MicroBitFlash flash;
MicroBitMessageBus messageBus;
MicroBitBLEManager bleManager(storage);
BLEDevice ble;
MicroBitUARTService *uart;

#define TICK_VECTOR 0
#define BUTTONA_VECTOR 4
#define BUTTONB_VECTOR 8

void lib_init(void);
void evt_poll(void);
void dev_poll(void);
void direct_setshape(UBYTE a, UBYTE b, UBYTE c,  UBYTE d,  UBYTE e);
void mwait(SLONG);

UBYTE ugetc(){return pc.read(SYNC_SPINWAIT);}
int ugetcAsync(){return pc.read(ASYNC);}
void uputc(UBYTE c){pc.sendChar(c, SYNC_SPINWAIT);}
UBYTE BLEgetcAsync(){return uart->getc(ASYNC);}
void BLEputc(UBYTE c){uart->putc(c);}

UBYTE BLEgetc(){
    while(1){
        int c = uart->getc(ASYNC);
        if (c!=MICROBIT_NO_DATA) return(c);
    }
}


UBYTE (*xgetc)(void);
void (*xputc)(UBYTE);

void vm_run_cc(void);
void vm_run_vector(int32_t);
extern int tick_evt, btna_evt, btnb_evt, tilt_evt, untilt_evt;
extern volatile uint32_t ticks;

void init(){
    pc.baud(19200);  
    scheduler_init(messageBus);
    microbit_create_heap(MICROBIT_SD_GATT_TABLE_START + MICROBIT_SD_GATT_TABLE_SIZE, MICROBIT_SD_LIMIT);
    ManagedString BLEName("microbit 1");
    ManagedString BLESerial("00003");
    bleManager.init(BLEName, BLESerial, messageBus, false);
    uart = new MicroBitUARTService(ble, 32, 32);
    pc.printf("starting...\n");
}

ULONG read16(){
    UBYTE c1 = xgetc();
    UBYTE c2 = xgetc();
    return (c2<<8)+c1;
}

ULONG read32(){
    UBYTE c1 = xgetc();
    UBYTE c2 = xgetc();
    UBYTE c3 = xgetc();
    UBYTE c4 = xgetc();
    return (c4<<24)+(c3<<16)+(c2<<8)+c1;
}

void readmemory(){
    ULONG addr = read32();
    ULONG count = read16();
    ULONG i;
    for(i=0;i<count;i++) xputc(*((UBYTE*) addr++));
}

void writememory(){
    ULONG addr = read32();
    ULONG count = read16();
    ULONG i;
    for(i=0;i<count;i++){
        UBYTE c = xgetc();
        *((UBYTE*) addr++)=c;
        if(xputc==uputc) xputc(c);
    }
}

void run(){
    vm_run_cc();
    xputc(0xcf);
}


void writeflash(){
    ULONG src = read32();
    ULONG dst = read32();
    ULONG count = read16();
    flash.flash_write((std::uint32_t*)dst, (std::uint32_t*)src, (int)count);
    xputc(0xbf);
}

void eraseflash(){
    ULONG addr = read32();
    flash.erase_page((std::uint32_t*)addr);
    xputc(0xaf);
}

void setshapecmd(){
    direct_setshape(xgetc(), xgetc(), xgetc(), xgetc(), xgetc());
}

void dispatch(UBYTE c){
    if(c==0xff) xputc(23);
    else if(c==0xfe) readmemory();
    else if(c==0xfd) writememory();
    else if(c==0xfc) run();
    else if(c==0xfb) writeflash();
    else if(c==0xfa) eraseflash();
    else if(c==0xf9) setshapecmd();
    else xputc(c);
}

void uDispatch(UBYTE c){
    xgetc = ugetc;
    xputc = uputc;
    dispatch(c);
}

void bleDispatch(UBYTE c){
    xgetc = BLEgetc;
    xputc = BLEputc;
    dispatch(c);
}

int bleAvail(){
    int c = uart->getc(ASYNC);
    return c!=MICROBIT_NO_DATA;
}

int main() {
    init();
    lib_init();
    uint32_t lastticks = ticks; 
    while(1){
        if(ticks!=lastticks){evt_poll(); dev_poll(); lastticks = ticks;} 
        int c = ugetcAsync();
        if (c!=MICROBIT_NO_DATA) uDispatch((UBYTE)c);
        c = uart->getc(ASYNC);
        if (c!=MICROBIT_NO_DATA) bleDispatch(c);
        if(tick_evt){tick_evt=0; vm_run_vector(TICK_VECTOR);}
        if(btna_evt){btna_evt=0; vm_run_vector(BUTTONA_VECTOR);}
        if(btnb_evt){btnb_evt=0; vm_run_vector(BUTTONB_VECTOR);}
    }
}

