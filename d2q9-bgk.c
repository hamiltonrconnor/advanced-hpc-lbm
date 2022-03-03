/*
** Code to implement a d2q9-bgk lattice boltzmann scheme.
** 'd2' inidates a 2-dimensional grid, and
** 'q9' indicates 9 velocities per grid cell.
** 'bgk' refers to the Bhatnagar-Gross-Krook collision step.
**
** The 'speeds' in each cell are numbered as follows:
**
** 6 2 5
**  \|/
** 3-0-1
**  /|\
** 7 4 8
**
** A 2D grid:
**
**           cols
**       --- --- ---
**      | D | E | F |
** rows  --- --- ---
**      | A | B | C |
**       --- --- ---
**
** 'unwrapped' in row major order to give a 1D array:
**
**  --- --- --- --- --- ---
** | A | B | C | D | E | F |
**  --- --- --- --- --- ---
**
** Grid indicies are:
**
**          ny
**          ^       cols(ii)
**          |  ----- ----- -----
**          | | ... | ... | etc |
**          |  ----- ----- -----
** rows(jj) | | 1,0 | 1,1 | 1,2 |
**          |  ----- ----- -----
**          | | 0,0 | 0,1 | 0,2 |
**          |  ----- ----- -----
**          ----------------------> nx
**
** Note the names of the input parameter and obstacle files
** are passed on the command line, e.g.:
**
**   ./d2q9-bgk input.params obstacles.dat
**
** Be sure to adjust the grid dimensions in the parameter file
** if you choose a different obstacle file.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#define NSPEEDS         9
#define FINALSTATEFILE  "final_state.dat"
#define AVVELSFILE      "av_vels.dat"

/* struct to hold the parameter values */
typedef struct
{
  int    nx;            /* no. of cells in x-direction */
  int    ny;            /* no. of cells in y-direction */
  int    maxIters;      /* no. of iterations */
  int    reynolds_dim;  /* dimension for Reynolds number */
  float density;       /* density per link */
  float accel;         /* density redistribution */
  float omega;         /* relaxation parameter */
} t_param;

/* struct to hold the 'speed' values */
typedef struct
{
  float speeds[NSPEEDS];
} t_speed;

typedef struct
{
  float* s0;
  float* s1;
  float* s2;
  float* s3;
  float* s4;
  float* s5;
  float* s6;
  float* s7;
  float* s8;
} soa;


/*
** function prototypes
*/

/* load params, allocate memory, load obstacles & initialise fluid particle densities */
// int initialise(const char* paramfile, const char* obstaclefile,
//                t_param* params, t_speed** cells_ptr, t_speed** tmp_cells_ptr,
//                int** obstacles_ptr, float** av_vels_ptr);


int initialise(const char* paramfile, const char* obstaclefile,
              t_param* params,
              int** obstacles_ptr, float** av_vels_ptr,soa* grid_ptr,soa* tmp_grid_ptr );
/*
** The main calculation methods.
** timestep calls, in order, the functions:
** accelerate_flow(), propagate(), rebound() & collision()
*/
//float fusion(const t_param params, t_speed** cells_ptr, t_speed** tmp_cells_ptr, int* obstacles);
float fusion(const t_param params,  int* obstacles,soa* restrict grid_ptr,soa* restrict tmp_grid_ptr);

//float fusion(const t_param params, t_speed** cells_ptr, t_speed** tmp_cells_ptr, int* obstacles,soa* grid_ptr,soa* tmp_grid_ptr);
float timestep(const t_param params,  int* obstacles,soa* grid_ptr,soa*tmp_grid_ptr);
//float timestep(const t_param params, t_speed** cells_ptr, t_speed** tmp_cells_ptr, int* obstacles);
//int accelerate_flow(const t_param params, t_speed* cells, int* obstacles);
int accelerate_flow(const t_param params, int* obstacles,soa *grid_ptr);
int propagate(const t_param params, t_speed* cells, t_speed* tmp_cells);
int rebound(const t_param params, t_speed* cells, t_speed* tmp_cells, int* obstacles);
int collision(const t_param params, t_speed* cells, t_speed* tmp_cells, int* obstacles);
int write_values(const t_param params,  int* obstacles, float* av_vels,soa*grid_ptr);

/* finalise, including freeing up allocated memory */
int finalise(const t_param* params, t_speed** cells_ptr, t_speed** tmp_cells_ptr,
             int** obstacles_ptr, float** av_vels_ptr);

/* Sum all the densities in the grid.
** The total should remain constant from one timestep to the next. */
float total_density(const t_param params, t_speed* cells);

/* compute average velocity */
float av_velocity(const t_param params,  int* obstacles,soa* grid_ptr);
//float av_velocity(const t_param params, t_speed* cells, int* obstacles);

/* calculate Reynolds number */
//float calc_reynolds(const t_param params, t_speed* cells, int* obstacles);
float calc_reynolds(const t_param params, int* obstacles,soa* grid );

/* utility functions */
void die(const char* message, const int line, const char* file);
void usage(const char* exe);

