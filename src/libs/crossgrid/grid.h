#ifndef CROSSGRID_GRID_H
#define CROSSGRID_GRID_H

#include "crossgrid.h"
#include "bgeom2d.h"
#include "contours.h"

namespace GGeom{
struct Modify;
struct Constructor;
struct Info;
};

class GridPoint: public Point{
	int ind;
public:
	GridPoint(double x, double y, int _ind=0): Point(x,y), ind(_ind){}
	GridPoint(const Point& p): Point(p), ind(0){}
	int get_ind() const { return ind; }

	friend class GridGeom;
};

class Cell{
	int ind;
	//reverses points array if neseccary
	void check_ordering();
public:
	vector<GridPoint*> points;
	explicit Cell(int _ind = 0):ind(_ind){}
	int dim() const { return points.size(); }
	const GridPoint* get_point(int i) const {
		if (i<0) return get_point(i+dim());
		else if (i>=dim()) return get_point(i-dim());
		else return points[i];
	}
	vector<const GridPoint*> get_points() const
		{ return vector<const GridPoint*>(points.begin(), points.end());}
	int get_ind() const { return ind; }
	double area() const;
	PContour get_contour() const;
	//returns true if cell edges has crosses or are tangent
	bool has_self_crosses() const;

	friend class GridGeom;
};

struct Edge{
	//nodes indicies
	int p1, p2;
	//cells indicies
	mutable int cell_left, cell_right;
	Edge(int i1=0, int i2=0):
		p1(std::min(i1, i2)), p2(std::max(i1,i2)), 
		cell_left(-1), cell_right(-1){}
	void add_adj_cell(int cell_ind, int i1, int i2) const;
	bool is_boundary() const { return cell_left<0 || cell_right<0; }
	int any_cell() const { return std::max(cell_left, cell_right); }
};
inline bool operator<(const Edge& e1, const Edge& e2){
	if (e1.p1<e2.p1) return true;
	else if (e1.p1>e2.p1) return false;
	else return (e1.p2<e2.p2);
}

class GridGeom: public Grid{
protected:
	//Data
	ShpVector<GridPoint> points;
	ShpVector<Cell> cells;
	//tables
	vector<vector<int>> point_cell_tab() const;
	//make all cells be counter clockwise
	void force_cells_ordering();
	//indexation
	void set_indicies();
	void delete_unused_points();
	//data manipulation
	static void add_point_to_cell(Cell* c, GridPoint* p){ c->points.push_back(p); }
	static void change_point_of_cell(Cell* c, int j, GridPoint* p){ c->points[j] = p; }
	static void delete_point_of_cell(Cell* c, int j){ c->points.erase(c->points.begin()+j); }
	static void shallow_copy(GridGeom* from, GridGeom* to){ to->cells = from->cells; to->points = from->points; }
	void clear(){ points.clear(); cells.clear(); }
	vector<Point> cells_internal_points() const;
	//constructors
	GridGeom(){};
	GridGeom(const GridGeom& g);
	GridGeom& operator=(GridGeom g);
	//swaps points and cells arrays between two grids
	static void swap_data(GridGeom& g1, GridGeom& g2);
	//add all points and cells from grid. No points merge. Deepcopied.
	void add_data(const GridGeom& g);
	//add points and cells from cls index array. Merge congruent points.
	void add_data(const GridGeom& g, const std::vector<int>& cls);
	//merge points with equal coordinates
	void merge_congruent_points();
	//remove cells
	void remove_cells(const vector<int>& bad_cells);
	//remove cells which lie outside cnt
	void leave_only(const ContoursCollection& cnt);
	//remove cells which lie inside cnt
	void exclude_area(const ContoursCollection& cnt);
	//tries to move boundary points to bp.
	//bp should lie on existing boundary
	void move_boundary_points(const vector<Point>& bp);
	//move point srcp->tarp and check if cell geometry is still ok
	//returns false if failed to move without breaking geometry
	bool move_point(const GridPoint* srcp, const Point& tarp, const vector<vector<int>>& point_cell);

public:
	//build grid from raw points coordinates array
	//and cells->points connectivity array
	//pts = [x0, y0, x1, y1, ..... ]
	//cls = 
	//	[number of points in cell0, cell0 point0 index, point1 index, ..., 
	//	 number of points in cell1, cell1 point0, cell1 point1,....]
	GridGeom(int Npts, int Ncells, double* pts, int* cls);
	GridGeom(GridGeom&& g);
	~GridGeom(){}

	//number of points
	int n_points() const { return points.size(); }
	//number of cells
	int n_cells() const { return cells.size(); }
	//sum of all cells dimensions
	int n_cellsdim() const;
	//area as a sum of cells area
	double area() const;

