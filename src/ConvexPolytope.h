/*
 * ConvexPolytope.h
 *
 * Convex polytope in V (vertices) and H (facets) representation,
 * and intersection test by separating axes (SAT)
 */

#ifndef PLP_CONVEXPOLYTOPE_H_
#define PLP_CONVEXPOLYTOPE_H_

#include <vector>
#include <stdexcept>
#include "Geometry.h"

namespace plp {

/**
 * Halfspace n.x <= d (n is a unit outward normal)
 */
struct Plane {
	Vec3 n;
	double d;
	Plane(const Vec3& n, double d) : n(n), d(d) { }
};

class ConvexPolytope {
public:

	/**
	 * Convex hull of the points (incremental algorithm).
	 * Points closer than eps*diameter are considered coplanar.
	 * Throws std::runtime_error if the points do not span a solid.
	 */
	/** edges: compute the edge directions (for penetration_depth; not needed for NFPs) */
	static ConvexPolytope hull(const std::vector<Vec3>& points, double eps=1e-9, bool edges=true);

	/** v' = R v + t (R is a rotation) */
	ConvexPolytope transformed(const Mat3& R, const Vec3& t) const;

	ConvexPolytope translated(const Vec3& t) const;

	/** true if n.p <= d + eps for all facets */
	bool contains(const Vec3& p, double eps=0.0) const;

	const std::vector<Vec3>& get_vertices() const { return vertices; }
	const std::vector<Plane>& get_facets() const { return facets; }
	const std::vector<Vec3>& get_edge_dirs() const { return edge_dirs; }
	const Box3& get_bbox() const { return bbox; }
	double get_volume() const { return volume; }

private:

	ConvexPolytope() : volume(0) { }

	void update_bbox();

	std::vector<Vec3> vertices;

	//facets (coplanar triangles are merged)
	std::vector<Plane> facets;

	//unit edge directions (no duplicates, up to sign)
	std::vector<Vec3> edge_dirs;

	Box3 bbox;

	double volume;
};

/**
 * Penetration depth of A and B: the smallest overlap of their projections
 * onto the candidate axes (facet normals and cross products of edges).
 * For convex polytopes it is the smallest translation that separates them.
 * If it is <= 0 the polytopes do not intersect (or only touch).
 * Stops as soon as it finds an axis with overlap <= stop.
 */
double penetration_depth(const ConvexPolytope& A, const ConvexPolytope& B, double stop=0.0, Vec3* axis=NULL);

} /* namespace plp */

#endif /* PLP_CONVEXPOLYTOPE_H_ */
