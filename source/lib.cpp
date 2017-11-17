#include <stdint.h>
#include "MicroBit.h"
#include "MicroBitTicker.h"
#include "cvm.h"


#define flashshapes 0x31000
#define ramshapes 0x31000
extern char shapes[];
unsigned char directshape[5];

MicroBitDisplay display;
MicroBitButton buttona(MICROBIT_PIN_BUTTON_A, MICROBIT_ID_BUTTON_A);
MicroBitButton buttonb(MICROBIT_PIN_BUTTON_B, MICROBIT_ID_BUTTON_B);
MicroBitI2C i2c = MicroBitI2C(I2C_SDA0, I2C_SCL0); 
MicroBitAccelerometer acc = MicroBitAccelerometer(i2c); 
MicroBitFont charfont;
MicroBitFont romshapefont((const unsigned char*)shapes, 32+8);
MicroBitFont flashshapefont((const unsigned char*)flashshapes, 32+20);
MicroBitFont ramshapefont((const unsigned char*)directshape, 32+2);
MicroBitTicker ticker;
MicroBitRadio radio;

extern MicroBitStorage flash;
extern MicroBitSerial pc;
extern BLEDevice ble;

void mwait(SLONG);
void dshape(char);
void clear(void);
SLONG getacc(void);

//SLONG fftin[16] = {0,1950903,3826834,5555702,7071068,8314696,9238795,9807853,
//                   10000000,9807853,9238795,8314696,7071068,5555702,3826834,1950903};
SLONG fftin[16];
SLONG buf_ave(SSHORT*);

extern volatile uint32_t ticks;
uint32_t tick_period;
ULONG t0;
int tick_evt;
int btna_evt, last_btna;
int btnb_evt, last_btnb;

char thisshape = 0;

SSHORT xbuf[32];
SSHORT ybuf[32];
SSHORT zbuf[32];
SLONG last_ave[3];

void lib_init(){
    display.setDisplayMode(DISPLAY_MODE_GREYSCALE);
    display.setBrightness(100);
    dshape(101); mwait(50);
    dshape(102); mwait(50);
    dshape(103); mwait(50);
    dshape(102); mwait(50);
    dshape(101); mwait(50);
    clear();
}

void evt_poll(){
  if(tick_period&&((ticks%tick_period)==0)) tick_evt = true;
  int this_btna = buttona.isPressed();
  if(this_btna&!last_btna) btna_evt=1;
  last_btna = this_btna;
  int this_btnb = buttonb.isPressed();
  if(this_btnb&!last_btnb) btnb_evt=1;
  last_btnb = this_btnb;
}

void dev_poll(){
  xbuf[ticks&0x1f] = acc.getX();
  ybuf[ticks&0x1f] = acc.getY();
  zbuf[ticks&0x1f] = acc.getZ();
  fftin[ticks%16] = getacc();
}

void direct_setshape(UBYTE a, UBYTE b, UBYTE c,  UBYTE d,  UBYTE e){
  directshape[0] = a;
  directshape[1] = b;
  directshape[2] = c;
  directshape[3] = d;
  directshape[4] = e;
  MicroBitFont::setSystemFont(ramshapefont); 
  display.printChar(32);
}


void startticker(uint32_t n){tick_period = n;}
void stopticker(){tick_period = 0;}

void print(SLONG c){pc.printf("%d\n", c);}
void prh(SLONG c){pc.printf("%08X\n", c);}
void prhb(SLONG c){pc.printf("%02X\n", c&0xff);}
void prhh(SLONG c){pc.printf("%04X\n", c&0xffff);}
void prs(UBYTE* s){pc.printf("%s\n", s);}

