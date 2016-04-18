#ifndef HYBMESH_HMCPORT_GRID3D_H
#define HYBMESH_HMCPORT_GRID3D_H

#include "crossgrid.h"
#include "hmcport.h"
#include "cport_grid2d.h"

extern "C"{

struct CPortGrid3D{
	void* grid;
	mutable void* serialized;
};

//====== destructor
void free_grid3d(CPortGrid3D*);

//====== sweep constructors
//construct by sweep in z direction
//algo_top/algo_bot = 0 - constant value taken from btop[0]
//                  = 1 - variable for each face from i-th cell as btop[i]
//algo_side = 0 - constant taken from bside
//            1 - take value from boundary struct
CPortGrid3D* grid2_sweep_z(const Grid* g, const Grid2DBoundaryStruct* bc,
		int nz, double* zvals,
		int algo_top, int* btop,
		int algo_bot, int* bbot,
		int algo_side, int bside);

//vec - [x0, y0, x1, y1] array defining vector of rotation
//phi[n_phi] - increasing vector of angular partition (degree)
//b1, b2 - boundary types for surfaces at minimum and maximum phi's
//is_trian (bool) - whether to triangulate center cell
//return NULL if failed
CPortGrid3D* grid2_revolve(Grid* g, double* vec, int n_phi, double* phi,
		Grid2DBoundaryStruct* bc,
		int b1, int b2, int is_trian);


//====== exporters
//returns 0 on success
int export_vtk_grid3(const CPortGrid3D* grid, const char* fname, hmcport_callback f2);
int export_surface_vtk_grid3(const CPortGrid3D* grid, const char* fname, hmcport_callback f2);
int export_msh_grid3(const CPortGrid3D* grid, const char* fname, const BoundaryNamesStruct* bnames,
		int n_periodic, double* data_periodic, hmcport_callback f2);
int export_tecplot_grid3(const CPortGrid3D* grid, const char* fname, const BoundaryNamesStruct* bnames,
		hmcport_callback f2);



}

#endif