#include "hmblay.hpp"
#include "fileproc.h"

int FAILED_CHECKS = 0;

void add_check(bool ex, std::string info){
	if (info.size()==0){
		std::cout<<"\tunknown check: ";
	} else{
		std::cout<<"\t"<<info;
	}
	if (ex){
		std::cout<<": True"<<std::endl;
	} else {
		++FAILED_CHECKS;
		std::cout<<": False <<<<<<<<<<<<<<<<<<<"<<std::endl;
	}
};


void test01(){
	std::cout<<"01. Boundary Layers grid from circle"<<std::endl;
	std::string cn;
	// 1. Build Edges Collection 
	auto col = HMCont2D::Constructor::Circle(8, 2.0, Point(0, 0));
	// 2. set inp
	HMBlay::Input inp;
	inp.edges = &col;
	inp.direction = HMBlay::DirectionFromString("INNER");
	inp.bnd_step_method = HMBlay::MethFromString("NO");
	inp.partition = {0.0, 0.2, 0.4, 1.5};
	inp.bnd_step = 0.1;
	inp.round_off = true;
	inp.start=inp.end=Point(0,0);
	inp.sharp_angle = inp.corner_angle = 0.0;
	inp.regular_angle = 300;
	inp.start = inp.end = Point(0,0);
	//3. Calculations
	cn = "Inner full circle";
	GridGeom Ans1 = HMBlay::BuildBLayerGrid({inp});
	add_check(fabs(Ans1.area() - 10.606602)<0.00001, cn);
	HMCont2D::SaveVtk(col, "orig.vtk");

	cn = "Outer full circle";
	inp.direction = HMBlay::DirectionFromString("OUTER");
	GridGeom Ans2 = HMBlay::BuildBLayerGrid({inp});
	add_check(fabs(Ans2.area() - 23.3345237) <1e-6, cn);
}

void test02(){
	std::cout<<"02. Smoothing normals"<<std::endl;
	
	//basic inp
	HMBlay::Input inp;
	inp.direction = HMBlay::DirectionFromString("OUTER");
	inp.bnd_step_method = HMBlay::MethFromString("KEEP_SHAPE");
	inp.partition = {0.0, 0.2, 0.4, 1.5};
	inp.bnd_step = 0.05;
	inp.start=inp.end=Point(0,0);
	inp.sharp_angle = inp.corner_angle = 0.0;
	inp.regular_angle = 300;
	inp.start = inp.end = Point(0,0);
	inp.smooth_normals_steps = 10;

	std::string cn("8-side polygon with outer layer");
	auto col1 = HMCont2D::Constructor::Circle(8, 1.3, Point(2, 2));
	inp.edges = &col1;
	GridGeom Ans1 = HMBlay::BuildBLayerGrid({inp});

	vector<double> area1;
	for(int i = 2; i<Ans1.n_cells(); i+=3) area1.push_back(Ans1.get_cell(i)->area());
	//maximum outer element is bigger not more then two times then minimum
	double mina1 = *(std::min_element(area1.begin(), area1.end()));
	double maxa1 = *(std::max_element(area1.begin(), area1.end()));
	add_check(mina1>0 && maxa1/mina1<2.1, cn);

	cn = std::string("8-side polygon with inner layer");
	inp.direction = HMBlay::DirectionFromString("INNER");
	inp.partition = {0.0, 0.02, 0.08, 0.2};
	GridGeom Ans2 = HMBlay::BuildBLayerGrid({inp});
	vector<double> area2;
	for(int i = 2; i<Ans2.n_cells(); i+=3) area2.push_back(Ans2.get_cell(i)->area());
	//maximum outer element is bigger not more then two times then minimum
	double mina2 = *(std::min_element(area2.begin(), area2.end()));
	double maxa2 = *(std::max_element(area2.begin(), area2.end()));
	add_check(mina2>0 && maxa2/mina2<2.1, cn);
};