	//scaling
	ScaleBase do_scale();
	void do_scale(const ScaleBase& sc);
	void undo_scale(const ScaleBase& sc);
	//contours manipulation
	vector<PContour> get_contours() const;
	ContoursCollection get_contours_collection() const { return ContoursCollection(get_contours()); }
	std::pair<Point, Point> outer_rect() const;
	
	//returns set of single connected meshes.
	ShpVector<GridGeom> subdivide() const;

	//data access
	const GridPoint* get_point(int i) const { return points[i].get(); }
	const Cell* get_cell(int i) const { return cells[i].get(); }
	GridPoint* get_point(int i) { return points[i].get(); }
	Cell* get_cell(int i) { return cells[i].get(); }
	//edges 
	std::set<Edge> get_edges() const;
	//boundary points
	std::set<const GridPoint*> get_bnd_points() const;

	//data modify
	//change internal structure of the grid with data from another one.
	//in fact simply copies data from gg data
	void change_internal(const GridGeom& gg);

	//static builders
	static GridGeom* cross_grids(GridGeom* gmain, GridGeom* gsec, double buffer_size, 
			double density, bool preserve_bp, bool empty_holes,
			CrossGridCallback::func cb);
	
	//result = g - area(c). Area could be inner or outer. 
	static GridGeom* grid_minus_cont(GridGeom* g, PointsContoursCollection* c,
			bool is_inner,
			CrossGridCallback::func cb);
	
	//builds a grid wich is constructed by imposition of gsec onto gmain
	//no bufferzones. 
	//Created grid area equals the intersection of gmain and gsec areas.
	static GridGeom* combine(GridGeom* gmain, GridGeom* gsec);

	//builds a grid from the set of other grids. Grids should not overlap.
	//no nodes merging, no check procedures. Deepcopied.
	static GridGeom sum(const vector<GridGeom>& g);
	static GridGeom sum(const vector<GridGeom*>& g);

	friend class BufferGrid;

	friend struct GGeom::Constructor;
	friend struct GGeom::Modify;
	friend struct GGeom::Info;
};


//This is a new style interface.
//Old interface should be completely changed to new one during grid refactoring.
#include "hybmesh_contours2d.hpp"
#include <sstream>
namespace GGeom{

// === exceptions
//basic exception
class EException: public std::runtime_error{
public:
	EException(std::string m) noexcept: std::runtime_error(
			std::string("Grid geometry exception: ") + m){};
};
//point is out of area
class EOutOfArea: public EException{
	static std::string Msg(Point p){
		std::ostringstream s;
		s<<"Point "<<p<<" lies outside grid area"<<std::endl;
		return s.str();
	};
public:
	EOutOfArea(Point p) noexcept: EException(Msg(p)){};
};


// === builders
struct Constructor{

static GridGeom RectGrid(const vector<double>& part_x, vector<double>& part_y);
static GridGeom RectGrid01(int Nx, int Ny);

};

// === modifiers
struct Modify{

static void RemoveCells(GridGeom& grid, const std::vector<const Cell*>& cls);
//primitives modifications
static void PointModify(GridGeom& grid, std::function<void(GridPoint*)> fun);
static void CellModify(GridGeom& grid, std::function<void(Cell*)> fun);
//adds data
static void ShallowAdd(const GridGeom* from, GridGeom* to);
static void ShallowAdd(const ShpVector<GridPoint>& from, GridGeom* to);
static void DeepAdd(const GridGeom* from, GridGeom* to);
//merges congruent, deletes unused, forces cells rotation
static void Heal(GridGeom& grid);

};

// === grid structure information
struct Info{

//points
static ShpVector<GridPoint> SharePoints(const GridGeom& grid, const vector<int>& indicies);
static ShpVector<GridPoint> BoundaryPoints(const GridGeom& grid);
//collection of all outer contours
static HMCont2D::ContourTree Contour(const GridGeom& grid);
//only the first contour of tree.
//Used for grids which are definitly singly connected
static HMCont2D::Contour Contour1(const GridGeom& grid);
//Build a bounding box
static BoundingBox BBox(const GridGeom& grid, double eps=geps);


//Finders
class CellFinder{
	const GridGeom* grid;
	const int Nx, Ny;
	double Hx, Hy;
	Point p0;
	vector<vector<const Cell*>> cells_by_square;

	int GetSquare(const Point& p) const;
	vector<const Cell*> CellCandidates(const Point& p) const;
	std::set<int> IndSet(const BoundingBox& bbox) const;
public:
	CellFinder(const GridGeom* g, int nx, int ny);
	//Find cell containing point.
	//Throws EOutOfArea if failed to find the point.
	const Cell* Find(const Point& p) const;
};



};



}








#endif
