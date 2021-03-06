#include <iostream>
#include "hmmapping.hpp"
#include "hmtesting.hpp"
#include "circrect.hpp"
#include "scpack_port.hpp"
#include "dscpack_port.hpp"
#include "confrect_fem.hpp"
#include "buildcont.hpp"
#include "partcont.hpp"
#include "assemble2d.hpp"
#include "treverter2d.hpp"
#include "assemble2d.hpp"
#include "buildgrid.hpp"
#include "infogrid.hpp"
#include "export2d_vtk.hpp"
using HMTesting::add_check;
using HMTesting::add_file_check;

void test01(){
	using namespace HMMap::Conformal::Impl::SCPack;
	std::cout<<"01. SCPACK conformal mapping"<<std::endl;
	auto left = HM2D::Contour::Constructor::FromPoints({
			Point {0.4, 0.2},
			Point {0.47662610, 0.22298783},
		});
	auto bottom = HM2D::Contour::Constructor::FromPoints({
			Point {0.4, 0.2},
			Point {0.46, 0.0},
		});
	auto right = HM2D::Contour::Constructor::FromPoints({
			Point {0.46, 0.0},
			Point {0.53990000, -0.00000002},
			});
	auto top = HM2D::Contour::Constructor::FromPoints({
			Point {0.47662610, 0.22298783},
			Point {0.53662610, 0.02298783},
			Point {0.53909957, 0.01196903},
			Point {0.53989935, 0.00401162},
			Point {0.53990000, -0.00000002},
		});
	auto trans = ToRect::Build(left, right, bottom, top);
	add_check(fabs(trans->module() - 2.73804)<1e-3, "Module");
	auto rect = trans->RectContour();
	double w = 0.678678;
	vector<Point> rectpnt { Point(w*trans->module(), 0), Point(w*trans->module(), 1) };
	vector<Point> polypnt = trans->MapToPolygon(rectpnt);
	add_check(
		fabs(polypnt[0].x - 0.442541 )<1e-6 &&
		fabs(polypnt[0].y - 0.0581967)<1e-6 &&
		fabs(polypnt[1].x - 0.519522 )<1e-6 &&
		fabs(polypnt[1].y - 0.0800001)<1e-6,
		"Map to Polygon");
}

void test02(){
	using namespace HMMap::Conformal::Impl;
	std::cout<<"02. Rectangle approximation of conformal mapping"<<std::endl;
	auto top = HM2D::Contour::Constructor::FromPoints({
		Point{0.43701993, 0.07659573},
		Point{0.44410482, 0.07840499},
		Point{0.45201250, 0.07960025},
		Point{0.46000000, 0.08000000},
		Point{1.00000000, 0.08000000}});

	auto right = HM2D::Contour::Constructor::FromPoints({
		Point(1,0), Point(1,0.08)});

	auto bottom = HM2D::Contour::Constructor::FromPoints({
		Point(0.46, 0.0), Point(1.0, 0.0)});

	auto left = HM2D::Contour::Constructor::FromPoints({
		Point(0.46, 0.0), Point(0.43701993, 0.07659573)});
	auto trans = RectApprox::Build(left, right, bottom, top);
	add_check(fabs(trans->module()-6.89702)<1e-5, "module");

	Point p(0.519522452, 0.08);
	Point torect = trans->MapToRectangle(p);
	Point topoly = trans->MapToPolygon(torect);
	Point diff = p-topoly;
	add_check(ISZERO(diff.x) && ISZERO(diff.y), "map bnd point: forward, backward");

	p = Point(0.47, 0.02);
	torect = trans->MapToRectangle(p);
	topoly = trans->MapToPolygon(torect);
	diff = p-topoly;
	add_check(ISZERO(diff.x) && ISZERO(diff.y), "map internal point: forward, backward");

}

void test03(){
	using namespace HMMap::Conformal::Impl::DSCPack;
	std::cout<<"03. DSCPack conformal mapping"<<std::endl;

	auto bot1 = HM2D::Contour::Constructor::Circle(8, 2, Point(0,0.1));
	auto top1 = HM2D::Contour::Constructor::Circle(10, 4, Point(0,0.1));
	auto trans1 = ToAnnulus::Build(top1, bot1);

	auto p10 = vector<Point> {Point(2.5, 1.5)};
	auto p11 = trans1->MapToAnnulus(p10);
	auto p12 = trans1->MapToOriginal(p11);
	add_check(
		ISEQ(trans1->PhiInner(0),trans1->PhiOuter(0)) &&
		ISEQ(trans1->PhiInner(4),trans1->PhiOuter(5)) &&
		p10[0] == p12[0],
		"Uniform points distribution"
	);
}

