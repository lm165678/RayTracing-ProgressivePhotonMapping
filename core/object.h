#ifndef OBJECT_H
#define OBJECT_H

#include "json/json.h"
#include "color.h"
#include "vector.h"
#include "texture.h"

struct Collision {
	Vector C;//The Center of Collision
	Vector N;//The normal of the plane
	Vector D;//The direction of reflaction
	double dist;
	bool face;
	std::string description()const;
};

struct Material {
	double refl;//reflection ratio
	double diff;//diffusion ratio
	double spec;
	void accept(const Json::Value& val);
};

class Object {
	protected:
		unsigned hash;
		Collision collision;
		Material material;
		std::string name;
		Texture* texture;
	public:
		Object();
		virtual ~Object();
		virtual void accept(const Json::Value& val);
		unsigned getHash()const {return hash;}

		const Collision& getCollision()const {return collision;}
		const Material& getMaterial()const {return material;}

		virtual bool collideWith(const Vector& rayO, const Vector& rayD) = 0;
		virtual Color getColor(const Vector &pos)const = 0;
		std::string getName()const{return name;}
};


#endif