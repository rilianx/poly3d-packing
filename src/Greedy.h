/*
 * Greedy.h
 *
 * Greedy construction (Algorithm 2 of the paper): pieces are placed one at a time on top of the load. For every
 * type with pieces left, the piece descends from a grid of starting points; the pair (type, position) with the
 * largest score
 *   alpha*log(V/Vmax) - beta*z/H0 - gamma*max(0, z + h - Hcur)/H0
 * is placed (V: volume of the piece, Vmax: largest volume, z: height of the position, h: height of the piece,
 * Hcur: current height of the load, H0: lower bound of the height).
 */

#ifndef PLP_GREEDY_H_
#define PLP_GREEDY_H_

#include "Placer.h"
#include "Solution.h"

namespace plp {

/** complete solution (throws if a piece cannot be placed) */
Solution greedy(Placer& placer, double alpha=1.0, double beta=5.53, double gamma=0.77);

} /* namespace plp */

#endif /* PLP_GREEDY_H_ */