void test04(){
	std::cout<<"04. Conformal mapping to rectangle: FEM vs. SCPACK"<<std::endl;
	int sz1 = 13;
	auto r1 = HM2D::Contour::Constructor::Circle(sz1, 10, Point(0,0));
	auto inp1 = HMMap::Conformal::Rect::FactoryInput(r1, {0, sz1/4, sz1/2, 3*sz1/4});
	Point po(7.2, 1.1);

	HMMap::Conformal::Options opt;
	//fem 1
	auto trans1 = HMMap::Conformal::Impl::ConfFem::ToRect::Build(
			std::get<0>(inp1), sz1/4, sz1/2, 3*sz1/4, opt);
	Point p1  = trans1.MapToRectangle1(po);
	Point p11 = trans1.MapToPolygon1(p1);

	//fem 2
	auto trans2 = HMMap::Conformal::Impl::ConfFem::ToRect::Build(
			std::get<0>(inp1), sz1/4, sz1/2, 3*sz1/4, opt);
	Point p2  = trans2.MapToRectangle1(po);
	Point p22 = trans2.MapToPolygon1(p2);

	//scpack
	auto trans3 = HMMap::Conformal::Impl::SCPack::ToRect::Build(
			std::get<0>(inp1), sz1/4, sz1/2, 3*sz1/4);
	Point p3 =  trans3->MapToRectangle1(po);
	Point p33 = trans3->MapToPolygon1(p3);


	add_check(fabs(trans1.module() - 1.05)<0.02, "fem, even partition: module");
	add_check(fabs(trans2.module() - 1.05)<0.02, "fem, odd partition: module");
	add_check(fabs(trans3->module() - 1.05)<0.02, "scpack: module");

	add_check(po == p11, "fem, even: forward->backward");
	add_check(po == p22, "fem, odd: forward->backward");
	add_check(po == p33, "scpack: forward->backward");

	add_check(Point::dist(p1, p3)<0.01, "fem, even vs scpack: forward");
	add_check(Point::dist(p2, p3)<0.01, "fem, odd vs scpack: forward");
}

void test05(){
	std::cout<<"05. Conformal mapping to annulus: FEM vs. DSCPACK"<<std::endl;

	auto top = HM2D::Contour::Constructor::Circle(12, 1.5, Point(10,0.1));
	auto bot = HM2D::Contour::Constructor::Circle(5, 0.4, Point(10.5,0.2));
	vector<Point> topp, botp;
	for (auto p: HM2D::Contour::OrderedPoints(top)) topp.push_back(*p); topp.pop_back();
	for (auto p: HM2D::Contour::OrderedPoints(bot)) botp.push_back(*p); botp.pop_back();

	Point tp(11.12415, 0.40282);

	//dscpack
	auto trans1 = HMMap::Conformal::Impl::DSCPack::ToAnnulus::Build(topp, botp);
	Point tp1 = trans1->MapToAnnulus1(tp);
	Point tp11 = trans1->MapToOriginal1(tp1);
	add_check(Point::dist(tp, tp11)<1e-2, "dscpack forward->backward");

	//fem
	HMMap::Conformal::Options opt;
	//opt.fem_segment_partition = 5;
	auto trans2 = HMMap::Conformal::Impl::ConfFem::ToAnnulus::Build(
			topp, botp, opt);
	Point tp2 = trans2->MapToAnnulus1(tp);
	Point tp22 = trans2->MapToOriginal1(tp2);
	add_check(Point::dist(tp, tp22)<1e-2, "fem forward->backward");

	add_check(fabs(trans1->module() - trans2->module())<1e-2, "modulus");
	add_check([&]()->bool{
		double a1 = ToAngle(trans1->PhiInner(0) - trans1->PhiInner(2));
		double a2 = ToAngle(trans2->PhiInner(0) - trans2->PhiInner(2));
		if (fabs(a1-a2)>1e-1) return false;
		return true;
	}(), "Inner contour angles");

	add_check([&]()->bool{
		double a1 = ToAngle(trans1->PhiInner(0) - trans1->PhiOuter(3));
		double a2 = ToAngle(trans2->PhiInner(0) - trans2->PhiOuter(3));
		if (fabs(a1-a2)>1e-1) return false;
		return true;
	}(), "Outer contour angles");
}

