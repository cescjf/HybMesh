#include "export_grid3d.hpp"
#include <unordered_map>
#include <fstream>
#include <sstream>
#include "surface_grid3d.hpp"
#include "debug_grid3d.hpp"

using namespace HMGrid3D;
namespace hme = HMGrid3D::Export;

namespace{

std::string to_hex(int a){
	std::stringstream os;
	os<<std::hex<<a;
	return os.str();
}

char msh_face_type(HMGrid3D::Face& f){
	int ne = f.n_edges();
	if (ne == 3) return '3';
	if (ne == 4) return '4';
	return '5';
}

char msh_cell_type(HMGrid3D::Cell& c){
	int nf = c.n_faces();
	int ne = c.n_edges();
	int nv = c.n_vertices();
	//tetrahedral
	if (nf == 4 && ne == 6 && nv == 4)
		return '2';
	//hexahedral
	if (nf == 6 && ne == 12 && nv == 8)
		return '4';
	//pyramid
	if (nf == 5 && ne == 8 && nv == 5)
		return '5';
	//wedge
	if (nf == 5 && ne == 9 && nv == 6)
		return '6';
	//mixed
	return '7';
}

std::map<int, ShpVector<Face>> faces_by_btype(const HMGrid3D::Grid& g){
	std::map<int, ShpVector<Face>> ret;
	//interior zone has always be there
	ret.emplace(std::numeric_limits<int>::min(), ShpVector<Face>());
	for (auto f: g.allfaces()){
		int tp = (f->is_boundary()) ? f->boundary_type : std::numeric_limits<int>::min();
		auto emp = ret.emplace(tp, ShpVector<Face>());
		emp.first->second.push_back(f);
	}
	return ret;
}
std::vector<std::pair<int, int>> left_right_cells(const ShpVector<Face>& data, const ShpVector<HMGrid3D::Cell>& cells){
	std::vector<std::pair<int, int>> ret; ret.reserve(data.size());
	std::unordered_map<HMGrid3D::Cell*, int> cell_ind;
	int i=0;
	for (auto c: cells) cell_ind.emplace(c.get(), i++);
	for (auto& f: data){
		ret.push_back(std::make_pair(-1, -1));
		if (f->right) ret.back().first = cell_ind[f->right.get()];
		if (f->left) ret.back().second = cell_ind[f->left.get()];
	}
	return ret;
}

std::vector<std::vector<int>> int_face_vertices(const ShpVector<Face>& data, const ShpVector<Vertex>& vert){
	std::vector<std::vector<int>> ret; ret.reserve(data.size());
	std::unordered_map<Vertex*, int> vert_ind;
	int i=0;
	for (auto p: vert) vert_ind.emplace(p.get(), i++); 
	for (auto& f: data){
		ret.push_back(vector<int>());
		for (auto p: f->sorted_vertices()){
			ret.back().push_back(vert_ind[p.get()]);
		}
	}
	return ret;
}
char get_common_type(vector<char>::iterator istart, vector<char>::iterator iend){
	for (auto it=istart; it!=iend; ++it) if (*it != *istart) return '0';
	return *istart;
}
typedef std::map<
	std::pair<int, int>,
	std::vector<std::pair<int, int>>
> PeriodicMap;
PeriodicMap assemble_periodic(const std::map<int, ShpVector<Face>>& fzones, const std::map<Face*, Face*>& pfaces){
	std::map<std::pair<int, int>,
		std::vector<std::pair<int, int>>> ret;
	auto perface_index = [&](Face* f){
		int ret = 0;
		auto fnd1 = fzones.find(f->boundary_type);
		for (auto it=fzones.begin(); it!= fnd1; ++it){
			ret += it->second.size();
		}
		auto fnd3 = aa::shp_find(fnd1->second.begin(), fnd1->second.end(), f);
		return ret + fnd3 - (fnd1->second.begin());
	};
	std::map<int, int> btype_zone;
	int zt = 4;
	for (auto it=++fzones.begin(); it!=fzones.end(); ++it){
		btype_zone[it->first] = zt++;
	}
	for (auto& ff: pfaces){
		int perzone = btype_zone[ff.first->boundary_type];
		int shazone = btype_zone[ff.second->boundary_type];
		int perindex = perface_index(ff.first);
		int shaindex = perface_index(ff.second);
		auto emp = ret.emplace(
				std::make_pair(perzone, shazone),
				std::vector<std::pair<int, int>>());
		emp.first->second.emplace_back(perindex, shaindex);
	}
	return ret;
}

void zones_names(const PeriodicMap& periodic,
		const std::map<int, ShpVector<Face>>& fzones,
		std::map<int, std::string>& btypes,
		std::map<int, std::string>& bnames){

	std::set<int> periodic_zones, pershadow_zones;
	for (auto& d: periodic){
		int zt1 = d.first.first, zt2 = d.first.second;
		periodic_zones.insert(zt1);
		pershadow_zones.insert(zt2);
	}

	btypes[3] = to_hex(2);
	bnames[3] = "interior";
	int zt = 4;
	for (auto it = ++fzones.begin(); it!=fzones.end(); ++it){
		if (periodic_zones.find(zt) != periodic_zones.end()){
			btypes[zt] = to_hex(12);
			bnames[zt] = "periodic";
		} else if (pershadow_zones.find(zt) != pershadow_zones.end()){
			btypes[zt] = to_hex(8);
			bnames[zt] = "periodic-shadow";
		} else {
			btypes[zt] = to_hex(3);
			bnames[zt] = "wall";
		}
		++zt;
	}
}

//save to fluent main function
void gridmsh(const HMGrid3D::Grid& g, const char* fn,
		std::function<std::string(int)> btype_name,
		std::map<Face*, Face*> pfaces=std::map<Face*, Face*>()){
	if (g.n_cells() == 0) throw std::runtime_error("Exporting blank grid");
	//Zones:
	//1    - verticies default
	//2    - fluid for cells
	//3    - default interior
	//4..N - bc's faces

	// ===== Needed data
	auto vert = g.allvertices();
	//* faces by boundary_type grouped by zones
	std::map<int, ShpVector<Face>> facezones = faces_by_btype(g);
	//* faces as they would be in resulting file
	ShpVector<Face> allfaces;
	for (auto& it: facezones) allfaces.insert(allfaces.end(), it.second.begin(), it.second.end());
	//* left/right cell indicies for each face in integer representation
	std::vector<std::pair<int, int>> face_adjacents = left_right_cells(allfaces, g.allcells()); 
	//* face->vertices connectivity in integer representation
	std::vector<std::vector<int>> face_vertices = int_face_vertices(allfaces, vert);
	//* cells, faces types for each entry;
	std::vector<char> ctypes;   //cell types array
	std::vector<char> ftypes;   //face types for each zone
	for (auto& c: g.allcells()) ctypes.push_back(msh_cell_type(*c));
	for (auto& f: allfaces)     ftypes.push_back(msh_face_type(*f));
        //* common cell types or '0' if they differ, common face type for each zone
	char cell_common_type = get_common_type(ctypes.begin(), ctypes.end());
	std::vector<char> facezones_common_type;
	auto ftypes_it = ftypes.begin();
	for (auto& v: facezones){
		auto itnext = ftypes_it + v.second.size();
		facezones_common_type.push_back(get_common_type(ftypes_it, itnext));
		ftypes_it = itnext;
	}
	//* <periodic zonetype, shadow zonetype> -> vector of periodic face, shadow face>
	PeriodicMap periodic_data = assemble_periodic(facezones, pfaces);
	//* name and type of each zone by zone id: 2-interior, 3-wall, 8-periodic-shadow, 12-periodic
	std::map<int, std::string> facezones_btype, facezones_bname;
	zones_names(periodic_data, facezones, facezones_btype, facezones_bname);
	
	//=========== Write to file
	std::ofstream fs(fn);
	fs.precision(17);
	//header
	fs<<"(0 \"HybMesh to Fluent File\")\n(0 \"Dimensions\")\n(2 3)\n";

	//Vertices: Zone 1
	fs<<"(10 (0 1 "<<to_hex(vert.size())<<" 0 3))\n";
	fs<<"(10 (1 1 "<<to_hex(vert.size())<<" 1 3)(\n";
	for (auto p: vert){
		fs<<p->x<<" "<<p->y<<" "<<p->z<<"\n";
	}
	fs<<"))\n";

	//Cells: Zone 2
	fs<<"(12 (0 1 "<<to_hex(g.n_cells())<<" 0))\n";
	fs<<"(12 (2 1 "<<to_hex(g.n_cells())<<" 1 "<<cell_common_type<<")";
	if (cell_common_type != '0') fs<<")\n";
	else {
		fs<<"(\n";
		for (auto s: ctypes) fs<<s<<" ";
		fs<<"\n))\n";
	}

	//Faces: Zones 3+it
	fs<<"(13 (0 1 "<<to_hex(allfaces.size())<<" 0))\n";
	int zonetype = 3;
	int iface = 0;
	int it = 0;
	for (auto& fz: facezones){
		if (fz.second.size() == 0) { ++it; ++zonetype; continue;}
		std::string first_index = to_hex(iface+1);
		std::string last_index = to_hex(iface+fz.second.size());
		fs<<"(13 ("<<to_hex(zonetype)<<" "<<first_index<<" "<<last_index<<" ";
		fs<<facezones_btype[zonetype]<<" "<<facezones_common_type[it]<<")(\n";
		for (auto f: fz.second){
			if (facezones_common_type[it] == '0' ||
			    facezones_common_type[it] == '5')
				fs<<to_hex(face_vertices[iface].size())<<" ";
			for (auto i: face_vertices[iface]) fs<<to_hex(i+1)<<" ";
			fs<<to_hex(face_adjacents[iface].first+1)<<" "<<to_hex(face_adjacents[iface].second+1);
			fs<<"\n";
			++iface;
		}
		fs<<"))\n";
		++it; ++zonetype;
	}
	//Periodic
	if (pfaces.size() > 0){
		int it = 0;
		for (auto& pd: periodic_data){
			int sz = pd.second.size();
			fs<<"(18 ("<<to_hex(it+1)<<" "<<to_hex(it+sz)<<" ";
			fs<<to_hex(pd.first.first)<<" "<<to_hex(pd.first.second)<<")(\n";
			for (auto& v: pd.second){
				fs<<to_hex(v.first+1)<<" "<<to_hex(v.second+1)<<"\n";
			}
			fs<<")\n";
			it += sz;
		}
	}

	//Boundary features
	fs<<"(45 (2 fluid fluid)())\n";
	fs<<"(45 (3 interior default-interior)())\n";
	zonetype = 4;
	for (auto it = std::next(facezones.begin()); it!=facezones.end(); ++it){
		fs<<"(45 ("<<zonetype<<" "<<facezones_bname[zonetype]<<" "<<btype_name(it->first)<<")())\n";
		++zonetype;
	}
}

}