/*
** main program:
** initialise, timestep loop, finalise
*/
int main(int argc, char* argv[])
{
  char*    paramfile = NULL;    /* name of the input parameter file */
  char*    obstaclefile = NULL; /* name of a the input obstacle file */
  t_param  params;              /* struct to hold parameter values */
  int*     obstacles = NULL;    /* grid indicating which cells are blocked */
  float* av_vels   = NULL;     /* a record of the av. velocity computed for each timestep */
  struct timeval timstr;                                                             /* structure to hold elapsed time */
  double tot_tic, tot_toc, init_tic, init_toc, comp_tic, comp_toc, col_tic, col_toc; /* floating point numbers to calculate elapsed wallclock time */
  soa grid ;
  soa tmp_grid ;
  /* parse the command line */
  if (argc != 3)
  {
    usage(argv[0]);
  }
  else
  {
    paramfile = argv[1];
    obstaclefile = argv[2];
  }

  /* Total/init time starts here: initialise our data structures and load values from file */
  gettimeofday(&timstr, NULL);
  tot_tic = timstr.tv_sec + (timstr.tv_usec / 1000000.0);
  init_tic=tot_tic;

  initialise(paramfile, obstaclefile, &params,  &obstacles, &av_vels,&grid,&tmp_grid);

  /* Init time stops here, compute time starts*/
  gettimeofday(&timstr, NULL);
  init_toc = timstr.tv_sec + (timstr.tv_usec / 1000000.0);
  comp_tic=init_toc;

  soa* tmp_grid_ptr = &tmp_grid;
  soa* grid_ptr = &grid;
  for (int tt = 0; tt < params.maxIters; tt++)
  {

    av_vels[tt] = timestep(params, obstacles,grid_ptr,tmp_grid_ptr);

    soa* t = grid_ptr;
    grid_ptr = tmp_grid_ptr;
    tmp_grid_ptr = t;
    //av_vels[tt] = av_velocity(params, cells, obstacles);
#ifdef DEBUG
    printf("==timestep: %d==\n", tt);
    printf("av velocity: %.12E\n", av_vels[tt]);
    printf("tot density: %.12E\n", total_density(params, cells));
#endif
  }


  /* Compute time stops here, collate time starts*/
  gettimeofday(&timstr, NULL);
  comp_toc = timstr.tv_sec + (timstr.tv_usec / 1000000.0);
  col_tic=comp_toc;

  // Collate data from ranks here

  /* Total/collate time stops here.*/
  gettimeofday(&timstr, NULL);
  col_toc = timstr.tv_sec + (timstr.tv_usec / 1000000.0);
  tot_toc = col_toc;

  /* write final values and free memory */
  printf("==done==\n");
  printf("Reynolds number:\t\t%.12E\n", calc_reynolds(params,  obstacles,grid_ptr));
  printf("Elapsed Init time:\t\t\t%.6lf (s)\n",    init_toc - init_tic);
  printf("Elapsed Compute time:\t\t\t%.6lf (s)\n", comp_toc - comp_tic);
  printf("Elapsed Collate time:\t\t\t%.6lf (s)\n", col_toc  - col_tic);
  printf("Elapsed Total time:\t\t\t%.6lf (s)\n",   tot_toc  - tot_tic);
  write_values(params,  obstacles, av_vels,grid_ptr);
  //finalise(&params, &cells, &tmp_cells, &obstacles, &av_vels);

  return EXIT_SUCCESS;
}

float timestep(const t_param params,  int* obstacles,soa* grid_ptr,soa*tmp_grid_ptr)
{
  accelerate_flow(params, obstacles,grid_ptr);
  return fusion(params, obstacles,grid_ptr,tmp_grid_ptr);
}

int accelerate_flow(const t_param params,  int* obstacles,soa* grid_ptr)
{

  soa grid = *grid_ptr;

  /* compute weighting factors */
  float w1 = params.density * params.accel / 9.f;
  float w2 = params.density * params.accel / 36.f;

  /* modify the 2nd row of the grid */
  int jj = params.ny - 2;

  for (int ii = 0; ii < params.nx; ii++)
  {
    /* if the cell is not occupied and
    ** we don't send a negative density */

    if (!obstacles[ii + jj*params.nx]
        && (grid.s3[ii + jj*params.nx] - w1) > 0.f
        && (grid.s6[ii + jj*params.nx] - w2) > 0.f
        && (grid.s7[ii + jj*params.nx] - w2) > 0.f)
    {
      /* increase 'east-side' densities */
      grid.s1[ii + jj*params.nx] += w1;
      grid.s5[ii + jj*params.nx] += w2;
      grid.s8[ii + jj*params.nx] += w2;
      /* decrease 'west-side' densities */
      grid.s3[ii + jj*params.nx] -= w1;
      grid.s6[ii + jj*params.nx] -= w2;
      grid.s7[ii + jj*params.nx] -= w2;
    }
  }


  return EXIT_SUCCESS;
}

int propagate(const t_param params, t_speed* cells, t_speed* tmp_cells)
{
  /* loop over _all_ cells */
  for (int jj = 0; jj < params.ny; jj++)
  {
    for (int ii = 0; ii < params.nx; ii++)
    {
      /* determine indices of axis-direction neighbours
      ** respecting periodic boundary conditions (wrap around) */
      int y_n = (jj + 1) % params.ny;
      int x_e = (ii + 1) % params.nx;
      int y_s = (jj == 0) ? (jj + params.ny - 1) : (jj - 1);
      int x_w = (ii == 0) ? (ii + params.nx - 1) : (ii - 1);
      /* propagate densities from neighbouring cells, following
      ** appropriate directions of travel and writing into
      ** scratch space grid */
      tmp_cells[ii + jj*params.nx].speeds[0] = cells[ii + jj*params.nx].speeds[0]; /* central cell, no movement */
      tmp_cells[ii + jj*params.nx].speeds[1] = cells[x_w + jj*params.nx].speeds[1]; /* east */
      tmp_cells[ii + jj*params.nx].speeds[2] = cells[ii + y_s*params.nx].speeds[2]; /* north */
      tmp_cells[ii + jj*params.nx].speeds[3] = cells[x_e + jj*params.nx].speeds[3]; /* west */
      tmp_cells[ii + jj*params.nx].speeds[4] = cells[ii + y_n*params.nx].speeds[4]; /* south */
      tmp_cells[ii + jj*params.nx].speeds[5] = cells[x_w + y_s*params.nx].speeds[5]; /* north-east */
      tmp_cells[ii + jj*params.nx].speeds[6] = cells[x_e + y_s*params.nx].speeds[6]; /* north-west */
      tmp_cells[ii + jj*params.nx].speeds[7] = cells[x_e + y_n*params.nx].speeds[7]; /* south-west */
      tmp_cells[ii + jj*params.nx].speeds[8] = cells[x_w + y_n*params.nx].speeds[8]; /* south-east */
    }
  }

  return EXIT_SUCCESS;
}

