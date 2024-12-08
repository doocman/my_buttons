
cmake -S . -B build/rp2040 -DPICO_PLATFORM=rp2040 -DCMAKE_C_COMPILER=/bin/arm-none-eabi-gcc -DCMAKE_CXX_COPILER=/bin/arm-none-eabi-g++ -DCMAKE_BUILD_TYPE=Release
CXX=g++-14 CC=gcc-14 cmake --build build/rp2040