void test06(){
	std::cout<<"06. Map grid from square"<<std::endl;
	HM2D::GridData base = HM2D::Grid::Constructor::RectGrid01(10, 10);
	auto cont = HM2D::Contour::Constructor::FromPoints({0,0, 1,0, 1.5, 1, 1, 1.3, 0.7, 1.0}, true);
	HM2D::GridData mapped = HMMap::MapGrid(base, cont,
		std::vector<Point>{Point(0, 0), Point(1, 0), Point(1,1), Point(0, 1)},
		std::vector<Point>{Point(0, 0), Point(1, 0), Point(1.5, 1), Point(0.7, 1.0)},
		false,
		HMMap::Options("inverse-laplace"));
	add_check(base.vcells.size() == mapped.vcells.size(), "cells number");
	add_check(base.vvert.size() == mapped.vvert.size(), "nodes number");
	add_check([&](){
		double a1 = HM2D::Contour::Area(cont);
		double a2 = HM2D::Grid::Area(mapped);
		if (a2>a1) return false;
		if (a1<0.9*a2) return false;
		return true;
	}(), "grids area");
	HM2D::Export::GridVTK(mapped, "g1.vtk");
}

void test07(){
	std::cout<<"07. Throw at invalid data"<<std::endl;
	HM2D::GridData base = HM2D::Grid::Constructor::Ring(Point(1,1), 2, 1, 36, 6);

	auto c1 = HM2D::Contour::Constructor::Circle(4, 5, Point(0, 0));
	auto c2 = HM2D::Contour::Constructor::Circle(18, 2, Point(0, 0.1));
	HM2D::EdgeData ecol;
	ecol.insert(ecol.end(), c1.begin(), c1.end());
	ecol.insert(ecol.end(), c2.begin(), c2.end());

	bool was_err1 = false;
	try{
		HM2D::GridData ans1 = HMMap::MapGrid(base, ecol,
			std::vector<Point> {Point(3, 1), Point(1, 3)},
			std::vector<Point> {Point(0, 5), Point(5, 0)},
			false, HMMap::Options("inverse-laplace")
		);
	} catch (HMMap::MapException &e) {
		was_err1 = e.what() == (std::string)"Grid mapping exception: All grid boundaries should contain at least one base point";
	}
	add_check(was_err1, "grid boundary has no base points");

	bool was_err2 = false;
	try{
		HM2D::GridData ans2 = HMMap::MapGrid(base, ecol,
			std::vector<Point> {Point(3, 1), Point(1, 3), Point(2, 1)},
			std::vector<Point> {Point(0, 5), Point(5, 0), Point(0, -5)},
			false, HMMap::Options("inverse-laplace")
		);
	} catch (HMMap::MapException &e) {
		was_err2 = e.what() == (std::string)"Grid mapping exception: Contour-to-contour links are ambiguous";
	}
	add_check(was_err2, "points from same grid boundary are referenced to different mapped area");

	bool was_err3 = false;
	try{
		HM2D::GridData ans3 = HMMap::MapGrid(base, ecol,
			std::vector<Point> {Point(3, 1), Point(1, 3), Point(3, 3), Point(2, 1), Point(0, 1)},
			std::vector<Point> {Point(5, 0), Point(0, 5), Point(0, -5), Point(2, 0), Point(-2, 0)},
			false, HMMap::Options("inverse-laplace")
		);
	} catch (HMMap::MapException &e) {
		was_err3 = e.what() == (std::string)"Grid mapping exception: Invalid order of points in mapped contour";
	}
	add_check(was_err3, "invalid points ordering");

	try{
		HMMap::Options opt("inverse-laplace");
		opt.fem_nrec = 1000;
		HM2D::GridData ans4 = HMMap::MapGrid(base, ecol,
			std::vector<Point> {Point(3, 1), Point(1, 3), Point(3, 3), Point(2, 1), Point(0, 1)},
			std::vector<Point> {Point(5, 0), Point(0, 5), Point(5, 5), Point(2, 0.1), Point(-2, 0.1)},
			false, opt
		);
		HM2D::Export::GridVTK(ans4, "g1.vtk");
		add_file_check(12327043228302344750U, "g1.vtk", "valid doubly connected data");
	} catch (HMMap::MapException &e) {add_check(false, "valid doubly connected data");}
}