int rebound(const t_param params, t_speed* cells, t_speed* tmp_cells, int* obstacles)
{
  /* loop over the cells in the grid */
  for (int jj = 0; jj < params.ny; jj++)
  {
    for (int ii = 0; ii < params.nx; ii++)
    {
      /* if the cell contains an obstacle */
      if (obstacles[jj*params.nx + ii])
      {
        /* called after propagate, so taking values from scratch space
        ** mirroring, and writing into main grid */
        cells[ii + jj*params.nx].speeds[1] = tmp_cells[ii + jj*params.nx].speeds[3];
        cells[ii + jj*params.nx].speeds[2] = tmp_cells[ii + jj*params.nx].speeds[4];
        cells[ii + jj*params.nx].speeds[3] = tmp_cells[ii + jj*params.nx].speeds[1];
        cells[ii + jj*params.nx].speeds[4] = tmp_cells[ii + jj*params.nx].speeds[2];
        cells[ii + jj*params.nx].speeds[5] = tmp_cells[ii + jj*params.nx].speeds[7];
        cells[ii + jj*params.nx].speeds[6] = tmp_cells[ii + jj*params.nx].speeds[8];
        cells[ii + jj*params.nx].speeds[7] = tmp_cells[ii + jj*params.nx].speeds[5];
        cells[ii + jj*params.nx].speeds[8] = tmp_cells[ii + jj*params.nx].speeds[6];
      }
    }
  }

  return EXIT_SUCCESS;
}

