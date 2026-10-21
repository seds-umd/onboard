#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include "hardware/i2c.h"
#include "imu.h"
static int scenario, inits, reads;
static jmp_buf done;
static char output[4096];
static size_t used;
i2c_inst_t bus; i2c_inst_t *i2c0=&bus;
void i2c_init(i2c_inst_t *i,unsigned hz) {assert(i==i2c0 && hz==400000);}
void gpio_set_function(unsigned pin,int fn) {assert(pin<=1 && fn==3);}
void gpio_pull_up(unsigned pin) {assert(pin<=1);}
void stdio_init_all(void) {}
bool stdio_usb_connected(void) {return true;}
void tight_loop_contents(void) {longjmp(done,1);}
void sleep_ms(unsigned ms) {assert(ms==100);longjmp(done,2);}
static int capture(const char *fmt,...) {
 va_list args;va_start(args,fmt);int n=vsnprintf(output+used,sizeof output-used,fmt,args);va_end(args);assert(n>0);used+=(size_t)n;return n;
}
lsm6dsox_t *lsm6dsox_create(i2c_inst_t *i,uint8_t addr) {assert(i==i2c0 && addr==0x6a);return scenario==0 ? NULL : (lsm6dsox_t *)&bus;}
lsm6dsox_error_t lsm6dsox_init(const lsm6dsox_t *d) {assert(d);inits++;return scenario==1 ? LSM6DSOX_ERR_BAD_ID:LSM6DSOX_OK;}
const char *lsm6dsox_strerror(lsm6dsox_error_t e) {return e==LSM6DSOX_ERR_BAD_ID?"bad id":"bus error";}
lsm6dsox_error_t lsm6dsox_read(const lsm6dsox_t *d,lsm6dsox_data_t *data) {
 assert(d);reads++;if(scenario==2)return LSM6DSOX_ERR_BUS;if(scenario==3)return LSM6DSOX_ERR_NO_DATA;
 *data=(lsm6dsox_data_t){.ax=1,.ay=2,.az=3,.gx=4,.gy=5,.gz=6};return LSM6DSOX_OK;
}
#define printf capture
#define main onboard_main
#include "../../src/main.c"
#undef main
#undef printf
int main(void) {
 for(scenario=0;scenario<5;scenario++) {
  inits=reads=used=0;output[0]=0;
  int result=setjmp(done);if(!result)onboard_main();
  assert(strstr(output,"LSM6DSOX\n"));
  if(scenario==0) {assert(result==1 && inits==0 && reads==0);assert(strstr(output,"create failed"));}
  if(scenario==1) {assert(result==1 && inits==1 && reads==0);assert(strstr(output,"bad id"));}
  if(scenario>=2) {assert(result==2 && inits==1 && reads==1);}
  if(scenario==2) {assert(strstr(output,"IMU read failed"));assert(!strstr(output,"A[g]"));}
  if(scenario==3) {assert(!strstr(output,"IMU read failed"));assert(!strstr(output,"A[g]"));}
  if(scenario==4) {assert(strstr(output,"A[g]   1.00   2.00   3.00"));assert(strstr(output,"4.0"));}
 }
 puts("PASS: allocation failure; init failure once; bus/no-data skips; valid formatted sample");
}
