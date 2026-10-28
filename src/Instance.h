/*
 * Instance.h
 *
 * Instances of the polyhedra loading problem (3D irregular strip packing):
 * a container with a fixed W x D base and open height, and piece types with their demand.
 * ESICUP JSON format (https://github.com/ESICUP/datasets/tree/main/3d_irregular).
 */

#ifndef PLP_INSTANCE_H_
#define PLP_INSTANCE_H_

#include <string>
#include <vector>
#include "ConvexPolytope.h"

namespace plp {

/**
 * Piece as a union of convex polytopes.
 * Coordinates are translated so that the bounding box starts at the origin
 * ("AlignToMinimum" convention of ESICUP solutions).
 */
class Piece {
public:

	/**
	 * Reads an OBJ or STL mesh. The parts are the OBJ groups ('o'/'g') or, if there
	 * are no groups, the consecutive closed shells of the mesh.
	 * If parts_file is not empty, the parts are read from that file (convex decomposition).
	 */
	static Piece load(const std::string& file, const std::string& parts_file="");

	std::string name;

	//convex decomposition file (empty if the parts come from the mesh)
	std::string parts_file;

	//convex parts (if convex_parts==false, the convex hulls of the parts)
	std::vector<ConvexPolytope> parts;

	//all parts are convex: the piece is represented exactly
	bool convex_parts;

	//the whole piece is convex
	bool convex;

	double volume;

	Box3 bbox;

	//minimum corner of the bounding box (of the parts) in file coordinates
	Vec3 offset;

	//minimum corner of the original mesh (the ESICUP position, itemOrigin = AlignToMinimum, refers to it); with
	//a conservative decomposition (the parts contain the mesh) it is >= offset
	Vec3 mesh_min;

	std::vector<std::string> warnings;
};

struct ItemType {
	std::string path;
	int piece;
	int demand;
};

class Instance {
public:

	/**
	 * Reads the instance. Piece paths are relative to the root folder of the
	 * dataset; if root is empty, it is searched upwards from the folder of the file.
	 * If root/decompositions/<piece path with .obj extension> exists, the convex
	 * parts of the piece are read from that file.
	 */
	/** dec: folder (under root) with the convex decompositions of the pieces */
	static Instance load(const std::string& file, const std::string& root="", const std::string& dec="decompositions");

	int n_items() const;

	double total_volume() const;

	/** max(total volume / (W*D), height of the tallest piece) */
	double height_lower_bound() const;

	std::string name;
	std::string file;
	std::string root;

	double W, D;

	std::vector<Piece> pieces;
	std::vector<ItemType> types;
};

} /* namespace plp */

#endif /* PLP_INSTANCE_H_ */
