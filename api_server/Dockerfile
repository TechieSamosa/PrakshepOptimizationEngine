# Start from Python 3.11 slim
FROM python:3.11-slim

# Install CMake and build tools for pybind11
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy physics engine source and build it
# We do this before installing Python dependencies so that we don't rebuild if only Python code changes
COPY cpp_engine /app/cpp_engine
WORKDIR /app/cpp_engine/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --config Release

# Copy api_server and ml_environment requirements
WORKDIR /app
COPY api_server/requirements.txt ./api_server/
COPY ml_environment/requirements.txt ./ml_environment/

# Install dependencies
RUN pip install --no-cache-dir -r api_server/requirements.txt -r ml_environment/requirements.txt

# Copy the rest of the application
COPY api_server /app/api_server
COPY ml_environment /app/ml_environment

# Create data directory for parquet files and logs
RUN mkdir -p /app/data

# Ensure Python can find the built pybind11 module
# Since Docker runs on Linux, it compiles to a .so file inside /app/cpp_engine/build
ENV PYTHONPATH="${PYTHONPATH}:/app/cpp_engine/build:/app"

# Expose port
EXPOSE 8000

# Run uvicorn server
WORKDIR /app/api_server
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
