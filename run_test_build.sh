mkdir -p build
cd build || return 1
cmake -G Ninja ..
ninja