void prf(char *s, int32_t n) {
  for (; *s; s++) {
    if (*s=='%'){
      s++;
      switch (*s){
          case 'b': pc.printf("%02x", n); break;
          case 'h': pc.printf("%04x", n); break;
          case 'w': pc.printf("%08x", n); break;
          case 'd': pc.printf("%d", n); break;
          case 'f': 
            pc.printf("%d", n/10000000); 
            pc.printf("."); 
            if(n<0) n=-n;
            pc.printf("%04d", (n%10000000)/1000); 
            break;
          case 0: return;
          default: pc.sendChar(*s); break;
      }
    } else pc.sendChar(*s);
  }
}


void scroll(char* s){display.scroll(s);}
void dprint(char* s, ULONG d){display.print(s, d);}
void dchar(char c){MicroBitFont::setSystemFont(charfont); display.printChar(c);}
void clear(){display.clear(); thisshape = 0;}
void setbrightness(int b){display.setBrightness(b);}

void flashwrite(uint32_t* addr, ULONG data){flash.flashWordWrite(addr, data);}
void flasherase(uint32_t* addr){flash.flashPageErase(addr);}

void resett(){t0 = (ULONG)system_timer_current_time();}
ULONG timer(){return ((ULONG)system_timer_current_time()) - t0;}
ULONG get_ticks(){return ticks;}

void dshape(char s){
  thisshape = s;
  if(s<100){
    MicroBitFont::setSystemFont(flashshapefont); 
    display.printChar(s+32);
    thisshape = s;
  }
  else {
    MicroBitFont::setSystemFont(romshapefont); 
    display.printChar(s-100+32);
    thisshape = 0;
  }
}

void nextshape(){
  unsigned char *font = (unsigned char*)flashshapes;
  thisshape++;
  if(font[5*thisshape]==0xff) thisshape = 0;
  MicroBitFont::setSystemFont(flashshapefont); 
  display.printChar(thisshape+32);
}

void mwait(SLONG d){
    if(d<0) return;
    uint64_t end = system_timer_current_time()+d;
    while(system_timer_current_time()<end){};
}
 
ULONG get_buttona(){return buttona.isPressed();}
ULONG get_buttonb(){return buttonb.isPressed();}

SLONG buf_ave(SSHORT *buf){
  int res=0;
  for(int i=0;i<32;i++) res+=buf[i];
  return (SLONG)(res/32);
}

SLONG getx(){return buf_ave(xbuf);}
SLONG gety(){return buf_ave(ybuf);}
SLONG getz(){return buf_ave(zbuf);}

int blerunning(){return ble_running();}
void switchradio(){ble.shutdown(); radio.enable();}
void rsend(uint8_t c){radio.datagram.send(&c, 1);}

int rrecc(){
  uint8_t c;
  radio.idleTick();
  int len = radio.datagram.recv(&c, 1);
  if(len==MICROBIT_INVALID_PARAMETER) return -1;
  else return c;
}

struct complex {
  SLONG re;
  SLONG im;
};

struct complex wktable[] = {
  {10000000,0},
  {9807853,-1950903},
  {9238795,-3826834},
  {8314696,-5555702},
  {7071068,-7071068},
  {5555702,-8314696},
  {3826834,-9238795},
  {1950903,-9807853},
  {0,-10000000},
  {-1950903,-9807853},
  {-3826834,-9238795},
  {-5555702,-8314696},
  {-7071068,-7071068},
  {-8314696,-5555702},
  {-9238795,-3826834},
  {-9807853,-1950903}
};

struct complex fftpts[32]; 

int swap_table[32] = {0,16,8,24,4,20,12,28,2,18,10,26,6,22,14,30,1,17,9,25,5,21,13,29,3,19,11,27,7,23,15,31};


SLONG sprod(SLONG a, SLONG b){
  long long prod = ((long long)a) * ((long long)b);
  return (SLONG)(prod/10000000L);
}

SLONG squot(SLONG a, SLONG b){
  long long prod = ((long long)a) * 10000000L;
  return (SLONG)(prod/((long long)b));
}

