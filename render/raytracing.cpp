#include "raytracing.h"
#include <string>
#include "../core/object.h"
#include "../objects/sphere.h"
#include "../objects/area_light.h"
#include "../objects/plane.h"
#include "glog/logging.h"
#include <iostream>

RayTracing::RayTracing() {
	camera = NULL;
}
RayTracing::~RayTracing() {
	if (camera)
		delete camera;
}

void RayTracing::accept(const Json::Value& val) {
	DLOG(INFO)<<"RayTracing : "<<val.toStyledString()<<std::endl;
	DLOG(INFO)<<"RayTracing : Init camera"<<std::endl;

	if (val["camera"]["type"].asString() == "default") {
		camera = new Camera();
		camera->accept(val["camera"]);
	}

	DLOG(INFO)<<"RayTracing : Init objects"<<std::endl;
	for (int i=0;i<val["objects"].size();i++) {
		Json::Value v = val["objects"][i];
		std::string tag = v["type"].asString();
		DLOG(INFO)<<"RayTracing : "<<tag<<"<"<<v["name"]<<">"<<std::endl;
		if (tag == "sphere") {
			objects.push_back(new Sphere());
			objects.back()->accept(v);
		}else if (tag == "plane") {
			objects.push_back(new Plane());
			objects.back()->accept(v);
		}else if(tag[0] == '#') {
		}else {
			DLOG(ERROR)<<"Strange Object <"<<tag<<std::endl;
		}
	}

	DLOG(INFO)<<"RayTracing : Init lights"<<std::endl;
	for (int i=0;i<val["lights"].size();i++) {
		Json::Value v = val["lights"][i];
		std::string tag = v["type"].asString();

		if (tag == "area_light") {
			lights.push_back(new AreaLight());
			lights.back()->accept(v);
		}else if (tag[0] == '#') {

		}else {
			DLOG(ERROR)<<"Strange Object <"<<tag<<">"<<std::endl;
		}
	}
	bg_color.accept(val["bg_color"]);
	max_depth = val["max_depth"].asInt();
	shade_quality = val["shade_quality"].asInt();
	spec_power = val["spec_power"].asInt();
	DLOG(INFO)<<"RayTracing : Data accepted"<<std::endl;
	board = new Color[camera->getRx()*camera->getRy()];
	for (int i=0;i<camera->getRx()*camera->getRy();i++)
		board[i] = Color(1,1,1);
}


Color RayTracing::calcReflection(const Object& obj, const Collision& obj_coll, int depth, unsigned&hash) {
	DLOG(INFO)<<"calcReflection... <d="<<depth<<",h="<<hash<<">"<<std::endl;
	DLOG(INFO)<<"REFLECTION POSITION : "<<obj_coll.description()<<std::endl;
	if (depth > max_depth) {
		hash = hash * 17 + 233;
		return bg_color;
	}
	Color result = (rayTrace(obj_coll.C,obj_coll.D,depth,hash) * obj.getMaterial().refl).adjust();
	hash = hash * 17 + obj.getHash();
	return result;
}

Color RayTracing::calcDiffusion(const Object& obj, const Collision& obj_coll) {
	DLOG(INFO)<<"calcDiffusion..."<<std::endl;
	Color color = obj.getColor(obj_coll.C);
	DLOG(INFO)<<"DIFFUSION POSITION : "<<obj_coll.description()<<std::endl;
	Color ret = color * bg_color;
	for (auto &lgt : lights) {
		double shade = lgt->getShade(obj_coll.C,objects,shade_quality);
		ret += color * lgt->getColor(lgt->getCenter()) * shade * obj.getMaterial().diff;
		double spec_ratio = (lgt->getCenter() - obj_coll.C).unit() ^ obj_coll.N.unit();
		if (spec_ratio > 0)
			ret += color * lgt->getColor(lgt->getCenter()) * shade * pow(spec_ratio,spec_power) * obj.getMaterial().spec;
	}
	return ret.adjust();
}

const Object* RayTracing::findCollidedObject(const Vector& _rayO,const Vector& _rayD) {
	Vector rayO = _rayO;
	Vector rayD = _rayD;
	Object* ret = NULL;
	double cdist = inf;
	for (auto obj : objects) {
		if (obj->collideWith(rayO,rayD)) {
			if (obj->getCollision().dist < cdist) {
				cdist = obj->getCollision().dist;
				ret = obj;
			}
		}
	}
	return ret;
}

