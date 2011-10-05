/* ----------------------------------------------------------------------
   miniMD is a simple, parallel molecular dynamics (MD) code.   miniMD is
   an MD microapplication in the Mantevo project at Sandia National 
   Laboratories ( http://software.sandia.gov/mantevo/ ). The primary 
   authors of miniMD are Steve Plimpton and Paul Crozier 
   (pscrozi@sandia.gov).

   Copyright (2008) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This library is free software; you 
   can redistribute it and/or modify it under the terms of the GNU Lesser 
   General Public License as published by the Free Software Foundation; 
   either version 3 of the License, or (at your option) any later 
   version.
  
   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
   Lesser General Public License for more details.
    
   You should have received a copy of the GNU Lesser General Public 
   License along with this software; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA.  See also: http://www.gnu.org/licenses/lgpl.txt .

   For questions, contact Paul S. Crozier (pscrozi@sandia.gov). 

   Please read the accompanying README and LICENSE files.
---------------------------------------------------------------------- */

#include "stdio.h"
#include "stdlib.h"
#include "mpi.h"

#include "ljs.h"
#include "atom.h"
#include "force.h"
#include "neighbor.h"
#include "integrate.h"
#include "thermo.h"
#include "comm.h"
#include "timer.h"

#define MAXLINE 256

int input(In &, Atom &, Force &, Neighbor &, Integrate &, Thermo &);
void create_box(Atom &, int, int, int, double);
int create_atoms(Atom &, int, int, int, double);
void create_velocity(double, Atom &, Thermo &);
void output(In &, Atom &, Force &, Neighbor &, Comm &,
	    Thermo &, Integrate &, Timer &);
int prompt();

int main(int argc, char **argv)
{
  int me;
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&me);

  In in;
  Atom atom;
  Force force;
  Neighbor neighbor;
  Integrate integrate;
  Thermo thermo;
  Comm comm;
  Timer timer;

  int flag;
  while (1) {

    flag = input(in,atom,force,neighbor,integrate,thermo);
    if (flag && prompt()) continue;

    if (me == 0) printf("Setting up ...\n");

    create_box(atom,in.nx,in.ny,in.nz,in.rho);
    flag = comm.setup(neighbor.cutneigh,atom);
    if (flag && prompt()) continue;
    flag = neighbor.setup(atom);
    if (flag && prompt()) continue;

    integrate.setup();
    force.setup();
    thermo.setup(in.rho,integrate.ntimes);

    flag = create_atoms(atom,in.nx,in.ny,in.nz,in.rho);
    if (flag && prompt()) continue;
    create_velocity(in.t_request,atom,thermo);

    comm.exchange(atom);
    comm.borders(atom);
    neighbor.build(atom);
    thermo.compute(0,atom,neighbor,force);

    if (me == 0) printf("Starting dynamics ...\n");
          
    timer.barrier_start(TIME_TOTAL);
    integrate.run(atom,force,neighbor,comm,thermo,timer);
    timer.barrier_stop(TIME_TOTAL);

    thermo.compute(-1,atom,neighbor,force);
    output(in,atom,force,neighbor,comm,thermo,integrate,timer);
    prompt();
  }
}

int prompt()
{
  int me;
  MPI_Comm_rank(MPI_COMM_WORLD,&me);

  int flag;
  if (me == 0) {
    printf("(0) Stop, (1) Continue\n");
    char line[MAXLINE];
    fgets(line,MAXLINE,stdin);
    sscanf(line,"%d",&flag);
  }
  MPI_Bcast(&flag,1,MPI_INT,0,MPI_COMM_WORLD);
  if (flag == 0) {
    MPI_Finalize();
    exit(0);
  }
  return 1;
}