void test08(){
	std::cout<<"08. Circle vs rectangle"<<std::endl;
	HM2D::GridData circgrid = HM2D::Grid::Constructor::Circle(Point(2, 3), 2, 16, 6, false);
	auto cont4 = HM2D::Contour::Constructor::FromPoints({0,0, 3,0, 3,2, 0,2}, true);
	HM2D::GridData rectgrid = HM2D::Grid::Constructor::RectGrid01(5, 7);
	auto contc = HM2D::Contour::Constructor::Circle(200, 16, Point(8, 9));

	HM2D::GridData ans1 = HMMap::MapGrid(circgrid, cont4,
			std::vector<Point> {Point(4, 3)},
			std::vector<Point> {Point(0, 0)},
			false, HMMap::Options("inverse-laplace")
	);
	add_check(ans1.vvert.size() == circgrid.vvert.size() &&
		  ans1.vcells.size() == circgrid.vcells.size(), "circle to rectangle, one base point");

	HM2D::GridData ans2 = HMMap::MapGrid(circgrid, cont4,
			std::vector<Point> {Point(4, 3), Point(2, 5)},
			std::vector<Point> {Point(0, 0), Point(3, 0)},
			false, HMMap::Options("inverse-laplace")
	);
	add_check(ans2.vvert.size() == circgrid.vvert.size() &&
		  ans2.vcells.size() == circgrid.vcells.size(), "circle to rectangle, two base points");

	HM2D::GridData ans3 = HMMap::MapGrid(circgrid, cont4,
			std::vector<Point> {Point(4, 3), Point(2, 5), Point(2, 1), Point(0, 3)},
			std::vector<Point> {Point(0, 0), Point(3, 0), Point(0, 2), Point(3,2)},
			false, HMMap::Options("inverse-laplace")
	);
	add_check(ans3.vvert.size() == circgrid.vvert.size() &&
		  ans3.vcells.size() == circgrid.vcells.size(), "circle to rectangle, four base points");

	HM2D::GridData ans4 = HMMap::MapGrid(rectgrid, contc,
	                std::vector<Point> {Point(0, 0)},
	                std::vector<Point> {Point(-8, 9)},
			false, HMMap::Options("inverse-laplace")
	);
	add_check(ans4.vvert.size() == rectgrid.vvert.size() &&
	          ans4.vcells.size() == rectgrid.vcells.size(), "rectangle to circle, one base point");
}

void test09(){
	std::cout<<"09. Snapping"<<std::endl;
	HM2D::GridData rectgrid = HM2D::Grid::Constructor::RectGrid01(10, 10);
	auto mcont = HM2D::Contour::Constructor::FromPoints(
		{0,0, 2,2, 1,3, -4,3}, true);
	HMMap::Options opt("inverse-laplace");
	opt.fem_nrec = 5000;

	opt.snap = "ADD_VERTICES";
	HM2D::GridData ans1 = HMMap::MapGrid(rectgrid, mcont,
			std::vector<Point> {Point(0, 0)},
			std::vector<Point> {Point(0, 0)}, false, opt);
	add_check(fabs(HM2D::Contour::Area(mcont) - HM2D::Grid::Area(ans1))<1e-12 &&
	          rectgrid.vvert.size() + 3 == ans1.vvert.size(),
		"snapping by adding points");
	HM2D::Export::GridVTK(ans1, "ans1.vtk");

	opt.snap = "SHIFT_VERTICES";
	HM2D::GridData ans2 = HMMap::MapGrid(rectgrid, mcont, vector<Point>{Point(0,0)}, vector<Point> {Point(0,0)}, false, opt);
	add_check(fabs(HM2D::Contour::Area(mcont) - HM2D::Grid::Area(ans2))<1e-12 &&
	          rectgrid.vvert.size() == ans2.vvert.size(),
		"snapping by shifting points");
	HM2D::Export::GridVTK(ans2, "ans2.vtk");
}

