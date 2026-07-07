# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel cmake
RUN pacman -Syu --noconfirm sdl3 sdl3_image
RUN pacman -Syu --noconfirm nodejs emscripten
ENV PATH=/usr/lib/emscripten:$PATH
RUN emcc -v
RUN emcc -sUSE_SDL=3 /dev/null || true

# Add the library src to our build env
FROM env AS devel
WORKDIR /home/project
COPY . .

ARG CMAKE_BUILD_PARALLEL_LEVEL
ENV CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-4}

FROM devel AS build
RUN cmake --version
RUN emcmake cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --target all -v
#RUN cmake --build build --target install -v

FROM build AS test
#RUN CTEST_OUTPUT_ON_FAILURE=1 cmake --build build --target test
RUN cd build && CTEST_OUTPUT_ON_FAILURE=1 ctest -V

FROM scratch AS export
COPY --from=build /home/project/build/bin/index.js /
COPY --from=build /home/project/build/bin/index.wasm /
COPY --from=build /home/project/build/bin/index.html /

