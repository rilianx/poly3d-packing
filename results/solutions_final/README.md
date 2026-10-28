# Best solutions of the final runs

Best solution out of 10 seeds (120 s each) for every instance of the main table of the paper, found by the final
method on 2× Xeon E5-2620 v4 with 14 parallel runs. The file name gives the instance and the height.

All of them are feasible according to `poly3d_check` (separating-axis test between every pair of convex parts) with
the default tolerance 1e-6. Four of them (Merged3, Merged4, Stoyan 2004 Ex1 and Ex4) have penetrations of up to
9e-7 and do not pass with `--eps 1e-9`.

```bash
cd benchs
../build/poly3d_check "Egeblad et al. 2009/Merged1.json" -s ../results/solutions_final/Egeblad2009_Merged1_16.56.json
```