void test10(){
	std::cout<<"10. Circle with rectangle cells"<<std::endl;
	{
		HM2D::GridData g1 = HMMap::Circ4Prototype(Point(0, 0), 1.0, 16, "linear", 0.5, 1.0);
		HM2D::GridData g2 = HMMap::Circ4Prototype(Point(0, 0), 1.0, 16, "laplace", 0.5, 1.0);
		HM2D::GridData g3 = HMMap::Circ4Prototype(Point(0, 0), 1.0, 16, "orthogonal-circ", 0.5, 1.0);
		HM2D::GridData g4 = HMMap::Circ4Prototype(Point(0, 0), 1.0, 16, "orthogonal-rect", 0.5, 1.0);
		HM2D::Export::GridVTK(g1, "g1.vtk");
		HM2D::Export::GridVTK(g2, "g2.vtk");
		HM2D::Export::GridVTK(g3, "g3.vtk");
		HM2D::Export::GridVTK(g4, "g4.vtk");
		add_file_check(15903419327826768668U, "g1.vtk", "linear algo");
		add_file_check(7968599338814609544U, "g2.vtk", "laplace algo");
		add_file_check(6805025905648782307U, "g3.vtk", "orthogonal-circ algo");
		add_file_check(14747674935011048887U, "g4.vtk", "orthogonal-rect algo");
	}
	{
		HM2D::GridData g1 = HMMap::Circ4Prototype(Point(0, 0), 1.0, 24, "laplace", 1.0, 1.0);
		HM2D::Export::GridVTK(g1, "g1.vtk");
		auto sk = HM2D::Grid::Skewness(g1);
		auto maxel = std::max_element(sk.begin(), sk.end());
		add_check(ISZERO(*maxel - 0.5), "skewness");
		add_file_check(13271040584486753269U, "g1.vtk", "no refinement, side = 1.0*rad");

		HM2D::GridData g2 = HMMap::Circ4Prototype(Point(3, 1), 2.0, 24, "laplace", 1.0, 0.3);
		HM2D::Export::GridVTK(g2, "g2.vtk");
		add_file_check(1042103010747200583U, "g2.vtk", "with refinement, side = 1.0*rad");

		HM2D::GridData g3 = HMMap::Circ4Prototype(Point(-3, 1), 0.2, 80, "laplace", 0.5, 1.0);
		HM2D::Export::GridVTK(g3, "g3.vtk");
		add_file_check(2561153484233905875U, "g3.vtk", "side = 0.5*rad");
	}
}

void test11(){
	std::cout<<"11. M-like target"<<std::endl;
	HM2D::GridData g1 = HM2D::Grid::Constructor::RectGrid01(10, 10);
	auto cont = HM2D::Contour::Constructor::FromPoints({0,0, 1,0, 1,1, 0.5, 0.2, 0,1}, true);

	auto ans = HMMap::MapGrid.Silent(g1, cont,
			std::vector<Point> {Point(0,0), Point(1,0), Point(1,1), Point(0,1)},
			std::vector<Point> {Point(0,0), Point(1,0), Point(1,1), Point(0,1)},
			false, HMMap::Options("inverse-laplace")
	);
	HM2D::Export::GridVTK(ans, "g1.vtk");
	add_file_check(5133065268170907878U, "g1.vtk", "m-like target, inverse");

	auto g1cont = HM2D::Contour::Assembler::GridBoundary1(g1);
	auto ans2 = HMMap::MapGrid.Silent(ans, g1cont,
			std::vector<Point> {Point(0,0), Point(1,0), Point(1,1), Point(0,1)},
			std::vector<Point> {Point(0,0), Point(1,0), Point(1,1), Point(0,1)},
			false, HMMap::Options("direct-laplace")
	);
	HM2D::Export::GridVTK(ans2, "g1.vtk");
	add_file_check(4588482714625395358U, "g1.vtk", "m-like base, direct");
}

