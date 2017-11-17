#include <stdint.h>

#define g 0x20003000
#define procs 0x30000

extern void *fcns[];

#define cc ((uint8_t*)g)
#define globals ((int32_t*)(g+0x80))
#define stack ((int32_t*)(g+0x100))

#define code ((uint8_t*)procs)

extern void(*prims[])();

int bleAvail(void);
void dev_poll(void);

void run_vm(uint8_t*);
void eol_repeat(void);
void prim_loop(void);
void eol_runmacro(void);
void eolr_waituntil(void);

uint8_t *ip;
int32_t *sp = stack;
int32_t *fp;

extern volatile uint8_t stop_evt;
extern volatile uint32_t ticks;

void vm_init(){
    sp = stack;
}

void vm_run_vector(int32_t v){
    if(*(code+v)!=8)    return;     // check for a valid vector
    run_vm(code+v);
}

void vm_run_cc(){
    run_vm(cc);
}

void run_vm(uint8_t *startip){
    ip = startip;
    fp = 0;
    uint32_t lastticks = ticks; 
    while(1){
        if(ticks!=lastticks){
            if(bleAvail()) break;
            dev_poll();
            lastticks = ticks;
        } 
//        if(buttonedge){buttonedge=0; alloff(); break;}
//      if(stop_evt) {stop_evt=0; break;}
        uint8_t token = *ip++;
        if((token==0)||(token==0xff)) break;
        void(*prim)() = prims[token];
        prim();
    }
}


void eval_done(){}

void eval_byte(){
    *sp++ = *ip++;
}

void eval_num(){
    int32_t t0 = *ip++;
    t0 += (*ip++<<8);
    t0 += (*ip++<<16);
    t0 += (*ip++<<24);
    *sp++ = t0;
}

void eval_list(){
    uint8_t offset = *ip++;
    offset += *ip++<<8;
    *sp++ = (int32_t)ip;
    ip+=offset;
}

void eval_eol(){
    switch(*--sp){
    case 0: ip=(uint8_t*)*--sp; break;
    case 1: eol_repeat(); break;
    case 2: prim_loop(); break;
    case 3: eol_runmacro(); break;
    }
}

void eval_eolr(){eolr_waituntil();}

int32_t *get_local_address(){
    int32_t t0 = (int8_t) *ip++;
    if(t0<0) return fp-t0-1;
    else return fp-t0-4;
}

void eval_lthing(){
    *sp++ = *get_local_address();
}

void eval_lmake(){
    *get_local_address() = *--sp;
}

void eval_ufun(){
    uint32_t newip = *ip++;
    newip += *ip++<<8;
    *sp++ = (int32_t)ip;
    ip = (uint8_t*)(newip+code);
    *sp++ = *ip++;
    *sp++ = (int32_t)fp;
    fp = sp;
    sp+=*ip++;
}

void eval_libfcn(){
    int32_t t0;
    uint8_t fcn = *ip++;
    uint32_t args = (uint32_t)(fcns[2*fcn]);
    void *fptr = (void*)(fcns[2*fcn+1]);
    switch(args){
        case 0: ((void (*)()) fptr)();
        break;
        case 1: ((void (*)(int32_t)) fptr)(*--sp);
        break;
        case 2: t0 = *--sp; ((void (*)(int32_t, int32_t)) fptr)(*--sp, t0);
        break;
    }
}

void eval_libfcnr(){
    int32_t t0;
    int32_t res=0;
    uint8_t fcn = *ip++;
    uint32_t args = (uint32_t)(fcns[2*fcn]);
    void *fptr = (void*)(fcns[2*fcn+1]);
    switch(args){
        case 0: res = ((uint32_t (*)()) fptr)();
        break;
        case 1: res = ((uint32_t (*)(int32_t)) fptr)(*--sp);
        break;
        case 2: t0 = *--sp; res = ((int32_t (*)(int32_t, int32_t)) fptr)(*--sp, t0);
        break;
    }
    *sp++ = res;
}

void prim_stop(){
    int32_t t0;
    if(fp==0) return;
    sp = fp;
    fp = (int32_t*) *--sp;
    t0 = *--sp;
    ip = (uint8_t*)*--sp;
    sp-= t0;
}

void prim_output(){
    int32_t t0;
    int32_t res = *--sp;
    sp = fp;
    fp = (int32_t*) *--sp;
    t0 = *--sp;
    ip = (uint8_t*)*--sp;
    sp-= t0;
    *sp++ = res;
}

void prim_call(){
    int32_t newip = *--sp;
    *sp++ = (int32_t)ip;
    ip = (uint8_t*)(newip+code);
    *sp++ = *ip++;
    *sp++ = (int32_t)fp;
    fp = sp;
    sp+=*ip++;
}


void prim_run(){
    int32_t addr = *--sp;
    *sp++ = (int32_t)ip;
    *sp++ = 0;
    ip = (uint8_t*)addr;
}

