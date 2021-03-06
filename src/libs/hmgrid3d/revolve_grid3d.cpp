#include "revolve_grid3d.hpp"
#include "debug3d.hpp"
using namespace HM3D;

namespace cns = Grid::Constructor;

namespace{
struct revolve_builder{
//  * planar surface - a surface which is built by rotation of 2d
//                     grid plane at certain angle.
//  * planar primitive - an edge or face which belongs to planar surface.
//  * perp primitive - an edge or face which connects sibling planar surfaces
 
//input data:
	vector<double> phi;
	const HM2D::GridData* g2;
	Vertex rot_vec, rot_p0;
	int Nsurf, Nsurf_wc; //number of surfaces, number of surfaces with 3d cell layer
	vector<vector<int>> cell_edges;
	vector<vector<bool>> cell_edges_isleft;
	vector<double> vertex_measure;
	bool iscomplete;
	
// Result of this procedure are arrays built in format of SimpleSerialize data.
	vector<int> cells, faces, edges, bnd, icell, iface;
	vector<double> vertices;

// Each 2d grid primitive has its index in input grid.
	vector<int> normal_cell, normal_edge, normal_vertex,
	            axis_cell, axis_edge, axis_vertex,
	            normal_edge_nn, normal_edge_n;
	vector<char> edge_type; //0-axis, 1-nn (both points are normal), 2-na (normal->axis), 3-an
	vector<char> cell_type; //0-axis, 1-regular
	vector<bool> do_revolve_edge; //should the edge be copied to each surface
// Each 3d primitives could be accessed by index of planar surface, and
// index of corresponding 2d primitive upon which the 3d one was built.
	vector<vector<int>> planar_edge3, planar_face3, perp_edge3, perp_face3,
	                    vertices3;
	std::map<int, vector<int>> boundary_types;
	vector<double> edge_curvature;