void hme::PeriodicDataEntry::assemble(HMGrid3D::Grid& g, std::map<Face*, Face*>& outmap, int trynum){
	HMGrid3D::Surface periodic_surf = HMGrid3D::Surface::FromBoundaryType(g, bt, 1);
	int dir = ((reversed && trynum == 0) || (!reversed && trynum == 1) ) ? -1 : 1;
	HMGrid3D::Surface shadow_surf = HMGrid3D::Surface::FromBoundaryType(g, bt_shadow, dir);
	//this (along with not null dir) guaranties that all bondary edges
	//would have same direction that is needed to call ExtractBoundary
	for (auto f: periodic_surf.allfaces()) f->correct_edge_directions();
	for (auto f: shadow_surf.allfaces()) f->correct_edge_directions();

	//boundary
	ShpVector<Edge> bnd1 = Surface::ExtractBoundary(periodic_surf, v);
	ShpVector<Edge> bnd2 = Surface::ExtractBoundary(shadow_surf, v_shadow);
	//boundary sizes should be equal
	if (bnd1.size() != bnd2.size() || bnd1.size() == 0)
		throw std::runtime_error("Periodic merging failed");

	//subsurface bounded by bnd1, bnd2 (if initial surface are multiply connected)
	periodic_surf = HMGrid3D::Surface::SubSurface(periodic_surf, bnd1[0]->vertices[0].get());
	shadow_surf = HMGrid3D::Surface::SubSurface(shadow_surf, bnd2[0]->vertices[0].get());

	//Rearranging to match topology
	HMGrid3D::Surface::FaceRearrange(periodic_surf, bnd1[0].get());
	HMGrid3D::Surface::FaceRearrange(shadow_surf, bnd2[0].get());

	//check topology: if ok then map surface else try reverse direction
	if (HMGrid3D::Surface::MatchTopology(periodic_surf, shadow_surf)){
		auto it1 = periodic_surf.faces.begin();
		auto it2 = shadow_surf.faces.begin();
		while (it1 != periodic_surf.faces.end()){
			outmap[(*it1++).get()] = (*it2++).get();
		}
	} else {
		if (trynum == 0){
			//try reverse
			std::string tp = (reversed) ? "reversed" : "direct";
			std::cout<<"Warning: Face mesh topology doesn't fit for "<<tp<<" ordering ";
			std::cout<<"of periodical merging"<<std::endl;
			std::cout<<"Trying reversed ... "<<std::endl;
			assemble(g, outmap, 1);
		} else {
			//reversed was tried and also failed
			throw std::runtime_error("Periodic merging failed");
		}
	}
}

