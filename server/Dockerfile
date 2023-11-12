FROM xianpengshen/clang-tools:17 AS build-stage

RUN apt-get update
RUN apt-get -y --no-install-recommends install cmake make gcc xz-utils gcc-multilib

ADD https://releases.linaro.org/components/toolchain/binaries/latest-7/arm-linux-gnueabihf/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf.tar.xz .
RUN tar -xf gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf.tar.xz
ENV PATH="${PATH}:/src/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin"

COPY src/*.c src/*.h src/CMakeLists.txt src/
COPY .clang-format .clang-tidy CMakeLists.txt .

RUN cmake .
RUN make tidy
RUN make format-check
RUN make

FROM scratch AS export-stage
COPY --from=build-stage /src/mister-status /
