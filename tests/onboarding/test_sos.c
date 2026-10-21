#include <assert.h>
#include <stdio.h>
static unsigned now_ms, release_at, led, high_count, low_count;
static unsigned highs[12], lows[12];
int gpio_get(unsigned pin) { assert(pin == 14); return now_ms >= release_at; }
void gpio_put(unsigned pin, int val) { assert(pin == 15); led = val; if(val) highs[high_count++]=now_ms; else lows[low_count++]=now_ms; }
void sleep_ms(unsigned ms) { assert(ms <= 5); now_ms += ms; }
void gpio_init(unsigned p) {(void)p;}
void gpio_set_dir(unsigned p,int dir) {(void)p;(void)dir;}
void gpio_pull_up(unsigned p) {(void)p;}
#define main sos_main
#include "../../docs/onboarding-corrections/examples/sos.c"
#undef main
static void reset(unsigned release) {now_ms=led=high_count=low_count=0;release_at=release;}
int main(void) {
 reset(99999);assert(play_sos());assert(high_count==9 && low_count==9 && !led);assert(now_ms==6800);
 for(unsigned i=0;i<9;i++) assert(lows[i]-highs[i]==(i>=3 && i<6 ? 600:200));
 for(unsigned i=0;i<8;i++)assert(highs[i+1]-lows[i]==(i==2 || i==5 ? 600:200));
 for(unsigned release=0;release<6800;release++) {
  reset(release);assert(!play_sos());assert(!led);assert(now_ms>=release && now_ms-release<=5);
 }
 reset(7);assert(!wait_held(20));assert(now_ms==10);
 reset(25);assert(wait_held(20));assert(now_ms==20);
 puts("PASS: full SOS waveform/gaps; cancellation at every one of 6800 release times; press filter");
}
