/*
 * Solution.h
 *
 * Solutions (placement of each piece) in the ESICUP JSON format, and their verification
 */

#ifndef PLP_SOLUTION_H_
#define PLP_SOLUTION_H_

#include <string>
#include <vector>
#include "Instance.h"

namespace plp {

/**
 * Placed piece: v' = R v + t, with v in piece coordinates (bounding box at the origin)
 */
struct Placement {
	int type;
	Mat3 R;
	Vec3 t;
};

struct Report {
	bool feasible;
	double height;
	double density;      //volume of the pieces / (W*D*height)
	double max_overlap;  //largest penetration depth between two pieces
	double max_outside;  //largest distance of a vertex outside the container
	int n_placed;
	std::vector<std::string> errors;
};

class Solution {
public:

	Solution(const Instance& inst) : inst(inst) { }

	/** reads an ESICUP solution (pieces are identified by file name) */
	static Solution load(const std::string& file, const Instance& inst);

	void save(const std::string& file) const;

	/** translation of the original mesh (ESICUP position, itemOrigin = AlignToMinimum) of item p */
	Vec3 mesh_translation(const Placement& p) const;

	/**
	 * Checks that each piece is inside the container (z >= 0), that the pieces
	 * do not overlap (penetration depth greater than eps) and that all pieces
	 * of the instance are placed (unless partial=true).
	 */
	Report verify(double eps=1e-6, bool partial=false) const;

	double height() const;

	const Instance& inst;
	std::vector<Placement> items;
};

std::ostream& operator<<(std::ostream& os, const Report& r);

} /* namespace plp */

#endif /* PLP_SOLUTION_H_ */