	revolve_builder(const HM2D::GridData& g2d, const vector<double>& phi_deg,
			Point pstart, Point pend){
		//prepare
		_0_fill_input(g2d, phi_deg, pstart, pend);
	}
	void process(){
		_1_sort_out_2d_data();
		//vertices
		_2_fill_vertices();
		if (iscomplete) vertices3.push_back(vertices3[0]);
		//edges
		_3_fill_planar_edges();
		_4_fill_perp_edges();
		if (iscomplete) planar_edge3.push_back(planar_edge3[0]);
		//faces
		_5_fill_planar_faces();
		_6_fill_normal_perp_faces();
		_6_fill_axis_perp_faces();
		if (iscomplete) planar_face3.push_back(planar_face3[0]);
		iface.push_back(faces.size());
		//cells
		_7_fill_interior_cells();
		_8_fill_axis_cells();
		icell.push_back(cells.size());
	}
	GridData build_ess(){
		GridData ret;
		fill_primitives(ret);
		for (auto& v: boundary_types){
			for (auto& find: v.second){
				ret.vfaces[find]->boundary_type = v.first;
			}
		}
		return ret;
	}
	void side_boundary(){
		for (int i=0; i<g2->vedges.size(); ++i) if (g2->vedges[i]->is_boundary()){
			int b = g2->vedges[i]->boundary_type;
			auto emp = boundary_types.emplace(b, vector<int>());
			vector<int>& inp = emp.first->second;
			for (int j=0; j<Nsurf_wc; ++j){
				int find = perp_face3[j][i];
				if (find >= 0) inp.push_back(find);
			}
		}
	}
	void afirst_boundary(int f){
		for (int i=0; i<g2->vcells.size(); ++i){
			int b = f;
			auto emp = boundary_types.emplace(b, vector<int>());
			vector<int>& inp = emp.first->second;
			int find = planar_face3[0][i];
			if (find >= 0) inp.push_back(find);
		}
	}
	void alast_boundary(int f){
		for (int i=0; i<g2->vcells.size(); ++i){
			int b = f;
			auto emp = boundary_types.emplace(b, vector<int>());
			vector<int>& inp = emp.first->second;
			int find = planar_face3[Nsurf-1][i];
			if (find >= 0) inp.push_back(find);
		}
	}
protected:
	void _0_fill_input(const HM2D::GridData& g2d, const vector<double>& phi_deg,
			Point pstart, Point pend){
		g2 = &g2d;
		phi.resize(phi_deg.size());
		auto it = phi.begin();
		for (auto v: phi_deg) *it++ = v/180.0*M_PI;
		rot_vec.x = pend.x - pstart.x;
		rot_vec.y = pend.y - pstart.y;
		rot_vec.z = 0;
		double abs = sqrt(sqr(rot_vec.x) + sqr(rot_vec.y) + sqr(rot_vec.z));
		rot_vec.x/=abs; rot_vec.y/=abs; rot_vec.z/=abs;
		rot_p0.x = pstart.x;
		rot_p0.y = pstart.y;
		rot_p0.z = 0;
		Nsurf = phi_deg.size();
		Nsurf_wc = Nsurf - 1;
		iscomplete = false;
		if (ISZERO(phi.back() - phi[0] - 2*M_PI)) {--Nsurf; iscomplete=true;}
		//cell->edges connectivity
		g2d.enumerate_all();
		cell_edges.reserve(g2d.vcells.size());
		for (auto c: g2d.vcells){
			cell_edges.emplace_back();
			for (auto e: c->edges){
				cell_edges.back().push_back(e->id);
			}
		}

		//cell->edges->is_left
		cell_edges_isleft.resize(g2d.vcells.size());
		for (int i=0; i<cell_edges_isleft.size(); ++i){
			auto& il = cell_edges_isleft[i];
			for (int j=0; j<cell_edges[i].size(); ++j){
				il.push_back(g2d.vcells[i]->edges[j]->left.lock() == g2d.vcells[i]);
			}
		}
	}
	void _1_sort_out_2d_data(){
		//vertices
		vector<bool> is_normal_vertex(g2->vvert.size());
		double x0 = rot_p0.x, y0 = rot_p0.y;
		std::array<double, 3> A = Point::line_eq(Point(0, 0), Point(rot_vec.x, rot_vec.y));
		auto meas_line = [A, x0, y0](const Point& p) -> double{
			double d0=A[0]*(p.x-x0) + A[1]*(p.y-y0) + A[2];
			return SIGN(d0)*d0*d0;
		};
		int sgn=0;
		vertex_measure.resize(g2->vvert.size());
		for (int i=0; i<g2->vvert.size(); ++i){
			double m = meas_line(*g2->vvert[i]);
			vertex_measure[i] = m;
			if (fabs(m) < geps*geps) {
				is_normal_vertex[i] = false;
				axis_vertex.push_back(i);
			}else{
				if (sgn == 0) sgn = SIGN(m);
				else if ((m>0 && sgn<0) || (m<0 && sgn>0))
					throw std::runtime_error("All points should be located"
							" to the one side of revolution vector");
				is_normal_vertex[i] = true;
				normal_vertex.push_back(i);
			}
		}
	
		//edges
		vector<bool> is_normal_edge(g2->vedges.size());
		edge_type.resize(g2->vedges.size());
		for (int i=0; i<g2->vedges.size(); ++i){
			int p1 = g2->vedges[i]->first()->id,
			    p2 = g2->vedges[i]->last()->id;
			bool n1 = is_normal_vertex[p1], n2 = is_normal_vertex[p2];
			if (!n1 && !n2){
				is_normal_edge[i] = false;
				axis_edge.push_back(i);
				edge_type[i] = 0;
			} else{
				is_normal_edge[i] = true;
				normal_edge.push_back(i);
				if (n1 && n2) {normal_edge_nn.push_back(i); edge_type[i] = 1;}
				else if (n1) {normal_edge_n.push_back(i); edge_type[i] = 2; }
				else {normal_edge_n.push_back(i); edge_type[i] = 3; }
			}
		}
		//cells
		for (int i=0; i<g2->vcells.size(); ++i){
			bool isnormal = true;
			for (int j=0; j<cell_edges[i].size(); ++j){
				if (!is_normal_edge[cell_edges[i][j]]){
					isnormal = false;
					break;
				}
			}
			if (isnormal) {
				cell_type.push_back(1);
				normal_cell.push_back(i);
			}else{
				cell_type.push_back(0);
				axis_cell.push_back(i);
			}
		}
		detect_edges_revolution();
	}
	virtual void detect_edges_revolution(){
		do_revolve_edge.resize(g2->vedges.size());
		for (int i=0; i<g2->vedges.size(); ++i){
			do_revolve_edge[i] = (edge_type[i] != 0);
		}
	}
	virtual void _2_fill_vertices(){
		int Nvert3 = Nsurf * normal_vertex.size() + axis_vertex.size();
		vertices.resize(Nvert3 * 3);
		vertices3.resize(Nsurf, vector<int>(normal_vertex.size() + axis_vertex.size()));
		int n = 0;
		auto it = vertices.begin();
		//regular vertices
		for (int j=0; j<Nsurf; ++j){
			double cosa = cos(phi[j]), sina = sin(phi[j]);
			double M11 = cosa + (1-cosa) * rot_vec.x * rot_vec.x;
			double M12 = (1-cosa) * rot_vec.x * rot_vec.y - sina * rot_vec.z;
			//double M13 = (1-cosa) * rot_vec.x * rot_vec.z + sina * rot_vec.y;
			double M21 = (1-cosa) * rot_vec.x * rot_vec.y + sina * rot_vec.z;
			double M22 = cosa + (1-cosa) * rot_vec.y * rot_vec.y;
			//double M23 = (1-cosa) * rot_vec.y * rot_vec.z - sina * rot_vec.x;
			double M31 = (1-cosa) * rot_vec.x * rot_vec.z - sina * rot_vec.y;
			double M32 = (1-cosa) * rot_vec.y * rot_vec.z + sina * rot_vec.x;
			//double M33 = cosa + (1-cosa) * rot_vec.z * rot_vec.z;
			for (int i=0; i<normal_vertex.size(); ++i){
				auto p = g2->vvert[normal_vertex[i]];
				double x = p->x - rot_p0.x, y = p->y - rot_p0.y, z = 0;
				*it++ = M11 * x + M12 * y + rot_p0.x;
				*it++ = M21 * x + M22 * y + rot_p0.y;
				*it++ = M31 * x + M32 * y;
				vertices3[j][normal_vertex[i]] = n++;
			}
		}
		//axis vertices
		for (int i=0; i<axis_vertex.size(); ++i){
			auto p = g2->vvert[axis_vertex[i]];
			*it++ = p->x;
			*it++ = p->y;
			*it++ = 0;
			for (int j=0; j<Nsurf; ++j) vertices3[j][axis_vertex[i]] = n;
			++n;
		}
	}
	virtual void _3_fill_planar_edges(){
		int Nedges = Nsurf * normal_edge.size() + axis_edge.size();
		edges.resize(Nedges*2);
		edge_curvature.resize(Nedges, 0.0);
		planar_edge3.resize(Nsurf, vector<int>(g2->vedges.size()));
		int n=0;
		auto it = edges.begin();
		//normal edges
		for (int i=0; i<normal_edge.size(); ++i){
			int ed = normal_edge[i];
			int p1 = g2->vedges[ed]->first()->id, p2 = g2->vedges[ed]->last()->id;
			for (int j=0; j<Nsurf; ++j){
				int v1 = vertices3[j][p1], v2 = vertices3[j][p2];
				*it++ = v1;
				*it++ = v2;
				planar_edge3[j][ed] = n++;
			}
		}
		//axis edges
		for (int i=0; i<axis_edge.size(); ++i){
			int ed = axis_edge[i];
			int p1 = g2->vedges[ed]->first()->id, p2 = g2->vedges[ed]->last()->id;
			int v1 = vertices3[0][p1], v2 = vertices3[0][p2];
			*it++ = v1;
			*it++ = v2;
			for (int j=0; j<Nsurf; ++j) planar_edge3[j][ed] = n;
			++n;
		}
	}
	void _4_fill_perp_edges(){
		int Nedges = Nsurf_wc * normal_vertex.size();
		int n = edges.size() / 2;
		edges.resize(2*n + 2*Nedges);
		edge_curvature.resize(n+Nedges, 0);
		perp_edge3.resize(Nsurf_wc, vector<int>(g2->vvert.size()));
		auto it = edges.begin() + 2 * n;
		for (int i=0; i<normal_vertex.size(); ++i){
			int v = normal_vertex[i];
			double curv = 1.0/sqrt(fabs(vertex_measure[v]));
			for (int j=0; j<Nsurf_wc; ++j){
				int p1 = vertices3[j][v];
				int p2 = vertices3[j+1][v];
				*it++ = p1;
				*it++ = p2;
				edge_curvature[n] = curv;
				perp_edge3[j][v] = n++;
			}
		}
	}
	virtual void _5_fill_planar_faces(){
		//calculate sizes
		int sz=0;
		for (auto& v: cell_edges) sz+=(v.size() + 3);
		faces.resize(Nsurf * sz);
		planar_face3.resize(Nsurf, vector<int>(cell_edges.size(), -1));
		iface.resize(Nsurf*g2->vcells.size());
		//fill
		int n=0;
		auto it = faces.begin();
		for (int i=0; i<g2->vcells.size(); ++i){
			int ned = cell_edges[i].size();
			for (int j=0; j<Nsurf; ++j){
				iface[n] = it - faces.begin();
				*it++ = ned;
				for (int k=0; k<ned; ++k){
					int ed = cell_edges[i][k];
					*it++ = planar_edge3[j][ed];
				}
				*it++ = -1;
				*it++ = -1;
				planar_face3[j][i] = n++;
			}
		}
	}
	void _6_fill_normal_perp_faces(){
		int n = iface.size();
		iface.resize(n + Nsurf_wc*normal_edge_nn.size());
		int oldlen = faces.size();
		faces.resize(oldlen + Nsurf_wc*normal_edge_nn.size()*7);
		auto it = faces.begin() + oldlen;
		perp_face3.resize(Nsurf_wc, vector<int>(g2->vedges.size(), -1));
		for (int i=0; i<normal_edge_nn.size(); ++i){
			int ed_2d = normal_edge_nn[i];
			int pstart_2d = g2->vedges[ed_2d]->first()->id;
			int pend_2d = g2->vedges[ed_2d]->last()->id;
			for (int j=0; j<Nsurf_wc; ++j){
				iface[n] = it - faces.begin();
				*it++ = 4;
				*it++ = planar_edge3[j][ed_2d];
				*it++ = perp_edge3[j][pend_2d];
				*it++ = planar_edge3[j+1][ed_2d]; 
				*it++ = perp_edge3[j][pstart_2d];
				*it++ = -1;
				*it++ = -1;
				perp_face3[j][ed_2d] = n++;
			}
		}
	}
	virtual void _6_fill_axis_perp_faces(){
		int n = iface.size();
		iface.resize(n + Nsurf_wc*normal_edge_n.size());
		int oldlen = faces.size();
		faces.resize(oldlen + Nsurf_wc*normal_edge_n.size()*6);
		auto it = faces.begin() + oldlen;
		for (int i=0; i<normal_edge_n.size(); ++i){
			int ed_2d = normal_edge_n[i];
			int pstart_2d = g2->vedges[ed_2d]->first()->id;
			int pend_2d = g2->vedges[ed_2d]->last()->id;
			for (int j=0; j<Nsurf_wc; ++j){
				iface[n] = it - faces.begin();
				*it++ = 3;
				*it++ = planar_edge3[j][ed_2d];
				if (edge_type[ed_2d] == 3) *it++ = perp_edge3[j][pend_2d];
				*it++ = planar_edge3[j+1][ed_2d]; 
				if (edge_type[ed_2d] == 2) *it++ = perp_edge3[j][pstart_2d];
				*it++ = -1;
				*it++ = -1;
				perp_face3[j][ed_2d] = n++;
			}
		}
	}
	void _7_fill_interior_cells(){
		//sizes
		int sz = 0;
		for (int i=0; i<normal_cell.size(); ++i)
			sz += (3 + cell_edges[normal_cell[i]].size());
		cells.resize(Nsurf_wc * sz);
		icell.resize(normal_cell.size()*Nsurf_wc);
		//filling
		int n = 0;
		auto it = cells.begin();
		for (int i=0; i<normal_cell.size(); ++i){
			int icell2d = normal_cell[i];
			auto& eds = cell_edges[icell2d];
			auto& isleft = cell_edges_isleft[icell2d];
			for (int j=0; j<Nsurf_wc; ++j){
				icell[n] = it - cells.begin();
				//cell->face connectivity
				*it++ = 2 + eds.size();
				*it++ = planar_face3[j][icell2d];
				*it++ = planar_face3[j+1][icell2d];
				for (int k=0; k<eds.size(); ++k){
					*it++ = perp_face3[j][eds[k]];
				}
				//face->cell connectivity
				add_lface_adj(n, planar_face3[j][icell2d]);
				add_rface_adj(n, planar_face3[j+1][icell2d]);
				assert(eds.size()==isleft.size());
				for (int k=0; k<eds.size(); ++k){
					if (isleft[k]){
						add_rface_adj(n, perp_face3[j][eds[k]]);
					} else {
						add_lface_adj(n, perp_face3[j][eds[k]]);
					}
				}
				++n;
			}
		}
	}

