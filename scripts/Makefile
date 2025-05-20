.PHONY: build test

ifndef BUILD_WRAPPER_OUT_DIR
    $(error BUILD_WRAPPER_OUT_DIR is not set. Please set it in your environment)
endif

build:
	mkdir -p build
	conan profile detect --force
	conan install ./conan/ --output-folder build --settings=build_type=Debug --build=missing
	cmake -S . -B build  -DCMAKE_BUILD_TYPE=Debug -DWITH_TEST=ON -DWITH_COVERAGE=ON -G "Unix Makefiles" \
		-DCMAKE_TOOLCHAIN_FILE=./conan_toolchain.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	build-wrapper-linux-x86-64 --out-dir $(BUILD_WRAPPER_OUT_DIR) cmake --build ./build/ --config Debug --parallel

test:
	cd build && make test ARGS="-E FSPlatformTest.* --repeat until-pass:2 --output-on-failure" && make coverage