HMGrid3D::Grid hme::PeriodicData::assemble(const HMGrid3D::Grid& g,
		std::map<Face*, Face*>& outmap){
	if (size() == 0) return g;
	HMGrid3D::Grid ret = HMGrid3D::Constructor::Copy::ShallowVertices(g);
	for (auto& d: data) d.assemble(ret, outmap);
	return ret;
}

void hme::GridMSH(const HMGrid3D::Grid& g, const char* fn,
		std::function<std::string(int)> btype_name,
		PeriodicData periodic){
	if (periodic.size() == 0) return gridmsh(g, fn, btype_name);
	else{
		std::map<Face*, Face*> periodic_cells;
		HMGrid3D::Grid gp = periodic.assemble(g, periodic_cells);
		return gridmsh(g, fn, btype_name, periodic_cells);
	}
}

void hme::GridMSH(const HMGrid3D::Grid& g, const char* fn){
	return gridmsh(g, fn, [](int i)->std::string{
			return std::string("boundary") + std::to_string(i);
		});
}

void hme::GridMSH(const HMGrid3D::Grid& g, const char* fn,
		std::function<std::string(int)> btype_name){
	return gridmsh(g, fn, btype_name);
}

void hme::GridMSH(const HMGrid3D::Grid& g, const char* fn,
		PeriodicData periodic){
	return GridMSH(g, fn, 
			[](int i)->std::string{
				return std::string("boundary") + std::to_string(i);
			},
			periodic);
}