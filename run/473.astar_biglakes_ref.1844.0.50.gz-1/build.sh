#!/bin/bash
cd ../../build
make -j$(nproc)
cd ../run/473.astar_biglakes_ref.1844.0.50.gz-1/