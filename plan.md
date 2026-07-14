1. The CI is failing the "C++ Format" check:
   ```
   src/apimanager.cpp:145:15: error: code should be clang-formatted [-Wclang-format-violations]
   ```
2. I will run `clang-format -i src/apimanager.cpp` to format the file and commit the fix.