HM2D::EdgeData PerturbedContour(Point p1, Point p2, int npart,
		std::function<double(double)> perturbation){
	vector<Point> pp(npart+1);
	pp[0] = p1;
	pp.back() = p2;
	Vect pvec = p2 - p1;
	for (int i=1; i<npart; ++i){
		double t = (double)i/(npart);
		double m = perturbation(t);
		Vect perpvec(-pvec.y, pvec.x);
		if (m<0) perpvec *= -1.0;
		vecSetLen(perpvec, fabs(m));
		pp[i] = Point::Weigh(p1, p2, t);
		pp[i] += perpvec;
	}
	return HM2D::Contour::Constructor::FromPoints(pp);
}

void test12(){
	std::cout<<"12. Orthonal grid in curvilinear quadrangle"<<std::endl;
	{
		//initial contours
		HM2D::VertexData pcol;
		auto left1 = PerturbedContour(Point(0, 0), Point(-0.11, 1), 100,
				[](double x){ return 0.2*sin(2*M_PI*x); });
		auto bot1 = PerturbedContour(Point(0, 0), Point(3, 0), 100,
				[](double x){ return 0.13*sin(8*M_PI*x); });
		auto right1 = PerturbedContour(Point(3, 0), Point(3.06, 1), 100,
				[](double x){ return 0.07*sin(4*M_PI*x); });
		auto top1 = PerturbedContour(Point(-0.11, 1), Point(3.06, 1), 100,
				[](double x){ return 0.1*sin(6*M_PI*x); });
		//partition
		auto left = HM2D::Contour::Algos::Partition(0.1, left1);
		auto bot = HM2D::Contour::Algos::Partition(0.1, bot1);
		//build grid
		HM2D::GridData ans = HMMap::OrthogonalRectGrid(left, bot, right1, top1);
		HM2D::Export::GridVTK(ans, "g1.vtk");
		add_file_check(15045105319010820220U, "g1.vtk", "grid");
	}
}
void test13(){
	std::cout<<"13. Laplace algorithms for custom rectangle"<<std::endl;
	{
		auto left1 = PerturbedContour(Point(-0.1, -0.1), Point(-0.0, 0.98), 100,
				[](double x){ return 0.1*sin(M_PI*x); });
		auto bot1 = PerturbedContour(Point(-0.1, -0.1), Point(2.1, 0.1), 100,
				[](double x){ return 0.05*sin(2*M_PI*x); });
		auto right1 = PerturbedContour(Point(2.1, 0.1), Point(2.8, 1.4), 100,
				[](double x){ return 0.07*sin(4*M_PI*x); });
		auto top1 = PerturbedContour(Point(-0.0, 0.98), Point(2.8, 1.4), 100,
				[](double x){ return 0.05*sin(8*M_PI*x); });
		std::map<double, double> m;

		m.clear(); m[0] = 0.2; m[0.3] = 0.03; m[1]=0.1;
		auto left = HM2D::Contour::Algos::WeightedPartition(m, left1);
		m.clear(); m[0] = 0.1; m[0.5]=0.07; m[1]=0.15;
		auto right = HM2D::Contour::Algos::WeightedPartition(m, right1);

		m.clear(); m[0] = 0.02; m[0.6]=0.2; m[1.0]=0.07;
		auto bot = HM2D::Contour::Algos::WeightedPartition(m, bot1);
		m.clear(); m[0] = 0.04; m[0.3]=0.2; m[1.0]=0.09;
		auto top = HM2D::Contour::Algos::WeightedPartition(m, top1);

		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(left, bot, right, top, "inverse-laplace");
		HM2D::Export::GridVTK(ans1, "g1.vtk");
		add_file_check(249036200226773492U, "g1.vtk", "inverse algorithm");

		HM2D::GridData ans2 = HMMap::LaplaceRectGrid(left, bot, right, top, "direct-laplace");
		HM2D::Export::GridVTK(ans2, "g2.vtk");
		add_file_check(8128882274083553470U, "g2.vtk", "direct algorithm");

		add_check(ISEQ(HM2D::Grid::Area(ans1), HM2D::Grid::Area(ans2)), "areas equality");
	}
}