	//adds data to cells, Ncells;
	virtual void _8_fill_axis_cells(){
		vector<int> addcell;
		int n = icell.size();
		for (int i=0; i<axis_cell.size(); ++i){
			int icell2d = axis_cell[i];
			auto& eds_all = cell_edges[icell2d];
			auto& isleft_all = cell_edges_isleft[icell2d];
			//leave only normal edges in above vectors
			vector<int> eds;
			vector<bool> isleft;
			for (int j=0; j<eds_all.size(); ++j){
				if (edge_type[eds_all[j]] != 0){
					eds.push_back(eds_all[j]);
					isleft.push_back(isleft_all[j]);
				}
			}
			for (int j=0; j<Nsurf_wc; ++j){
				icell.push_back(cells.size() + addcell.size());
				//topology
				addcell.push_back(2+eds.size());
				addcell.push_back(planar_face3[j][icell2d]);
				addcell.push_back(planar_face3[j+1][icell2d]);
				for (auto k=0; k<eds.size(); ++k){
					addcell.push_back(perp_face3[j][eds[k]]);
				}
				//face->cell connectivity
				add_lface_adj(n, planar_face3[j][icell2d]);
				add_rface_adj(n, planar_face3[j+1][icell2d]);
				for (int k=0; k<eds.size(); ++k){
					if (isleft[k]){
						add_rface_adj(n, perp_face3[j][eds[k]]);
					} else {
						add_lface_adj(n, perp_face3[j][eds[k]]);
					}
				}
				++n;
			}
		}
		cells.insert(cells.end(), addcell.begin(), addcell.end());
	}