void test03(){
	std::cout<<"03. Open contour boundary layer"<<std::endl;
	
	//basic inp
	HMBlay::Input inp;
	inp.bnd_step_method = HMBlay::MethFromString("KEEP_ALL");
	inp.partition = {0.0, 0.2, 0.4, 1.5};
	inp.bnd_step = 0.3;
	inp.start=inp.end=Point(0,0);
	inp.sharp_angle = inp.corner_angle = 0.0;
	inp.regular_angle = 300;
	inp.smooth_normals_steps = 0;

	std::string cn("5-points polyline. Left layer");
	auto col1 = HMCont2D::Constructor::ContourFromPoints(
			{0,0, 1,1, 2,1.9, 4,3.5, 7,6.1});
	inp.direction = HMBlay::DirectionFromString("LEFT");
	inp.start = Point(0,0);
	inp.end = Point(7,6.1);
	inp.edges = &col1;
	GridGeom Ans1 = HMBlay::BuildBLayerGrid({inp});
	add_check([&](){
		for (int i=0; i<Ans1.n_cells(); ++i) if (Ans1.get_cell(i)->area()<0) return false;
		for (int i=0; i<Ans1.n_points(); ++i) if (Ans1.get_point(i)->y<0.0) return false;
		return true;
	}(), cn);

	cn = std::string("5-points polyline. Right layer");
	inp.direction = HMBlay::DirectionFromString("RIGHT");
	GridGeom Ans2 = HMBlay::BuildBLayerGrid({inp});
	add_check([&](){
		for (int i=0; i<Ans2.n_cells(); ++i) if (Ans2.get_cell(i)->area()<0) return false;
		for (int i=0; i<Ans2.n_points(); ++i) if (Ans2.get_point(i)->y>6.1) return false;
		return true;
	}(), cn);

	cn = std::string("5-points polyline. Right layer. Opposite direction");
	std::swap(inp.start, inp.end);
	GridGeom Ans3 = HMBlay::BuildBLayerGrid({inp});
	add_check([&](){
		for (int i=0; i<Ans3.n_cells(); ++i) if (Ans3.get_cell(i)->area()<0) return false;
		for (int i=0; i<Ans3.n_points(); ++i) if (Ans3.get_point(i)->y<0.0) return false;
		return true;
	}(), cn);
}

void test04(){
	auto col1 = HMCont2D::Constructor::Circle(8, 10, Point(0, 0));
	//basic inp1
	HMBlay::Input inp1;
	inp1.bnd_step_method = HMBlay::MethFromString("KEEP_ALL");
	inp1.sharp_angle = inp1.corner_angle = 0.0;
	inp1.regular_angle = 300;
	inp1.smooth_normals_steps = 50;
	inp1.direction = HMBlay::DirectionFromString("OUTER");
	inp1.edges = &col1;

	std::string cn = std::string("mesh half of closed contour");
	inp1.partition = {0, 0.1, 0.2, 0.3, 0.4};
	inp1.start = Point(10, 0);
	inp1.end = Point(-10, 0);
	inp1.bnd_step = 0.3;
	GridGeom Ans1 = HMBlay::BuildBLayerGrid({inp1});
	add_check([&](){
		for (int i=0; i<Ans1.n_cells(); ++i) if (Ans1.get_cell(i)->area()<0) return false;
		for (int i=0; i<Ans1.n_points(); ++i) if (Ans1.get_point(i)->y<0.0) return false;
		return true;
	}(), cn);

	cn = std::string("Two layer with same partition depth count: outer");
	HMBlay::Input inp2(inp1);
	inp2.partition = {0, 0.1, 0.2, 0.3, 0.8};
	std::swap(inp2.start, inp2.end);
	inp2.bnd_step = 0.1;
	GridGeom Ans2 = HMBlay::BuildBLayerGrid({inp1, inp2});
	add_check([&](){
		for (int i=0; i<Ans2.n_cells(); ++i) if (Ans2.get_cell(i)->area()<0) return false;
		if (Ans2.n_cells()/4 != Ans2.n_points()/5) return false;
		for (int i=0; i<Ans2.n_points(); ++i) if (Ans2.get_point(i)->y<-10-0.8-1e-12) return false;
		for (int i=0; i<Ans2.n_points(); ++i) if (Ans2.get_point(i)->y>10+0.4+1e-12) return false;
		return true;
	}(), cn);

	cn = std::string("Two layer with same partition depth count: inner");
	inp1.direction = inp2.direction = HMBlay::DirectionFromString("INNER");
	GridGeom Ans3 = HMBlay::BuildBLayerGrid({inp1, inp2});
	add_check([&](){
		for (int i=0; i<Ans3.n_cells(); ++i) if (Ans3.get_cell(i)->area()<0) return false;
		if (Ans3.n_cells()/4 != Ans3.n_points()/5) return false;
		for (int i=0; i<Ans3.n_points(); ++i){
			const GridPoint* p=Ans3.get_point(i);
			if (p->y>10+1e-12 || p->y<-10-1e-12) return false;
			if (ISEQ(p->x, 0) && (p->y>-10+0.8+1e-12 && p->y<10-0.4-1e-12)) return false;
		}
		return true;
	}(), cn);

	cn = std::string("Two layer with different partition depth count");
	inp1.direction = inp2.direction = HMBlay::DirectionFromString("OUTER");
	inp2.partition.push_back(1.0); inp2.partition.push_back(1.2);
	inp1.smooth_normals_steps = inp2.smooth_normals_steps = 0;
	GridGeom Ans4 = HMBlay::BuildBLayerGrid({inp1, inp2});
	add_check([&](){
		for (int i=0; i<Ans4.n_cells(); ++i) if (Ans4.get_cell(i)->area()<0) return false;
		int cp = 0;
		for (int i=0; i<Ans4.n_points(); ++i){
			if (*Ans4.get_point(i) == Point(-10.8, 0) || *Ans4.get_point(i) == Point(10.8, 0)) ++cp;
		}
		if (cp!=2) return false;
		return true;
	}(), cn);
}