const Light* RayTracing::findCollidedLight(const Vector& _rayO, const Vector& _rayD) {
	Vector rayO = _rayO;
	Vector rayD = _rayD;
	Light* ret = NULL;
	double cdist = inf;
	for (auto &lgt : lights) {
		if (lgt->collideWith(rayO,rayD)) {
			if (lgt->getCollision().dist < cdist) {
				cdist = lgt->getCollision().dist;
				ret = lgt;
			}
		}
	}
	return ret;
}

Color RayTracing::rayTrace(const Vector& rayO, const Vector& rayD, int depth, unsigned& hash) {
	DLOG(INFO)<<"Ray Trace... <d="<<depth<<",h="<<hash<<">"<<std::endl;
	const Object* obj = findCollidedObject(rayO,rayD);
	const Light* lgt = findCollidedLight(rayO,rayD);
	if (!obj && !lgt) {
		hash = hash * 17 + 235;
		return bg_color;
	}else if (!obj || (obj && lgt && obj->getCollision().dist > lgt->getCollision().dist)) {
		hash = hash * 17 + lgt->getHash();
		return lgt->getColor(lgt->getCenter());
	}else {
		Collision obj_coll = obj->getCollision();
		Color ret;
		if (obj->getMaterial().refl>feps)
			ret += calcReflection(*obj,obj_coll,depth+1,hash);
		if (obj->getMaterial().diff>feps || obj->getMaterial().spec>feps)
			ret += calcDiffusion(*obj,obj_coll);
		return ret.adjust();
	}
}

void RayTracing::run() {
	LOG(INFO)<<"Sampling..."<<std::endl;
	hash_table = new unsigned*[camera->getRx()];
	for (int i=0;i<camera->getRx();i++)
		hash_table[i] = new unsigned[camera->getRy()];

	for (int i=0;i<camera->getRx();i++){
		printf("Render row #%d\n",i);
		for (int j=0;j<camera->getRy();j++) {
			LOG(INFO)<<"RENDER POSITION <"<<i<<","<<j<<">"<<std::endl;
			Vector rayO,rayD;
			camera->getRay(i,j,rayO,rayD);
			DLOG(INFO)<<"RayO = "<<rayO.description()<<std::endl;
			DLOG(INFO)<<"RayD = "<<rayD.description()<<std::endl;
			Color cc = rayTrace(rayO,rayD,0,hash_table[i][j]);
			board[i*camera->getRy()+j] = cc;
			LOG(INFO)<<"Color = "<<cc.description()<<std::endl;
		}
	}
	LOG(INFO)<<"Resampling..."<<std::endl;
	for (int i=0;i<camera->getRx();i++) {
		for (int j=0;j<camera->getRy();j++) {
			bool flag = false;
			if (i!=0 && hash_table[i][j] != hash_table[i-1][j])
				flag = true;
			if (i!=camera->getRx()-1 && hash_table[i][j] != hash_table[i+1][j])
				flag = true;
			if (j!=0 && hash_table[i][j] != hash_table[i][j-1])
				flag = true;
			if (j!=camera->getRy()-1 && hash_table[i][j] != hash_table[i][j+1])
				flag = true;
			if (flag) {
				Color res;
				for (int k1=-1; k1<=1; k1++) {
					for (int k2=-1; k2<=1; k2++) {
						unsigned hash = 0;
						Vector rayO,rayD;
						camera->getRay(i+k1/3.0,j+k2/3.0,rayO,rayD);
						res += rayTrace(rayO,rayD,0,hash);
					}
				}
				res = res*(1.0/9);
				board[i*camera->getRy() + j] = res;
			}
		}
	}
	for (int i=0;i<camera->getRx();i++) {
		delete[] hash_table[i];
	}
	delete[] hash_table;
}

void RayTracing::update() {
	this->paint_board->update();
}

void RayTracing::registerPaintBoard(PaintBoard* pb) {
	assert(camera);
	paint_board = pb;
	paint_board->init(camera->getRx(),camera->getRy(),&board);
}
