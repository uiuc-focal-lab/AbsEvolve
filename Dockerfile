FROM ubuntu:24.04

WORKDIR /home/absevolve

# Install system dependencies
RUN apt-get update && apt-get install -y \
    vim htop tree less wget unzip cmake software-properties-common \
    python3 python3-pip python3-venv \
    git make build-essential ninja-build \
    graphviz \
    libgmp-dev libmpfr-dev \
    clang-14 llvm-14 llvm-14-dev libpolly-14-dev \
    && rm -rf /var/lib/apt/lists/*

# Better terminal prompt and aliases
RUN echo 'export TERM=xterm-256color' >> /root/.bashrc && \
    echo "alias ll='ls -alF --color=auto'" >> /root/.bashrc && \
    echo "alias la='ls -A'" >> /root/.bashrc && \
    echo "alias l='ls -CF'" >> /root/.bashrc && \
    echo 'export PS1="\[\e[38;5;39m\]\u@\h \[\e[38;5;245m\]\w \[\e[38;5;82m\] ❯\[\e[0m\] "' >> /root/.bashrc

    # Set clang-14 as the default clang version
RUN ln -s /usr/bin/clang-14 /usr/bin/clang && \
    ln -s /usr/bin/clang++-14 /usr/bin/clang++

# Copy project files
ADD . /home/absevolve/

# Create python environment and install python dependencies
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
RUN pip3 install --no-cache-dir -r /home/absevolve/requirements.txt

# Build the dependencies
RUN bash /home/absevolve/scripts/install_deps.sh

# Copy the .env file
RUN cp /home/absevolve/.env.example /home/absevolve/.env

# Build the project
RUN bash /home/absevolve/scripts/build.sh scratch

# Set the gurobi license file
ENV GRB_LICENSE_FILE=/home/absevolve/experiments/licenses/gurobi.lic

# Set Symba binary path
ENV SYMBA_BINARY=/home/absevolve/src/binaries/symba

# Set runtime library path
ENV LD_LIBRARY_PATH=/home/absevolve/src/clam/build/install/lib:/home/absevolve/deps/install/boost_1_80_0/lib:$LD_LIBRARY_PATH