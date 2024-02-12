# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:latest

# Upgrade any ubuntu packages
RUN apt-get update && apt-get upgrade -y

# Install the conda and mynewt public GPG keys
RUN apt-get install -y curl gpg && \
    curl https://repo.anaconda.com/pkgs/misc/gpgkeys/anaconda.asc | apt-key add - && \
    curl https://raw.githubusercontent.com/JuulLabs-OSS/debian-mynewt/master/mynewt.gpg.key | apt-key add - && \
    echo "deb https://repo.anaconda.com/pkgs/misc/debrepo/conda stable main" > /etc/apt/sources.list.d/conda.list && \
    echo "deb https://raw.githubusercontent.com/JuulLabs-OSS/debian-mynewt/master latest main" > /etc/apt/sources.list.d/mynewt.list

# Install the native packages we need
RUN apt-get update && apt-get install -y \
    sudo \
    conda \
    newt=1.7.0-1 \
    build-essential

# Add a user
ARG USER=gg
ARG UID=1000
ARG GID=1000
RUN useradd -m ${USER} --uid=${UID}

# Run as the user we have setup
USER ${UID}:${GID}
WORKDIR /home/${USER}

# Make RUN commands use `bash --login`:
SHELL ["/bin/bash", "--login", "-c"]

# Setup conda and create the conda environment
COPY environment.yml /tmp/gg-env.yml
COPY docs/requirements.txt /tmp/docs/requirements.txt
ENV PATH /opt/conda/condabin:$PATH
RUN conda init bash
RUN conda env create -n gg -f /tmp/gg-env.yml
RUN echo "conda activate gg" >> /home/${USER}/.bashrc
