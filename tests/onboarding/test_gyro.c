#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "hardware/i2c.h"
i2c_inst_t bus; i2c_inst_t *i2c0=&bus;
static unsigned char selected,ctrl2;
void sleep_ms(unsigned ms) {(void)ms;}
int i2c_write_blocking(i2c_inst_t *i,uint8_t addr,const uint8_t *d,size_t n,bool no_stop) {
 (void)i;(void)addr;(void)no_stop;selected=d[0];if(n==2 && selected==0x11)ctrl2=d[1];return (int)n;
}
int i2c_read_blocking(i2c_inst_t *i,uint8_t addr,uint8_t *d,size_t n,bool no_stop) {
 (void)i;(void)addr;(void)no_stop;memset(d,0,n);
 if(selected==0x1e)d[0]=3;
 if(selected==0x22){for(size_t k=0;k<6;k+=2){d[k]=0xe8;d[k+1]=3;}}
 return (int)n;
}
#include "../../src/imu.c"
int main(void) {
 lsm6dsox_t *imu=lsm6dsox_create(i2c0,0x6a);
 const lsm6dsox_gyro_range_t ranges[]={LSM6DSOX_GYRO_250DPS,LSM6DSOX_GYRO_500DPS,LSM6DSOX_GYRO_1000DPS,LSM6DSOX_GYRO_2000DPS};
 const float expected[]={8.75f,17.5f,35.f,70.f};
 for(unsigned k=0;k<4;k++){
  assert(lsm6dsox_set_gyro(imu,LSM6DSOX_RATE_104HZ,ranges[k])==LSM6DSOX_OK);
  assert(ctrl2==(0x40|(k<<2)));
  lsm6dsox_data_t d;assert(lsm6dsox_read(imu,&d)==LSM6DSOX_OK);
  assert(fabsf(d.gx-expected[k])<0.0001f && d.gx==d.gy && d.gx==d.gz);
 }
 puts("PASS: four datasheet CTRL2_G ranges and raw-to-dps scaling; default500=0x44 and17.5dps/1000counts");
}
