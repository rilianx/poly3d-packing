#!/bin/bash
# Downloads the Chess and Engine instances of Lamas-Fernandez et al. (ESICUP repository) into benchs/.
# The meshes have their own licenses (Chess: GNU GPL; Engine: CC BY-NC); they are not included in this repository.
# Then: python tools/decompose_mesh.py benchs "Lamas-Fernandez, C. et al. 2022/Chess/<piece>.stl" ...
set -e
cd "$(dirname "$0")/../benchs"
tmp=$(mktemp -d)
git clone --depth 1 --filter=blob:none --sparse https://github.com/ESICUP/datasets.git "$tmp/datasets"
git -C "$tmp/datasets" sparse-checkout set "3d_irregular/Lamas-Fernandez, C. et al. 2022"
cp -r "$tmp/datasets/3d_irregular/Lamas-Fernandez, C. et al. 2022" .
rm -rf "$tmp"