	//adds data to face
	void add_lface_adj(int index_cell, int index_face){
		faces[iface[index_face+1]-2] = index_cell;
	}
	void add_rface_adj(int index_cell, int index_face){
		faces[iface[index_face+1]-1] = index_cell;
	}

	void fill_primitives(HM3D::GridData& ret){
		auto& rvert = ret.vvert;
		auto& redge = ret.vedges;
		auto& rface = ret.vfaces;
		auto& rcell = ret.vcells;
		//vertices
		rvert.resize(vertices.size()/3);
		auto vit = vertices.begin();
		for (int i=0; i<rvert.size(); ++i){
			rvert[i].reset(new Vertex(*vit, *(vit+1), *(vit+2)));
			vit+=3;
		}
		//edges
		redge.resize(edges.size()/2);
		auto eit = edges.begin();
		auto curvit = edge_curvature.begin();
		for (int i=0; i<redge.size(); ++i){
			auto p1=rvert[*eit++];
			auto p2=rvert[*eit++];
			double curv = *curvit++;
			redge[i].reset(new HM3D::Edge(p1, p2));
		}
		//faces init
		rface.resize(iface.size()-1);
		for (int i=0; i<rface.size(); ++i) rface[i].reset(new Face());
		//cells init
		rcell.resize(icell.size()-1);
		for (int i=0; i<rcell.size(); ++i) rcell[i].reset(new HM3D::Cell());
		//faces
		auto fit = faces.begin();
		for (int i=0; i<rface.size(); ++i){
			auto& f = rface[i];
			int n = *fit++;
			for (int k=0; k<n; ++k){
				f->edges.push_back(redge[*fit++]);
			}
			int c1 = *fit++;
			int c2 = *fit++;
			if (c1>=0) f->left = rcell[c1];
			if (c2>=0) f->right = rcell[c2];
		}
		//cells
		auto cit = cells.begin();
		for (int i=0; i<rcell.size(); ++i){
			auto& c = rcell[i];
			int n = *cit++;
			for (int k=0; k<n; ++k){
				c->faces.push_back(rface[*cit++]);
			}
		}
	}
};

class revolve_builder_no_tri: public revolve_builder{
public:
	revolve_builder_no_tri(const HM2D::GridData& g2d, const vector<double>& phi_deg,
			Point pstart, Point pend): revolve_builder(g2d, phi_deg, pstart, pend){}
protected:
	void detect_edges_revolution() override {
		do_revolve_edge.resize(g2->vedges.size());
		for (int i=0; i<g2->vedges.size(); ++i){
			switch (edge_type[i]){
				case 0: do_revolve_edge[i] = false; break;
				case 1: do_revolve_edge[i] = true; break;
				case 2: case 3: 
				{
					int c1 = g2->vedges[i]->has_left_cell() ?
					         g2->vedges[i]->left.lock()->id :
					         g2->vedges[i]->right.lock()->id;
					int c2 = g2->vedges[i]->has_right_cell() ?
					         g2->vedges[i]->right.lock()->id :
					         g2->vedges[i]->left.lock()->id;
					do_revolve_edge[i] = (cell_type[c1] == 1 || cell_type[c2] == 1);
				}
			}
		}
	}
	void _2_fill_vertices() override{
		revolve_builder::_2_fill_vertices();
		if (iscomplete){
			int n = Nsurf * normal_vertex.size();
			vertices.resize(3 * n);
			std::unordered_set<int> used;
			for (int i=0; i<normal_edge_n.size(); ++i){
				int ed_2d = normal_edge_n[i];
				if (do_revolve_edge[ed_2d]){
					int axisnode = (edge_type[ed_2d] == 2) ? g2->vedges[ed_2d]->last()->id
					                                       : g2->vedges[ed_2d]->first()->id;
					if (used.emplace(axisnode).second == false) continue;
					auto p = g2->vvert[axisnode];
					vertices.push_back(p->x);
					vertices.push_back(p->y);
					vertices.push_back(0);
					for (int j=0; j<Nsurf; ++j) vertices3[j][axisnode] = n;
					++n;
				}
			}
		}
	}
	void _3_fill_planar_edges() override {
		int Nedges = 0;
		for (int i=0; i<g2->vedges.size(); ++i){
			if (do_revolve_edge[i]) Nedges += Nsurf;
			else {
				if (!iscomplete){
					if (edge_type[i] == 0) Nedges+=1;
					else Nedges+=2;
				}
			}
		}
		edges.resize(Nedges*2);
		edge_curvature.resize(Nedges, 0.0);
		planar_edge3.resize(Nsurf, vector<int>(g2->vedges.size()));
		int n=0;
		auto it = edges.begin();
		for (int i=0; i<normal_edge.size(); ++i){
			int ed = normal_edge[i];
			if (!do_revolve_edge[ed] && iscomplete) continue;
			int p1 = g2->vedges[ed]->first()->id, p2 = g2->vedges[ed]->last()->id;
			for (int j=0; j<Nsurf; ++j){
				if (!do_revolve_edge[ed] && j!=0 && j!=Nsurf-1) continue;
				int v1 = vertices3[j][p1], v2 = vertices3[j][p2];
				*it++ = v1;
				*it++ = v2;
				planar_edge3[j][ed] = n++;
			}
		}
		//axis edges
		if (!iscomplete) for (int i=0; i<axis_edge.size(); ++i){
			int ed = axis_edge[i];
			int p1 = g2->vedges[ed]->first()->id, p2 = g2->vedges[ed]->last()->id;
			int v1 = vertices3[0][p1], v2 = vertices3[0][p2];
			*it++ = v1;
			*it++ = v2;
			for (int j=0; j<Nsurf; ++j) planar_edge3[j][ed] = n;
			++n;
		}
		assert(n == Nedges);
	}
	void _5_fill_planar_faces() override {
		//calculate sizes with overhead. Arrays will be shrinked at the end.
		int sz=0;
		for (auto& v: cell_edges) sz+=(v.size() + 3);
		faces.resize(Nsurf * sz);
		planar_face3.resize(Nsurf, vector<int>(cell_edges.size(), -1));
		iface.resize(Nsurf*g2->vcells.size());
		//fill
		int n=0;
		auto it = faces.begin();
		for (int i=0; i<g2->vcells.size(); ++i){
			if (iscomplete && cell_type[i] != 1) continue;
			int ned = cell_edges[i].size();
			for (int j=0; j<Nsurf; ++j){
				if (cell_type[i] != 1 && j!=0 && j!=Nsurf-1) continue;
				iface[n] = it - faces.begin();
				*it++ = ned;
				for (int k=0; k<ned; ++k){
					int ed = cell_edges[i][k];
					*it++ = planar_edge3[j][ed];
				}
				*it++ = -1;
				*it++ = -1;
				planar_face3[j][i] = n++;
			}
		}
		//shrink sizes
		iface.resize(n);
		faces.resize(it-faces.begin());
	}