void test05(){
	std::cout<<"05. Non-right boundary angle at the ends"<<std::endl;
	auto col1 = HMCont2D::Constructor::ContourFromPoints(
		{0,0, 5,0, 6,4, 2,4}, true);

	HMBlay::Input inp1;
	inp1.bnd_step_method = HMBlay::MethFromString("KEEP_ALL");
	inp1.sharp_angle = inp1.corner_angle = 0.0;
	inp1.regular_angle = 300;
	inp1.smooth_normals_steps = 0;
	inp1.direction = HMBlay::DirectionFromString("INNER");
	inp1.edges = &col1;
	inp1.partition = {0, 0.1, 0.2, 0.3, 0.4};
	inp1.start = Point(0, 0);
	inp1.end = Point(5, 0);
	inp1.bnd_step = 0.1;

	std::string cn = std::string("mesh half of closed contour");
	GridGeom Ans1 = HMBlay::BuildBLayerGrid({inp1});
	std::cout<<Ans1.area()<<std::endl;
	add_check([&](){
		if (fabs(Ans1.area()-1.97)>0.01) return false;
		for (int i=0; i<Ans1.n_cells(); ++i) if (Ans1.get_cell(i)->area()<0) return false;
		return true;
	}(), cn);
}


void test06(){
	std::cout<<"06. 1 segment sources"<<std::endl;
	auto col1 = HMCont2D::Constructor::ContourFromPoints(
			{0,0, 2,0});

	HMBlay::Input inp1;
	inp1.bnd_step_method = HMBlay::MethFromString("NO");
	inp1.sharp_angle = inp1.corner_angle = 0.0;
	inp1.regular_angle = 300;
	inp1.smooth_normals_steps = 0;
	inp1.partition = {0, 0.1};
	inp1.bnd_step = 0.1;

	std::string cn = std::string("left from positive");
	inp1.edges = &col1;
	inp1.direction = HMBlay::DirectionFromString("LEFT");
	inp1.start = Point(0, 0);
	inp1.end = Point(2, 0);
	GridGeom Ans1 = HMBlay::BuildBLayerGrid({inp1});
	add_check([&](){
		if (Ans1.n_cells() != 1) return false;
		for (int i=0; i<Ans1.n_points(); ++i) if (Ans1.get_point(i)->y<-1e-12) return false;
		return true;
	}(), cn);

	cn = std::string("right from positive");
	inp1.direction = HMBlay::DirectionFromString("RIGHT");
	GridGeom Ans2 = HMBlay::BuildBLayerGrid({inp1});
	add_check([&](){
		if (Ans2.n_cells() != 1) return false;
		for (int i=0; i<Ans2.n_points(); ++i) if (Ans2.get_point(i)->y>1e-12) return false;
		return true;
	}(), cn);

	cn = std::string("left from negative");
	std::swap(inp1.start, inp1.end);
	inp1.direction = HMBlay::DirectionFromString("LEFT");
	GridGeom Ans3 = HMBlay::BuildBLayerGrid({inp1});
	add_check([&](){
		if (Ans3.n_cells() != 1) return false;
		for (int i=0; i<Ans3.n_points(); ++i) if (Ans3.get_point(i)->y>1e-12) return false;
		return true;
	}(), cn);

	cn = std::string("right from negative");
	inp1.direction = HMBlay::DirectionFromString("RIGHT");
	GridGeom Ans4 = HMBlay::BuildBLayerGrid({inp1});
	add_check([&](){
		if (Ans4.n_cells() != 1) return false;
		for (int i=0; i<Ans4.n_points(); ++i) if (Ans4.get_point(i)->y<-1e-12) return false;
		return true;
	}(), cn);

	cn = std::string("right from negative. fine partition");
	inp1.bnd_step_method = HMBlay::MethFromString("IGNORE_ALL");
	GridGeom Ans5 = HMBlay::BuildBLayerGrid({inp1});
	add_check([&](){
		if (Ans5.n_cells() != 20) return false;
		for (int i=0; i<Ans5.n_points(); ++i) if (Ans5.get_point(i)->y<-1e-12) return false;
		return true;
	}(), cn);
}


void test07(){
	std::cout<<"07. Corner angle algorithm basic test"<<std::endl;
}

void test08(){
	std::cout<<"08 Obtuse angle algorithm basic test"<<std::endl;
}

void test09(){
	std::cout<<"09 Throw on crossing intervals"<<std::endl;
}

int main(){
	//test01();
	//test02();
	//test03();
	//test04();
	//test05();
	test06();
	//test07();
	//test08();
	//test09();

	if (FAILED_CHECKS ==1){
		std::cout<<FAILED_CHECKS<<" test failed <<<<<<<<<<<<<<<<<<<"<<std::endl;
	} else if (FAILED_CHECKS > 1) {
		std::cout<<FAILED_CHECKS<<" tests failed <<<<<<<<<<<<<<<<<<<"<<std::endl;
	} else {
		std::cout<<"All tests passed"<<std::endl;
	}
	std::cout<<"DONE"<<std::endl;
}
