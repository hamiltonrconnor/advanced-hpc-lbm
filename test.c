#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <string.h>
void foo(float* restrict grid_ptr, float* restrict tmp_grid_ptr){
  for (int jj = 0; jj < 128; jj++)
  {
    for (int ii = 0; ii < 128; ii++)
    {
      grid_ptr[ii + jj*128] = tmp_grid_ptr[ii + jj*128];
    }
  }
}

int main(){
  float* grid_ptr  = (float*)malloc(sizeof(float) * 128*128);
  float* tmp_grid_ptr  = (float*)malloc(sizeof(float) * 128*128);
  for(int i =0 ;i <100;i++){
    foo(grid_ptr,tmp_grid_ptr);
  }


  return 0;
}
