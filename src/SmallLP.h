/*
 * SmallLP.h
 *
 * Linear program in 3 variables: min c.x  s.t.  a_i.x <= b_i,
 * solved with an active-set method from a feasible point.
 * All iterates are feasible: if the iteration limit is reached,
 * the result is feasible although not necessarily optimal.
 */

#ifndef PLP_SMALLLP_H_
#define PLP_SMALLLP_H_

#include <vector>
#include "Geometry.h"

namespace plp {

class SmallLP {
public:

	void clear() { A.clear(); B.clear(); }

	void add(const Vec3& a, double b) { A.push_back(a); B.push_back(b); }

	int size() const { return A.size(); }

	/**
	 * Minimizes c.x starting from x0 (feasible). Returns the last iterate.
	 * optimal tells whether the optimality conditions were met.
	 */
	Vec3 minimize(const Vec3& c, const Vec3& x0, bool* optimal=NULL, int maxit=200) const;

private:
	std::vector<Vec3> A;
	std::vector<double> B;
};

} /* namespace plp */

#endif /* PLP_SMALLLP_H_ */