int collision(const t_param params, t_speed* cells, t_speed* tmp_cells, int* obstacles)
{
  const float c_sq = 1.f / 3.f; /* square of speed of sound */
  const float w0 = 4.f / 9.f;  /* weighting factor */
  const float w1 = 1.f / 9.f;  /* weighting factor */
  const float w2 = 1.f / 36.f; /* weighting factor */

  /* loop over the cells in the grid
  ** NB the collision step is called after
  ** the propagate step and so values of interest
  ** are in the scratch-space grid */
  for (int jj = 0; jj < params.ny; jj++)
  {
    for (int ii = 0; ii < params.nx; ii++)
    {
      /* don't consider occupied cells */
      if (!obstacles[ii + jj*params.nx])
      {
        /* compute local density total */
        float local_density = 0.f;

        for (int kk = 0; kk < NSPEEDS; kk++)
        {
          local_density += tmp_cells[ii + jj*params.nx].speeds[kk];
        }

        /* compute x velocity component */
        float u_x = (tmp_cells[ii + jj*params.nx].speeds[1]
                      + tmp_cells[ii + jj*params.nx].speeds[5]
                      + tmp_cells[ii + jj*params.nx].speeds[8]
                      - (tmp_cells[ii + jj*params.nx].speeds[3]
                         + tmp_cells[ii + jj*params.nx].speeds[6]
                         + tmp_cells[ii + jj*params.nx].speeds[7]))
                     / local_density;
        /* compute y velocity component */
        float u_y = (tmp_cells[ii + jj*params.nx].speeds[2]
                      + tmp_cells[ii + jj*params.nx].speeds[5]
                      + tmp_cells[ii + jj*params.nx].speeds[6]
                      - (tmp_cells[ii + jj*params.nx].speeds[4]
                         + tmp_cells[ii + jj*params.nx].speeds[7]
                         + tmp_cells[ii + jj*params.nx].speeds[8]))
                     / local_density;

        /* velocity squared */
        float u_sq = u_x * u_x + u_y * u_y;

        /* directional velocity components */
        float u[NSPEEDS];
        u[1] =   u_x;        /* east */
        u[2] =         u_y;  /* north */
        u[3] = - u_x;        /* west */
        u[4] =       - u_y;  /* south */
        u[5] =   u_x + u_y;  /* north-east */
        u[6] = - u_x + u_y;  /* north-west */
        u[7] = - u_x - u_y;  /* south-west */
        u[8] =   u_x - u_y;  /* south-east */

        /* equilibrium densities */
        float d_equ[NSPEEDS];
        /* zero velocity density: weight w0 */
        d_equ[0] = w0 * local_density
                   * (1.f - u_sq / (2.f * c_sq));
        /* axis speeds: weight w1 */
        d_equ[1] = w1 * local_density * (1.f + u[1] / c_sq
                                         + (u[1] * u[1]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        d_equ[2] = w1 * local_density * (1.f + u[2] / c_sq
                                         + (u[2] * u[2]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        d_equ[3] = w1 * local_density * (1.f + u[3] / c_sq
                                         + (u[3] * u[3]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        d_equ[4] = w1 * local_density * (1.f + u[4] / c_sq
                                         + (u[4] * u[4]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        /* diagonal speeds: weight w2 */
        d_equ[5] = w2 * local_density * (1.f + u[5] / c_sq
                                         + (u[5] * u[5]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        d_equ[6] = w2 * local_density * (1.f + u[6] / c_sq
                                         + (u[6] * u[6]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        d_equ[7] = w2 * local_density * (1.f + u[7] / c_sq
                                         + (u[7] * u[7]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));
        d_equ[8] = w2 * local_density * (1.f + u[8] / c_sq
                                         + (u[8] * u[8]) / (2.f * c_sq * c_sq)
                                         - u_sq / (2.f * c_sq));

        /* relaxation step */
        for (int kk = 0; kk < NSPEEDS; kk++)
        {
          cells[ii + jj*params.nx].speeds[kk] = tmp_cells[ii + jj*params.nx].speeds[kk]
                                                  + params.omega
                                                  * (d_equ[kk] - tmp_cells[ii + jj*params.nx].speeds[kk]);
        }
      }
    }
  }

  return EXIT_SUCCESS;
}

float av_velocity(const t_param params, int* obstacles,soa* grid_ptr)
{
  int    tot_cells = 0;  /* no. of cells used in calculation */
  float tot_u;          /* accumulated magnitudes of velocity for each cell */
  soa grid = *grid_ptr;
  /* initialise */
  tot_u = 0.f;

  /* loop over all non-blocked cells */
  for (int jj = 0; jj < params.ny; jj++)
  {
    for (int ii = 0; ii < params.nx; ii++)
    {
      /* ignore occupied cells */
      if (!obstacles[ii + jj*params.nx])
      {
        /* local density total */
        float local_density = grid.s0[ii + jj*params.nx] + grid.s1[ii + jj*params.nx]
                      + grid.s2[ii + jj*params.nx] + grid.s3[ii + jj*params.nx]
                      + grid.s4[ii + jj*params.nx] + grid.s5[ii + jj*params.nx]
                      + grid.s6[ii + jj*params.nx] + grid.s7[ii + jj*params.nx]
                      + grid.s8[ii + jj*params.nx];


       /* x-component of velocity */
       float u_x = (grid.s1[ii + jj*params.nx]
                     + grid.s5[ii + jj*params.nx]
                     + grid.s8[ii + jj*params.nx]
                     - (grid.s3[ii + jj*params.nx]
                        + grid.s6[ii + jj*params.nx]
                        + grid.s7[ii + jj*params.nx]))
                    / local_density;
       /* compute y velocity component */
       float u_y = (grid.s2[ii + jj*params.nx]
                     + grid.s5[ii + jj*params.nx]
                     + grid.s6[ii + jj*params.nx]
                     - (grid.s4[ii + jj*params.nx]
                        + grid.s7[ii + jj*params.nx]
                        + grid.s8[ii + jj*params.nx]))
                    / local_density;
        /* accumulate the norm of x- and y- velocity components */
        tot_u += sqrtf((u_x * u_x) + (u_y * u_y));
        /* increase counter of inspected cells */
        ++tot_cells;
      }
    }
  }

  return tot_u / (float)tot_cells;
}


float fusion(const t_param params,  int* restrict  obstacles,soa* restrict grid_ptr,soa* restrict tmp_grid_ptr)
{

   // soa grid = *grid_ptr;
   // soa tmp_grid = *tmp_grid_ptr;
  //CONSTS FROM COLLISION
  const float c_sq = 1.f / 3.f; /* square of speed of sound */
  const float w0 = 4.f / 9.f;  /* weighting factor */
  const float w1 = 1.f / 9.f;  /* weighting factor */
  const float w2 = 1.f / 36.f; /* weighting factor */

  // grid.s0 = (float*)__builtin_assume_aligned(grid.s0, 64);
  // grid.s1 = (float*)__builtin_assume_aligned(grid.s1, 64);
  // grid.s2 = (float*)__builtin_assume_aligned(grid.s2, 64);
  // grid.s3 = (float*)__builtin_assume_aligned(grid.s3, 64);
  // grid.s4 = (float*)__builtin_assume_aligned(grid.s4, 64);
  // grid.s5 = (float*)__builtin_assume_aligned(grid.s5, 64);
  // grid.s6 = (float*)__builtin_assume_aligned(grid.s6, 64);
  // grid.s7 = (float*)__builtin_assume_aligned(grid.s7, 64);
  // grid.s8 = (float*)__builtin_assume_aligned(grid.s8, 64);
  //
  // tmp_grid.s0 = (float*)__builtin_assume_aligned(tmp_grid.s0, 64);
  // tmp_grid.s1 = (float*)__builtin_assume_aligned(tmp_grid.s1, 64);
  // tmp_grid.s2 = (float*)__builtin_assume_aligned(tmp_grid.s2, 64);
  // tmp_grid.s3 = (float*)__builtin_assume_aligned(tmp_grid.s3, 64);
  // tmp_grid.s4 = (float*)__builtin_assume_aligned(tmp_grid.s4, 64);
  // tmp_grid.s5 = (float*)__builtin_assume_aligned(tmp_grid.s5, 64);
  // tmp_grid.s6 = (float*)__builtin_assume_aligned(tmp_grid.s6, 64);
  // tmp_grid.s7 = (float*)__builtin_assume_aligned(tmp_grid.s7, 64);
  // tmp_grid.s8 = (float*)__builtin_assume_aligned(tmp_grid.s8, 64);
  __assume_aligned((*grid_ptr).s0,64);
  __assume_aligned((*grid_ptr).s1,64);
  __assume_aligned((*grid_ptr).s2,64);
  __assume_aligned((*grid_ptr).s3,64);
  __assume_aligned((*grid_ptr).s4,64);
  __assume_aligned((*grid_ptr).s5,64);
  __assume_aligned((*grid_ptr).s6,64);
  __assume_aligned((*grid_ptr).s7,64);
  __assume_aligned((*grid_ptr).s8,64);

  __assume_aligned((*tmp_grid_ptr).s0,64);
  __assume_aligned((*tmp_grid_ptr).s1,64);
  __assume_aligned((*tmp_grid_ptr).s2,64);
  __assume_aligned((*tmp_grid_ptr).s3,64);
  __assume_aligned((*tmp_grid_ptr).s4,64);
  __assume_aligned((*tmp_grid_ptr).s5,64);
  __assume_aligned((*tmp_grid_ptr).s6,64);
  __assume_aligned((*tmp_grid_ptr).s7,64);
  __assume_aligned((*tmp_grid_ptr).s8,64);






  //t_speed* output = *output_ptr;

  int    tot_cells = 0;  /* no. of cells used in calculation */
  float tot_u;          /* accumulated magnitudes of velocity for each cell */

  /* initialise */
  tot_u = 0.f;

	//Comment asdas
  /* loop over _all_ cells */
  //
    // for(int n=0; n<params.ny*params.nx; n++) {
    //   int ii = n/params.nx; int jj=n%params.nx;
    //#pragma omp parallel for collapse(2) reduction(+:tot_u,tot_cells)
      //#pragma omp simd collapse(2)
      for (int jj = 0; jj < params.ny; jj++)
      {
        for (int ii = 0; ii < params.nx; ii++)
        {
      //printf("%d\n",omp_get_num_threads());
      //propagate(params,cells,tmp_cells,ii,jj);
      //PROPAGATE
      /* determine indices of axis-direction neighbours
      ** respecting periodic boundary conditions (wrap around) */
      int y_n = (jj + 1) % params.ny;
      int x_e = (ii + 1) % params.nx;
      int y_s = (jj == 0) ? (jj + params.ny - 1) : (jj - 1);
      int x_w = (ii == 0) ? (ii + params.nx - 1) : (ii - 1);
      /* propagate densities from neighbouring cells, following
      ** appropriate directions of travel and writing into
      ** scratch space grid */

      (*tmp_grid_ptr).s0[ii + jj*params.nx] = (*grid_ptr).s0[ii + jj*params.nx]; /* central cell, no movement */
      (*tmp_grid_ptr).s1[ii + jj*params.nx] = (*grid_ptr).s1[x_w + jj*params.nx]; /* east */
      (*tmp_grid_ptr).s2[ii + jj*params.nx] = (*grid_ptr).s2[ii + y_s*params.nx]; /* north */
      (*tmp_grid_ptr).s3[ii + jj*params.nx] = (*grid_ptr).s3[x_e + jj*params.nx]; /* west */
      (*tmp_grid_ptr).s4[ii + jj*params.nx] = (*grid_ptr).s4[ii + y_n*params.nx]; /* south */
      (*tmp_grid_ptr).s5[ii + jj*params.nx] = (*grid_ptr).s5[x_w + y_s*params.nx]; /* north-east */
      (*tmp_grid_ptr).s6[ii + jj*params.nx] = (*grid_ptr).s6[x_e + y_s*params.nx]; /* north-west */
      (*tmp_grid_ptr).s7[ii + jj*params.nx] = (*grid_ptr).s7[x_e + y_n*params.nx]; /* south-west */
      (*tmp_grid_ptr).s8[ii + jj*params.nx]= (*grid_ptr).s8[x_w + y_n*params.nx]; /* south-east */


      //REBOUND
      /* if the cell contains an obstacle */
      if (obstacles[jj*params.nx + ii])
      {
        //.rebound(params, output,tmp_cells,ii, jj );
        /* called after propagate, so taking values from scratch space
        ** mirroring, and writing into main grid */



        float t1 = (*tmp_grid_ptr).s1[ii + jj*params.nx];
        float t2 = (*tmp_grid_ptr).s2[ii + jj*params.nx];
        float t3 = (*tmp_grid_ptr).s3[ii + jj*params.nx];
        float t4 = (*tmp_grid_ptr).s4[ii + jj*params.nx];
        float t5 = (*tmp_grid_ptr).s5[ii + jj*params.nx];
        float t6 = (*tmp_grid_ptr).s6[ii + jj*params.nx];
        float t7 = (*tmp_grid_ptr).s7[ii + jj*params.nx];
        float t8 = (*tmp_grid_ptr).s8[ii + jj*params.nx];
        (*tmp_grid_ptr).s1[ii + jj*params.nx] = t3;
        (*tmp_grid_ptr).s2[ii + jj*params.nx] = t4;
        (*tmp_grid_ptr).s3[ii + jj*params.nx] = t1;
        (*tmp_grid_ptr).s4[ii + jj*params.nx] = t2;
        (*tmp_grid_ptr).s5[ii + jj*params.nx] = t7;
        (*tmp_grid_ptr).s6[ii + jj*params.nx] = t8;
        (*tmp_grid_ptr).s7[ii + jj*params.nx] = t5;
        (*tmp_grid_ptr).s8[ii + jj*params.nx] = t6;







      }


      // //COLLISION
      // /* don't consider occupied cells */
      else
      {
      //
      //   /* compute local density total */
      //
      //   const float local_density = (*tmp_grid_ptr).s0[ii + jj*params.nx] + (*tmp_grid_ptr).s1[ii + jj*params.nx]
      //                 + (*tmp_grid_ptr).s2[ii + jj*params.nx] + (*tmp_grid_ptr).s3[ii + jj*params.nx]
      //                 + (*tmp_grid_ptr).s4[ii + jj*params.nx] + (*tmp_grid_ptr).s5[ii + jj*params.nx]
      //                 + (*tmp_grid_ptr).s6[ii + jj*params.nx] + (*tmp_grid_ptr).s7[ii + jj*params.nx]
      //                 + (*tmp_grid_ptr).s8[ii + jj*params.nx];
      //
      //
      //  /* compute x velocity component */
      //  const float u_x = ((*tmp_grid_ptr).s1[ii + jj*params.nx]
      //                + (*tmp_grid_ptr).s5[ii + jj*params.nx]
      //                + (*tmp_grid_ptr).s8[ii + jj*params.nx]
      //                - ((*tmp_grid_ptr).s3[ii + jj*params.nx]
      //                   + (*tmp_grid_ptr).s6[ii + jj*params.nx]
      //                   + (*tmp_grid_ptr).s7[ii + jj*params.nx]))
      //               / local_density;
      //  /* compute y velocity component */
      //  const float u_y = ((*tmp_grid_ptr).s2[ii + jj*params.nx]
      //                + (*tmp_grid_ptr).s5[ii + jj*params.nx]
      //                + (*tmp_grid_ptr).s6[ii + jj*params.nx]
      //                - ((*tmp_grid_ptr).s4[ii + jj*params.nx]
      //                   + (*tmp_grid_ptr).s7[ii + jj*params.nx]
      //                   + (*tmp_grid_ptr).s8[ii + jj*params.nx]))
      //               / local_density;
      //
      //   /* velocity squared */
      //   float u_sq = u_x * u_x + u_y * u_y;
      //
      //   /* directional velocity components */
      //   float u[NSPEEDS];
      //   u[1] =   u_x;        /* east */
      //   u[2] =         u_y;  /* north */
      //   u[3] = - u_x;        /* west */
      //   u[4] =       - u_y;  /* south */
      //   u[5] =   u_x + u_y;  /* north-east */
      //   u[6] = - u_x + u_y;  /* north-west */
      //   u[7] = - u_x - u_y;  /* south-west */
      //   u[8] =   u_x - u_y;  /* south-east */
      //
        /* equilibrium densities */
         float d_equ[NSPEEDS];
          d[0] = 1.0f;
          d[1] = 1.0f;
          d[2] = 1.0f;
          d[3] = 1.0f;
          d[4] = 1.0f;
          d[5] = 1.0f;
          d[6] = 1.0f;
          d[7] = 1.0f;
          d[8] = 1.0f;
      //   /* zero velocity density: weight w0 */
      //   d_equ[0] = w0 * local_density
      //              * (1.f - u_sq / (2.f * c_sq));
      //   /* axis speeds: weight w1 */
      //   // d_equ[1] = w1 * local_density *
      //   // (1.f + (u[1] / c_sq )+ ((u[1] * u[1]) / (2.f * c_sq * c_sq)) - (u_sq / (2.f * c_sq)));
      //
      //   //printf("%f\n",w1 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[1])+(u[1]*u[1])-(u_sq*c_sq))/(2.f*c_sq*c_sq));
      //   for(int i = 1;i<9;i++){
      //     if(i<5){
      //     d_equ[i] = w1 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[i])+(u[i]*u[i])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //     }else{
      //       d_equ[i] = w2 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[i])+(u[i]*u[i])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //     }
      //   }
      //
      //
      //   // d_equ[2] = w1 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[2])+(u[2]*u[2])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //   // d_equ[3] = w1 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[3])+(u[3]*u[3])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //   // d_equ[4] = w1 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[4])+(u[4]*u[4])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //   // d_equ[5] = w2 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[5])+(u[5]*u[5])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //   // d_equ[6] = w2 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[6])+(u[6]*u[6])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //   // d_equ[7] = w2 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[7])+(u[7]*u[7])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //   // d_equ[8] = w2 *local_density *((2.f*c_sq*c_sq)+(2.f*c_sq*u[8])+(u[8]*u[8])-(u_sq*c_sq))/(2.f*c_sq*c_sq);
      //
      //
      //   /* local density total */
        float av_local_density = 0.f;
        /* relaxation step */

        float outVal = (*tmp_grid_ptr).s0[ii + jj*params.nx]
                        + params.omega * (d_equ[0] - (*tmp_grid_ptr).s0[ii + jj*params.nx]);
        (*tmp_grid_ptr).s0[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s1[ii + jj*params.nx]
                        + params.omega * (d_equ[1] - (*tmp_grid_ptr).s1[ii + jj*params.nx]);
        (*tmp_grid_ptr).s1[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s2[ii + jj*params.nx]
                        + params.omega * (d_equ[2] - (*tmp_grid_ptr).s2[ii + jj*params.nx]);
        (*tmp_grid_ptr).s2[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s3[ii + jj*params.nx]
                        + params.omega * (d_equ[3] - (*tmp_grid_ptr).s3[ii + jj*params.nx]);
        (*tmp_grid_ptr).s3[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s4[ii + jj*params.nx]
                        + params.omega * (d_equ[4] - (*tmp_grid_ptr).s4[ii + jj*params.nx]);
        (*tmp_grid_ptr).s4[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s5[ii + jj*params.nx]
                        + params.omega * (d_equ[5] - (*tmp_grid_ptr).s5[ii + jj*params.nx]);
        (*tmp_grid_ptr).s5[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s6[ii + jj*params.nx]
                        + params.omega * (d_equ[6] - (*tmp_grid_ptr).s6[ii + jj*params.nx]);
        (*tmp_grid_ptr).s6[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s7[ii + jj*params.nx]
                        + params.omega * (d_equ[7] - (*tmp_grid_ptr).s7[ii + jj*params.nx]);
        (*tmp_grid_ptr).s7[ii + jj*params.nx] = outVal;
        av_local_density += outVal;

        outVal = (*tmp_grid_ptr).s8[ii + jj*params.nx]
                        + params.omega * (d_equ[8] - (*tmp_grid_ptr).s8[ii + jj*params.nx]);
        (*tmp_grid_ptr).s8[ii + jj*params.nx] = outVal;
        av_local_density += outVal;




       const float av_u_x = ((*tmp_grid_ptr).s1[ii + jj*params.nx]
                     + (*tmp_grid_ptr).s5[ii + jj*params.nx]
                     + (*tmp_grid_ptr).s8[ii + jj*params.nx]
                     - ((*tmp_grid_ptr).s3[ii + jj*params.nx]
                        + (*tmp_grid_ptr).s6[ii + jj*params.nx]
                        + (*tmp_grid_ptr).s7[ii + jj*params.nx]))
                    / av_local_density;
       /* compute y velocity component */
       const float av_u_y = ((*tmp_grid_ptr).s2[ii + jj*params.nx]
                     + (*tmp_grid_ptr).s5[ii + jj*params.nx]
                     + (*tmp_grid_ptr).s6[ii + jj*params.nx]
                     - ((*tmp_grid_ptr).s4[ii + jj*params.nx]
                        + (*tmp_grid_ptr).s7[ii + jj*params.nx]
                        + (*tmp_grid_ptr).s8[ii + jj*params.nx]))
                    / av_local_density;



        /* accumulate the norm of x- and y- velocity components */
        tot_u += sqrtf((av_u_x * av_u_x) + (av_u_y * av_u_y));
        /* increase counter of inspected cells */
        ++tot_cells;


      }
    }
    }




    return tot_u / (float)tot_cells;






return EXIT_SUCCESS;


}

int initialise(const char* paramfile, const char* obstaclefile,
               t_param* params,
               int** obstacles_ptr, float** av_vels_ptr,soa* grid_ptr,soa* tmp_grid_ptr )
{
  char   message[1024];  /* message buffer */
  FILE*   fp;            /* file pointer */
  int    xx, yy;         /* generic array indices */
  int    blocked;        /* indicates whether a cell is blocked by an obstacle */
  int    retval;         /* to hold return value for checking */

  /* open the parameter file */
  fp = fopen(paramfile, "r");

  if (fp == NULL)
  {
    sprintf(message, "could not open input parameter file: %s", paramfile);
    die(message, __LINE__, __FILE__);
  }

  /* read in the parameter values */
  retval = fscanf(fp, "%d\n", &(params->nx));

  if (retval != 1) die("could not read param file: nx", __LINE__, __FILE__);

  retval = fscanf(fp, "%d\n", &(params->ny));

  if (retval != 1) die("could not read param file: ny", __LINE__, __FILE__);

  retval = fscanf(fp, "%d\n", &(params->maxIters));

  if (retval != 1) die("could not read param file: maxIters", __LINE__, __FILE__);

  retval = fscanf(fp, "%d\n", &(params->reynolds_dim));

  if (retval != 1) die("could not read param file: reynolds_dim", __LINE__, __FILE__);

  retval = fscanf(fp, "%f\n", &(params->density));

  if (retval != 1) die("could not read param file: density", __LINE__, __FILE__);

  retval = fscanf(fp, "%f\n", &(params->accel));

  if (retval != 1) die("could not read param file: accel", __LINE__, __FILE__);

  retval = fscanf(fp, "%f\n", &(params->omega));

  if (retval != 1) die("could not read param file: omega", __LINE__, __FILE__);

  /* and close up the file */
  fclose(fp);

  /*
  ** Allocate memory.
  **
  ** Remember C is pass-by-value, so we need to
  ** pass pointers into the initialise function.
  **
  ** NB we are allocating a 1D array, so that the
  ** memory will be contiguous.  We still want to
  ** index this memory as if it were a (row major
  ** ordered) 2D array, however.  We will perform
  ** some arithmetic using the row and column
  ** coordinates, inside the square brackets, when
  ** we want to access elements of this array.
  **
  ** Note also that we are using a structure to
  ** hold an array of 'speeds'.  We will allocate
  ** a 1D array of these structs.
  */


  (*grid_ptr).s0 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s1 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s2 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s3 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s4 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s5 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s6 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s7 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*grid_ptr).s8 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  // soa tmp_grid;
  // tmp_grid_ptr = &tmp_grid;
  (*tmp_grid_ptr).s0 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s1 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s2 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s3 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s4 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s5 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s6 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s7 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);
  (*tmp_grid_ptr).s8 = (float*)_mm_malloc(sizeof(float) * (params->ny * params->nx),64);

  /* initialise densities */
  float w0 = params->density * 4.f / 9.f;
  float w1 = params->density      / 9.f;
  float w2 = params->density      / 36.f;
    //#pragma omp parallel for collapse(2)
  for (int jj = 0; jj < params->ny; jj++)
  {
    for (int ii = 0; ii < params->nx; ii++)
    {
      /* centre */
      (*grid_ptr).s0[ii + jj*params->nx] = w0;
      /* axis directions */
      (*grid_ptr).s1[ii + jj*params->nx] = w1;
      (*grid_ptr).s2[ii + jj*params->nx] = w1;
      (*grid_ptr).s3[ii + jj*params->nx] = w1;
      (*grid_ptr).s4[ii + jj*params->nx] = w1;
      /* diagonals */
      (*grid_ptr).s5[ii + jj*params->nx] = w2;
      (*grid_ptr).s6[ii + jj*params->nx] = w2;
      (*grid_ptr).s7[ii + jj*params->nx] = w2;
      (*grid_ptr).s8[ii + jj*params->nx] = w2;
    }
  }


  /* the map of obstacles */
  *obstacles_ptr = malloc(sizeof(int) * (params->ny * params->nx));

  if (*obstacles_ptr == NULL) die("cannot allocate column memory for obstacles", __LINE__, __FILE__);






  /* first set all cells in obstacle array to zero */
  for (int jj = 0; jj < params->ny; jj++)
  {
    for (int ii = 0; ii < params->nx; ii++)
    {
      (*obstacles_ptr)[ii + jj*params->nx] = 0;
    }
  }

  /* open the obstacle data file */
  fp = fopen(obstaclefile, "r");

  if (fp == NULL)
  {
    sprintf(message, "could not open input obstacles file: %s", obstaclefile);
    die(message, __LINE__, __FILE__);
  }

  /* read-in the blocked cells list */
  while ((retval = fscanf(fp, "%d %d %d\n", &xx, &yy, &blocked)) != EOF)
  {
    /* some checks */
    if (retval != 3) die("expected 3 values per line in obstacle file", __LINE__, __FILE__);

    if (xx < 0 || xx > params->nx - 1) die("obstacle x-coord out of range", __LINE__, __FILE__);

    if (yy < 0 || yy > params->ny - 1) die("obstacle y-coord out of range", __LINE__, __FILE__);

    if (blocked != 1) die("obstacle blocked value should be 1", __LINE__, __FILE__);

    /* assign to array */
    (*obstacles_ptr)[xx + yy*params->nx] = blocked;
  }

  /* and close the file */
  fclose(fp);

  /*
  ** allocate space to hold a record of the avarage velocities computed
  ** at each timestep
  */
  *av_vels_ptr = (float*)malloc(sizeof(float) * params->maxIters);

  return EXIT_SUCCESS;
}

int finalise(const t_param* params, t_speed** cells_ptr, t_speed** tmp_cells_ptr,
             int** obstacles_ptr, float** av_vels_ptr)
{
  /*
  ** free up allocated memory
  */
  free(*cells_ptr);
  *cells_ptr = NULL;

  free(*tmp_cells_ptr);
  *tmp_cells_ptr = NULL;

  free(*obstacles_ptr);
  *obstacles_ptr = NULL;

  free(*av_vels_ptr);
  *av_vels_ptr = NULL;

  return EXIT_SUCCESS;
}


float calc_reynolds(const t_param params, int* obstacles,soa* grid )
{
  const float viscosity = 1.f / 6.f * (2.f / params.omega - 1.f);

  return av_velocity(params,  obstacles,grid) * params.reynolds_dim / viscosity;
}

float total_density(const t_param params, t_speed* cells)
{
  float total = 0.f;  /* accumulator */

  for (int jj = 0; jj < params.ny; jj++)
  {
    for (int ii = 0; ii < params.nx; ii++)
    {
      for (int kk = 0; kk < NSPEEDS; kk++)
      {
        total += cells[ii + jj*params.nx].speeds[kk];
      }
    }
  }

  return total;
}

int write_values(const t_param params, int* obstacles, float* av_vels,soa* grid_ptr)
{
  soa grid = *grid_ptr;
  FILE* fp;                     /* file pointer */
  const float c_sq = 1.f / 3.f; /* sq. of speed of sound */
  float local_density;         /* per grid cell sum of densities */
  float pressure;              /* fluid pressure in grid cell */
  float u_x;                   /* x-component of velocity in grid cell */
  float u_y;                   /* y-component of velocity in grid cell */
  float u;                     /* norm--root of summed squares--of u_x and u_y */

  fp = fopen(FINALSTATEFILE, "w");

  if (fp == NULL)
  {
    die("could not open file output file", __LINE__, __FILE__);
  }

  for (int jj = 0; jj < params.ny; jj++)
  {
    for (int ii = 0; ii < params.nx; ii++)
    {
      /* an occupied cell */
      if (obstacles[ii + jj*params.nx])
      {
        u_x = u_y = u = 0.f;
        pressure = params.density * c_sq;
      }
      /* no obstacle */
      else
      {

        local_density = grid.s0[ii + jj*params.nx] + grid.s1[ii + jj*params.nx]
                      + grid.s2[ii + jj*params.nx] + grid.s3[ii + jj*params.nx]
                      + grid.s4[ii + jj*params.nx] + grid.s5[ii + jj*params.nx]
                      + grid.s6[ii + jj*params.nx] + grid.s7[ii + jj*params.nx]
                      + grid.s8[ii + jj*params.nx];


        u_x = (grid.s1[ii + jj*params.nx]
                      + grid.s5[ii + jj*params.nx]
                      + grid.s8[ii + jj*params.nx]
                      - (grid.s3[ii + jj*params.nx]
                         + grid.s6[ii + jj*params.nx]
                         + grid.s7[ii + jj*params.nx]))
                     / local_density;
        /* compute y velocity component */
        u_y = (grid.s2[ii + jj*params.nx]
                      + grid.s5[ii + jj*params.nx]
                      + grid.s6[ii + jj*params.nx]
                      - (grid.s4[ii + jj*params.nx]
                         + grid.s7[ii + jj*params.nx]
                         + grid.s8[ii + jj*params.nx]))
                     / local_density;
        /* compute norm of velocity */
        u = sqrtf((u_x * u_x) + (u_y * u_y));
        /* compute pressure */
        pressure = local_density * c_sq;
      }

      /* write to file */
      fprintf(fp, "%d %d %.12E %.12E %.12E %.12E %d\n", ii, jj, u_x, u_y, u, pressure, obstacles[ii * params.nx + jj]);
    }
  }

  fclose(fp);

  fp = fopen(AVVELSFILE, "w");

  if (fp == NULL)
  {
    die("could not open file output file", __LINE__, __FILE__);
  }

  for (int ii = 0; ii < params.maxIters; ii++)
  {
    fprintf(fp, "%d:\t%.12E\n", ii, av_vels[ii]);
  }

  fclose(fp);

  return EXIT_SUCCESS;
}

void die(const char* message, const int line, const char* file)
{
  fprintf(stderr, "Error at line %d of file %s:\n", line, file);
  fprintf(stderr, "%s\n", message);
  fflush(stderr);
  exit(EXIT_FAILURE);
}

void usage(const char* exe)
{
  fprintf(stderr, "Usage: %s <paramfile> <obstaclefile>\n", exe);
  exit(EXIT_FAILURE);
}