void prim_runmacro(){
    int32_t addr = *--sp;
    *sp++ = (int32_t)fp;
    *sp++ = (int32_t)ip;
    *sp++ = 3;
    ip = (uint8_t*)addr;
    fp = (int32_t*) *(fp-1);
}

void eol_runmacro(){
    ip = (uint8_t*)(*--sp);
    fp = (int32_t*)(*--sp);
}

void prim_repeat(){
    *sp++ = (int32_t)ip;
    eol_repeat();
}

void eol_repeat(){
    if(*(sp-3)==0) {ip = (uint8_t*)(*--sp); sp-=2;}
    else {(*(sp-3))--; ip=(uint8_t*)(*(sp-2)); *sp++=1;}
}

void prim_loop(){
    ip = (uint8_t*)*(sp-1);
    *sp++ = 2;
}

void prim_if(){
    int32_t addr = *--sp;
    if(!*--sp) return;
    *sp++ = (int32_t)ip;
    *sp++ = 0;
    ip = (uint8_t*)addr;
}

void prim_ifelse(){
    int32_t addr;
    int32_t faddr = *--sp;
    int32_t taddr = *--sp;
    if(*--sp) addr = taddr;
    else addr = faddr;
    *sp++ = (int32_t)ip;
    *sp++ = 0;
    ip = (uint8_t*)addr;
}

void prim_waituntil(){
    *sp++ = (int32_t) ip;
    ip = (uint8_t*)*(sp-2);
}

void eolr_waituntil(){
    int32_t res = *--sp;
    if(!res) ip = (uint8_t*)*(sp-2);
    else {ip = (uint8_t*)*--sp; --sp;}
}

void prim_gwrite(){int32_t t0 = *--sp; globals[*--sp] = t0;}

void prim_gread(){
    int32_t t0 = globals[*--sp];
    *sp++ = t0;
}

void prim_sum(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1+t0;}
void prim_difference(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1-t0;}
void prim_product(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1*t0;}
void prim_quotient(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1/t0;}

void prim_mod(){
    int32_t t0 = *--sp;
    int32_t res = *--sp%t0;
    if(res<0) res+= t0;
    *sp++=res;
}

void prim_random(){
//    int32_t t0 = *--sp;
//    *sp++ = ((rand()+(rand()<<15))&0x7fffffff)%t0;
  }

void prim_extend(){int32_t t0 = *--sp;  *sp++ = (int32_t)(int16_t)t0;}
void prim_extendb(){int32_t t0 = *--sp;  *sp++ = (int32_t)(int8_t)t0;}

void prim_equal(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1==t0;}
void prim_ne(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1!=t0;}
void prim_greater(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1>t0;}
void prim_less(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1<t0;}

void prim_and(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1&t0;}
void prim_or(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1|t0;}
void prim_xor(){int32_t t0 = *--sp; int32_t t1 = *--sp; *sp++ = t1^t0;}
void prim_not(){if(*--sp) *sp++=0; else *sp++=1;}

void prim_lsl(){
    int32_t cnt = *--sp;
    int32_t n = *--sp;
    if(cnt<0) *sp++ = n>>-cnt;
    else *sp++ = n<<cnt;
}


void prim_readb(){int32_t t0 = (int32_t)*(uint8_t*)*--sp; *sp++ = t0;}
void prim_writeb(){uint8_t t0 = (uint8_t)*--sp; *(uint8_t*)*--sp = t0;}
void prim_readh(){int32_t t0 = (int32_t)*(uint16_t*)*--sp; *sp++ = t0;}
void prim_writeh(){uint16_t t0 = (uint16_t)*--sp; *(uint16_t*)*--sp = t0;}
void prim_read(){int32_t t0 = *(int32_t*)*--sp; *sp++ = t0;}
void prim_write(){int32_t t0 = *--sp; *(int32_t*)*--sp = t0;}

void prim_sp(){int32_t t0 = (int32_t) sp; *sp++ = t0;}

void(*prims[])() = {
    eval_done,
    eval_byte, eval_num,
    eval_list, eval_eol, eval_eolr,
    eval_lthing, eval_lmake,
    eval_ufun, eval_done,
    eval_libfcn, eval_libfcnr,
    prim_stop, prim_output, prim_call,
    prim_run, prim_runmacro, prim_repeat, prim_loop,
    prim_if, prim_ifelse, prim_waituntil,
    prim_gwrite, prim_gread,
    prim_sum, prim_difference, prim_product, prim_quotient, prim_mod,
    prim_random, prim_extend, prim_extendb,
    prim_equal, prim_ne, prim_greater, prim_less,
    prim_and, prim_or, prim_xor,
    prim_not, prim_lsl,
    prim_readb, prim_writeb,
    prim_readh, prim_writeh,
    prim_read, prim_write,
    prim_sp
};
