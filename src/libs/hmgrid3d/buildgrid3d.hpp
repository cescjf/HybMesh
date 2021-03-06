#ifndef  CONSTRUCT_GRID3D_HPP
#define  CONSTRUCT_GRID3D_HPP

#include "serialize3d.hpp"
#include "primitives2d.hpp"

namespace HM3D{ namespace Grid{ namespace Constructor{

//build a cuboid in [0, 0, 0]x[lx, ly, lz] and translate it to leftp.
//boundary types of resulting grid are (for unit cube):
//    x = 0 -> bt = 1
//    x = 1 -> bt = 2
//    y = 0 -> bt = 3
//    y = 1 -> bt = 4
//    z = 0 -> bt = 5
//    z = 1 -> bt = 6
HM3D::GridData Cuboid(HM3D::Vertex leftp, double lx, double ly, double lz, int nx, int ny, int nz);

//sweep xy grid along z vector.
//zcoords represents vector with increasing z coordinate values.
//boundary types:
//   z = zcoords[0] -> bt = 1
//   z = zcoords.back() -> bt = 2
//   all others -> bt= 3
HM3D::GridData SweepGrid2D(const HM2D::GridData& g2d, const vector<double>& zcoords);

//same with supplementary functions defining boundary types
HM3D::GridData SweepGrid2D(const HM2D::GridData& g2d, const vector<double>& zcoords,
		std::function<int(int)> bottom_bt,     //(g2d cell index) -> boundary type
		std::function<int(int)> top_bt,        //(g2d cell index) -> boundary type
		std::function<int(int)> side_bt);      //(g2d edge index) -> boundary type
HM3D::GridData SweepGrid2D(const HM2D::GridData& g2d, const vector<double>& zcoords,
		std::function<int(int)> bottom_bt,     //(g2d cell index) -> boundary type
		std::function<int(int)> top_bt);       //(g2d cell index) -> boundary type
		                                       //side boundary type will be taken from g2d
HM3D::GridData SweepGrid2D(const HM2D::GridData& g2d, const vector<double>& zcoords,
		std::function<int(int)> bottom_bt,     //(g2d cell index) -> boundary type
		std::function<int(int)> top_bt,        //(g2d cell index) -> boundary type
		int side_bt);                          //constant side boundary type


}}}


#endif
