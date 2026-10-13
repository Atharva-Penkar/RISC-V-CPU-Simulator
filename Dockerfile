# Container for the backend API (backend/) with the C++ simulator, e.g. for Google Cloud Run.
# Build from the repository root: docker build -t riscv-sim-api .

# Stage 1: compile rvsim and rvasm.
FROM debian:bookworm-slim AS simulator
RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ cmake make \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY apps apps
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --target rvsim rvasm --parallel

# Stage 2: the Node.js API with the two executables and the example programs.
FROM node:22-bookworm-slim
ENV NODE_ENV=production \
    RVSIM_PATH=/app/bin/rvsim \
    RVASM_PATH=/app/bin/rvasm \
    EXAMPLES_DIR=/app/examples
WORKDIR /app
COPY backend/package.json backend/package-lock.json ./
RUN npm ci --omit=dev && npm cache clean --force
COPY backend/src src
COPY --from=simulator /src/build/rvsim /src/build/rvasm bin/
COPY tests/programs/*.s examples/
USER node
# Cloud Run sets PORT (8080); the server listens on it.
CMD ["node", "src/server.js"]
