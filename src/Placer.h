/*
 * Placer.h
 *
 * Positions of a piece (translations only) and the moves of the method.
 *
 * A convex part a translated by t and a fixed part b translated by tb overlap iff t - tb lies in the interior
 * of the no-fit polytope N_ab = b + (-a). Each facet n.r <= d of N_ab gives the valid linear constraint
 * n.(t - tb) >= d.
 */

#ifndef PLP_PLACER_H_
#define PLP_PLACER_H_

#include <vector>
#include <map>
#include <random>
#include "Instance.h"
#include "SmallLP.h"

namespace plp {

/** placed convex part */
struct PlacedPart {
	int part;   //global index of the part
	Vec3 t;
	Box3 bbox;
};

/** GLS weights of the pairs of pieces (i < j); the weight of a pair is 1 + w_ij */
typedef std::map< std::pair<int,int>, double > Weights;

class Placer {
public:

	/**
	 * @param grid starting points per axis (x, y) of the greedy construction
	 * @param slide weight sigma of x and y (towards the nearest walls) in the objective of Descend; z has weight 1
	 * @param eps tolerance on the penetration depth
	 */
	Placer(const Instance& inst, int grid=4, double slide=0.065, double eps=1e-6);

	/**
	 * With seed > 0 the starting points are perturbed slightly (1e-7 of the scale). The method is very sensitive
	 * to small perturbations, so different seeds give independent runs.
	 */
	void set_seed(unsigned seed) { this->seed = seed; rng.seed(seed); }

	/**
	 * Descend: from the feasible position t, minimizes c.t without overlapping the placed parts, with a sequence
	 * of LPs (one facet of each nearby NFP, in a trust region around t) until the position does not improve.
	 */
	Vec3 descend(int p, Vec3 t, const Vec3& c, const std::vector<PlacedPart>& placed);

	/**
	 * Feasible positions of piece p: Descend from each point of a grid x grid on the plane z = top
	 * (top: current height of the load). Repeated positions are discarded.
	 */
	void candidates(int p, const std::vector<PlacedPart>& placed, double top, std::vector<Vec3>& out);

	/** true if piece p at t does not overlap the placed parts (penetration <= eps) */
	bool feasible(int p, const Vec3& t, const std::vector<PlacedPart>& placed) const;

	/**
	 * Improve: moves one piece at a time, the others fixed, in increasing order of z:
	 * - the piece descends again from its position;
	 * - a piece that defines the height moves to its lowest candidate position, if lower.
	 * Repeats while the height decreases (at most max_passes). Returns the new height.
	 */
	double improve(const std::vector<int>& pieces, std::vector<Vec3>& t, int max_passes=50);

	/**
	 * Exact axis move (Proposition 1): moves piece i along the axis (0, 1, 2) to the position, inside the container
	 * of height H, that minimizes sum_j (1 + w_ij) delta_ij, where delta_ij is the sum over the pairs of parts of
	 * the penetration depth max(0, min_f (d_f - n_f.(t_i - t_j))) in their NFP. Along the line each term is zero
	 * outside an interval and concave inside it, so the minimum is at a breakpoint. Returns the decrease (>= 0).
	 */
	double axis_move(const std::vector<int>& pieces, std::vector<Vec3>& t, int i, int axis, double H, const Weights& w);

	/**
	 * MinimizeOverlap: at fixed height H, moves the overlapping pieces along the three axes (axis_move) and, at
	 * local minima, updates the GLS weights of the pairs that still overlap. The weights persist in weights.
	 * mult: multiplicative weights (Gardeyn et al. 2025): 1 + w_ij of an overlapping pair is multiplied by
	 * 1.2 + 0.8 delta_ij / max delta and the others decay (x0.95, at least 1); otherwise +1 for the overlapping pairs.
	 * Returns true if the overlap is removed (checked with overlap()).
	 */
	bool minimize_overlap(const std::vector<int>& pieces, std::vector<Vec3>& t, double H, int max_passes,
			Weights& weights, std::mt19937& rng, bool mult=true);

	/** penetration between pieces i and j (sum over their pairs of parts, with the NFPs) */
	double pair_penetration(const std::vector<int>& pieces, const std::vector<Vec3>& t, int i, int j);

	/** sum, over the pairs of pieces, of the largest penetration depth between their parts */
	double overlap(const std::vector<int>& pieces, const std::vector<Vec3>& t) const;

	/** placed parts of the pieces, except piece skip */
	void place_all(const std::vector<int>& pieces, const std::vector<Vec3>& t, int skip, std::vector<PlacedPart>& out) const;

	/** parts (global indices) of piece p */
	const std::vector<int>& get_parts(int p) const { return piece_parts[p]; }

	const ConvexPolytope& get_part(int k) const { return *parts[k]; }

	const Instance& inst;

private:
	/** NFP of part a (moving) with respect to part b (fixed), both untranslated */
	const ConvexPolytope& nfp(int a, int b);

	int grid;
	double slide;
	double eps;
	double tol;

	std::vector<const ConvexPolytope*> parts;
	std::vector< std::vector<int> > piece_parts;

	std::map< std::pair<int,int>, ConvexPolytope > nfps;
	//direct access to the computed NFPs: nfp_idx[a][b] (rows created on use)
	std::vector< std::vector<const ConvexPolytope*> > nfp_idx;

	SmallLP lp;

	unsigned seed;
	std::mt19937 rng;
};

} /* namespace plp */

#endif /* PLP_PLACER_H_ */