	void _6_fill_axis_perp_faces() override {
		int Nfaces = normal_edge_n.size() * Nsurf;
		int n = iface.size();
		iface.resize(n + Nfaces);
		int oldlen = faces.size();
		faces.resize(oldlen + 6*Nfaces);
		auto it = faces.begin() + oldlen;
		for (int i=0; i<normal_edge_n.size(); ++i){
			int ed_2d = normal_edge_n[i];
			int pend_2d = g2->vedges[ed_2d]->last()->id;
			int pstart_2d = g2->vedges[ed_2d]->first()->id;
			if (!do_revolve_edge[ed_2d]){
				iface[n] = it-faces.begin();
				*it++ = (iscomplete) ? Nsurf_wc : Nsurf_wc + 2;
				if (!iscomplete) *it++ = planar_edge3[0][ed_2d];
				if (edge_type[ed_2d] == 3) for (int j=0; j<Nsurf_wc; ++j)
					*it++ = perp_edge3[j][pend_2d];
				if (!iscomplete) *it++ = planar_edge3[Nsurf-1][ed_2d]; 
				if (edge_type[ed_2d] == 2) for (int j=Nsurf_wc-1; j>=0; --j)
					*it++ = perp_edge3[j][pstart_2d];
				*it++ = -1;
				*it++ = -1;
				perp_face3[0][ed_2d] = n++;
			} else {
				for (int j=0; j<Nsurf_wc; ++j){
					iface[n] = it - faces.begin();
					*it++ = 3;
					*it++ = planar_edge3[j][ed_2d];
					if (edge_type[ed_2d] == 3) *it++ = perp_edge3[j][pend_2d];
					*it++ = planar_edge3[j+1][ed_2d]; 
					if (edge_type[ed_2d] == 2) *it++ = perp_edge3[j][pstart_2d];
					*it++ = -1;
					*it++ = -1;
					perp_face3[j][ed_2d] = n++;
				}
			}
		}
		//shrink
		iface.resize(n);
		faces.resize(it-faces.begin());
	}