void test14(){
	std::cout<<"14. Different source locations for custom rectangle"<<std::endl;
	auto left1 = PerturbedContour(Point(0.1, -0.1), Point(0.0, 1.3), 10,
			[](double x){ return 0.03*sin(M_PI*x); });
	auto bot1 = PerturbedContour(Point(0.1, -0.1), Point(3, 0), 25,
			[](double x){ return 0.05*sin(2*M_PI*x); });
	auto right1 = PerturbedContour(Point(3, 0), Point(3.2, 1.0), 10,
			[](double x){ return 0.07*sin(4*M_PI*x); });
	auto top1 = PerturbedContour(Point(0.0, 1.3), Point(3.2, 1.0), 25,
			[](double x){ return 0.02*sin(8*M_PI*x); });
	HM2D::GridData ans = HMMap::LaplaceRectGrid(left1, bot1, right1, top1, "direct-laplace");
	HM2D::Export::GridVTK(ans, "g1.vtk");
	//Inversions
	{
		auto top = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(top1));
		HM2D::Contour::R::ReallyRevert::Permanent(top);
		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(left1, bot1, right1, top, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_file_check("g1.vtk", "g2.vtk", "top reversed");
	}
	{
		auto left = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(left1));
		HM2D::Contour::R::ReallyRevert::Permanent(left);
		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(left, bot1, right1, top1, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_file_check("g1.vtk", "g2.vtk", "left reversed");
	}
	{
		auto top = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(top1));
		auto left = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(left1));
		auto bot = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(bot1));
		auto right = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(right1));
		HM2D::Contour::R::ReallyRevert::Permanent(top);
		HM2D::Contour::R::ReallyRevert::Permanent(left);
		HM2D::Contour::R::ReallyRevert::Permanent(bot);
		HM2D::Contour::R::ReallyRevert::Permanent(right);
		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(left, bot, right, top, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_file_check("g1.vtk", "g2.vtk", "all reversed");
	}
	//swaps
	{
		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(bot1, right1, top1, left1, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_check(ISEQ(HM2D::Grid::Area(ans1), HM2D::Grid::Area(ans)), "bot->right->top->left");
	}
	{
		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(right1, bot1, left1, top1, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_check(ISEQ(HM2D::Grid::Area(ans1), HM2D::Grid::Area(ans)), "right->bot->left->top");
	}
	{
		auto top = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(top1));
		HM2D::Contour::R::ReallyRevert::Permanent(top);
		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(top, right1, bot1, left1, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_check(ISEQ(HM2D::Grid::Area(ans1), HM2D::Grid::Area(ans)), "top->right->bot->left");
	}
	//not connected data
	{
		auto top2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(top1));
		auto left2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(left1));
		auto bot2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(bot1));
		auto right2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(right1));
		HM2D::EdgeData top, left, bot, right;
		HM2D::DeepCopy(top2, top);
		HM2D::DeepCopy(left2, left);
		HM2D::DeepCopy(right2, right);
		HM2D::DeepCopy(bot2, bot);

		HM2D::Contour::R::ReallyRevert::Permanent(left);
		for (auto p: HM2D::Contour::OrderedPoints(top)) *p+=Point(0.2, 1.2);
		for (auto p: HM2D::Contour::OrderedPoints(right)) *p+=Point(-0.5, 0);

		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(left, bot, right, top, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g2.vtk");
		add_check(ISEQ(HM2D::Grid::Area(ans1), HM2D::Grid::Area(ans)), "moved top, right");

		for (auto p: HM2D::Contour::OrderedPoints(top)) *p+=Point(0.2, 1.2);
		for (auto p: HM2D::Contour::OrderedPoints(right)) *p+=Point(-0.5, 0);
		HM2D::GridData ans2 = HMMap::LaplaceRectGrid(top, left, bot, right, "direct-laplace");
		HM2D::Export::GridVTK(ans2, "g2.vtk");
		add_check(ISEQ(HM2D::Grid::Area(ans2), HM2D::Grid::Area(ans))
			&& *ans2.vvert[0] == Point(0.2, 2.5), "moved top, right; base top");
	}
	//equal data
	{
		auto top2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(top1));
		auto left2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(left1));
		auto bot2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(top1));
		auto right2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(left1));
		HM2D::EdgeData top, left, bot, right;
		HM2D::DeepCopy(top2, top);
		HM2D::DeepCopy(left2, left);
		HM2D::DeepCopy(right2, right);
		HM2D::DeepCopy(bot2, bot);

		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(top, left, bot, right, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g3.vtk");
		add_file_check(11091658640678329136U, "g3.vtk", "top & left");
	}
	{
		auto right2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(right1));
		auto bot2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(bot1));
		auto left2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(right1));
		auto top2 = HM2D::Contour::Assembler::Contour1(HM2D::Contour::OrderedPoints(bot1));
		HM2D::EdgeData top, left, bot, right;
		HM2D::DeepCopy(top2, top);
		HM2D::DeepCopy(left2, left);
		HM2D::DeepCopy(right2, right);
		HM2D::DeepCopy(bot2, bot);

		HM2D::GridData ans1 = HMMap::LaplaceRectGrid(right, bot, left, top, "direct-laplace");
		HM2D::Export::GridVTK(ans1, "g4.vtk");
		add_file_check(15643697133166545954U, "g4.vtk", "right & bot");
	}
}

