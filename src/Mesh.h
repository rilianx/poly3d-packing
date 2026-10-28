/*
 * Mesh.h
 *
 * Polygonal meshes (reading OBJ and STL files)
 */

#ifndef PLP_MESH_H_
#define PLP_MESH_H_

#include <string>
#include <vector>
#include "Geometry.h"

namespace plp {

struct Mesh {
	std::vector<Vec3> vertices;

	//faces (polygons) as vertex indices
	std::vector< std::vector<int> > faces;

	//face groups ('o' or 'g' in OBJ): group k contains faces [groups[k], groups[k+1])
	std::vector<int> groups;

	/** OBJ or STL (ASCII or binary) depending on the extension */
	static Mesh load(const std::string& file);

	static Mesh load_obj(const std::string& file);
	static Mesh load_stl(const std::string& file);

	int n_groups() const { return groups.size(); }

	/**
	 * Splits the faces into consecutive closed shells (each edge used by
	 * exactly two faces). This is how the unions of convex parts of Stoyan et al. (2004) are given.
	 * Returns the range [begin,end) of each shell, or empty if the faces cannot be split this way.
	 */
	std::vector< std::pair<int,int> > closed_shells(int begin, int end) const;

	/** true if each edge of faces [begin,end) is used by exactly two faces */
	bool is_closed(int begin, int end) const;

	/** volume enclosed by faces [begin,end) (closed, oriented shell) */
	double volume(int begin, int end) const;
};

} /* namespace plp */

#endif /* PLP_MESH_H_ */
