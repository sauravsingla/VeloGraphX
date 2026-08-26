# Python bindings

If pybind11 is installed, configure with `-DVELOgraphx_BUILD_PYTHON=ON` and CMake will build the `velographx` extension. The native C++ engine remains the implementation of all hot paths. NumPy/SciPy/Arrow zero-copy adapters are tracked as a follow-on optimization because their exact ownership and dtype contracts require careful benchmarking and correctness tests.