struct complex cplus(struct complex a, struct complex b){
  struct complex res;
  res.re = a.re+b.re;
  res.im = a.im+b.im;
  return res;
}

struct complex cminus(struct complex a, struct complex b){
  struct complex res;
  res.re = a.re-b.re;
  res.im = a.im-b.im;
  return res;
}

struct complex ctimes(struct complex a, struct complex b){
  struct complex res;
  res.re = sprod(a.re,b.re)-sprod(a.im,b.im);
  res.im = sprod(a.re,b.im)+sprod(a.im,b.re);
  return res;
}

#define N 32

void butterfly(){
  for(int L=2;L<=N;L<<=1){
    for(int k=0;k<L/2;k++){
      struct complex w = wktable[k*N/L];
      for(int j=0;j<N/L;j++){
        struct complex tao = ctimes(w,fftpts[j*L+k+L/2]);
        fftpts[j*L+k+L/2] = cminus(fftpts[j*L+k],tao); 
        fftpts[j*L+k] = cplus(fftpts[j*L+k], tao); 
      }
    }
  }
}

void copy_fftin(){
  int i;
  struct complex zero = {0,0};
  SLONG sum=0,ave;
  for(i=0;i<N/2;i++) sum+=fftin[i];
  ave = squot(sum,N/2*10000000);
  for(i=0;i<N;i++) fftpts[i]=zero;
  for(i=0;i<N/2;i++) fftpts[i].re=fftin[i]-ave;
}

void fft_swap(){
  int swapi;
  struct complex temp;       
  for(int i=0;i<N;i++){
    swapi = swap_table[i];
    if(swapi>i){
      temp = fftpts[i];
      fftpts[i] = fftpts[swapi];
      fftpts[swapi] = temp;
    }
  }
}

void fft(){
  copy_fftin();
  fft_swap();
  butterfly();
}

SLONG freq(SLONG n){
  struct complex f = fftpts[n];
  float re = ((float)f.re)/10000000;
  float im = ((float)f.im)/10000000;
  float abs = sqrt(re*re+im*im);
  return (SLONG)(abs*10000000);  
}

SLONG getacc(){
  float x = ((float)getx())/1024;
  float y = ((float)gety())/1024;
  float z = ((float)getz())/1024;
  float acc = sqrt(x*x+y*y+z*z);
  return (SLONG)(acc*10000000);
}

void *fcns[] = {
    (void*) 1, (void*) print,  
    (void*) 1, (void*) prh,  
    (void*) 1, (void*) prhb,  
    (void*) 1, (void*) prhh,  
    (void*) 1, (void*) prs,  
    (void*) 2, (void*) prf,  
    (void*) 1, (void*) scroll,  
    (void*) 2, (void*) dprint,  
    (void*) 1, (void*) dchar,  
    (void*) 1, (void*) dshape,  
    (void*) 0, (void*) clear,  
    (void*) 1, (void*) setbrightness,  
    (void*) 2, (void*) flashwrite,  
    (void*) 1, (void*) flasherase,  
    (void*) 0, (void*) resett,  
    (void*) 0, (void*) timer,  
    (void*) 0, (void*) get_ticks,  
    (void*) 1, (void*) mwait,  
    (void*) 0, (void*) get_buttona,  
    (void*) 0, (void*) get_buttonb,  
    (void*) 0, (void*) getx,  
    (void*) 0, (void*) gety,  
    (void*) 0, (void*) getz,  
    (void*) 1, (void*) startticker,
    (void*) 0, (void*) stopticker,
    (void*) 0, (void*) blerunning,
    (void*) 0, (void*) switchradio,
    (void*) 1, (void*) rsend,
    (void*) 0, (void*) rrecc,
    (void*) 0, (void*) nextshape,
    (void*) 2, (void*) sprod,
    (void*) 2, (void*) squot,
    (void*) 0, (void*) fft,
    (void*) 0, (void*) getacc,
    (void*) 1, (void*) freq,
};