	void _8_fill_axis_cells() override{
		std::list<int> addcell;
		int n = icell.size();
		for (int i=0; i<axis_cell.size(); ++i){
			icell.push_back(cells.size() + addcell.size());
			addcell.push_back(0);
			int& sz = addcell.back();
			int icell2d = axis_cell[i];
			//planar faces
			if (!iscomplete){
				int f1 = planar_face3[0][icell2d];
				int f2 = planar_face3[Nsurf-1][icell2d];
				sz+=2;
				addcell.push_back(f1);
				addcell.push_back(f2);
				add_lface_adj(n, f1);
				add_rface_adj(n, f2);
			}
			auto& eds = cell_edges[icell2d];
			auto& isleft = cell_edges_isleft[icell2d];
			//other faces
			for (int j=0; j<eds.size(); ++j) if (edge_type[eds[j]] != 0) {
				int ed = eds[j];
				for (int k=0; k<Nsurf_wc; ++k){
					//if (edge_type[ed] != 1 && k!=0) break;
					if (!do_revolve_edge[ed] && k>0) break;
					int f = perp_face3[k][eds[j]];
					sz += 1;
					addcell.push_back(f);
					isleft[j] ? add_rface_adj(n, f) : add_lface_adj(n, f);
				}
			}
			++n;
		}
		cells.insert(cells.end(), addcell.begin(), addcell.end());
	}
};

shared_ptr<revolve_builder> revolve_builder_factory(const HM2D::GridData& g2d, const vector<double>& phi_coords,
		Point pstart, Point pend, bool is_trian){
	if (is_trian) return std::make_shared<revolve_builder>(revolve_builder(g2d, phi_coords, pstart, pend));
	else return std::make_shared<revolve_builder_no_tri>(revolve_builder_no_tri(g2d, phi_coords, pstart, pend));
}

struct EdRevert{
	HM2D::GridData* g2;
	vector<bool> reverted_edges;
	EdRevert(const HM2D::GridData& g2d){
		g2 = const_cast<HM2D::GridData*>(&g2d);
		g2d.enumerate_all();
		reverted_edges.resize(g2d.vedges.size(), false);
		for (int i=0; i<g2d.vedges.size(); ++i){
			if (g2d.vedges[i]->first()->id > g2d.vedges[i]->last()->id){
				reverted_edges[i] = true;
			}
		}
		dorevert();
	}
	~EdRevert(){ dorevert(); }
	void dorevert(){
		for (int i=0; i<g2->vedges.size(); ++i){
			if (reverted_edges[i]) g2->vedges[i]->reverse();
		}
	}
};

};

HM3D::GridData cns::RevolveGrid2D(const HM2D::GridData& g2d, const vector<double>& phi_coords,
		Point pstart, Point pend, bool is_trian,
		int bt1, int bt2){
	//temporary revert edges of g2 so that: first()->id < last()->id for backward compatibility
	EdRevert er(g2d);
	//topology
	auto dt = revolve_builder_factory(g2d, phi_coords, pstart, pend, is_trian);
	dt->process();
	//boundary condition
	dt->side_boundary();
	if (!dt->iscomplete){
		dt->afirst_boundary(bt1);
		dt->alast_boundary(bt2);
	}
	//assemble grid
	return dt->build_ess();
}