void test15(){
	std::cout<<"15. Transfinite Interpolation"<<std::endl;
	{
		auto left = HM2D::Contour::Constructor::FromPoints({0,0, 0,1});
		auto bot = PerturbedContour(Point(0, 0), Point(1, 0), 100,
			       [](double t){return 0.15*sin(2*M_PI*3*t);});
		auto right = HM2D::Contour::Constructor::FromPoints({1,0, 1,1});
		auto top = HM2D::Contour::Constructor::FromPoints({0,1, 1,1});
		std::map<double, double> m;
		m.clear(); m[0] = 0.05; m[1] = 0.2;
		auto left1 = HM2D::Contour::Algos::WeightedPartition(m, left);
		m.clear(); m[0] = 0.2; m[1] = 0.05;
		auto right1 = HM2D::Contour::Algos::WeightedPartition(m, right);
		m.clear(); m[0] = 0.018; m[1] = 0.1;
		auto top1 = HM2D::Contour::Algos::WeightedPartition(m, top);
		m.clear(); m[0] = 0.16; m[1] = 0.1;
		auto bot1 = HM2D::Contour::Algos::WeightedPartition(m, bot, 21);

		HM2D::GridData g1 = HMMap::LinearTFIRectGrid(left1, bot1, right1, top1);
		HM2D::Export::GridVTK(g1, "g1.vtk");
		add_file_check(8469727367193026329U, "g1.vtk", "1.linear tfi");
	}
	{
		auto left = PerturbedContour(Point(0, 0), Point(0.05, 0.9), 10,
				[](double t){return 0.10*sin(2*M_PI*t);});
		auto bot = PerturbedContour(Point(0, 0), Point(1, 0.1), 7,
				[](double t){return 0.04*sin(2*M_PI*1.5*t);});
		auto right = PerturbedContour(Point(1, 0.1), Point(0.9, 1.05), 10,
				[](double t){return 0.07*sin(-2*M_PI*t);});
		auto top = PerturbedContour(Point(0.05, 0.9), Point(0.9, 1.05), 7,
				[](double t){return 0.2*sin(M_PI*t);});
		HM2D::GridData g1 = HMMap::LinearTFIRectGrid(left, bot, right, top);
		HM2D::GridData g2 = HMMap::CubicTFIRectGrid(left, bot, right, top, {1, 1, 1, 1});
		HM2D::GridData g3 = HMMap::CubicTFIRectGrid(left, bot, right, top, {1, 0, 1, 0});
		HM2D::Export::GridVTK(g1, "g1.vtk");
		HM2D::Export::GridVTK(g2, "g2.vtk");
		HM2D::Export::GridVTK(g3, "g3.vtk");
		add_file_check(16471099325635865985U, "g1.vtk", "2.linear tfi");
		add_file_check(8146930717215035548U, "g2.vtk", "2.hermite tfi, 1");
		add_file_check(960387211664608807U, "g3.vtk", "2.hermite tfi, 1010");
	}
}

int main(){
	test01();
	test02();
	test03();
	test04();
	test05();
	test06();
	test07();
	test08();
	test09();
	test10();
	test11();
	test12();
	test13();
	test14();
	test15();

	HMTesting::check_final_report();
	std::cout<<"DONE"<<std::endl;
	return 0;
}

