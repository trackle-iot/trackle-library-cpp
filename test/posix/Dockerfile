FROM ghcr.io/catthehacker/ubuntu:act-22.04

# Install Python and pip
RUN apt-get update && \
    apt-get install -y python3 python3-pip && \
    apt-get clean

# set working dir
WORKDIR /workspace

# default command
CMD ["bash"]