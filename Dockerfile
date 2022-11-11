# Build DTNME
FROM ubuntu:20.04 AS build
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin
RUN apt-get clean
RUN apt-get update && apt-get install -y automake g++-9 gcc-9 \
    checkinstall unzip tcl-dev libdb-dev libexpat1-dev git libssl-dev
COPY ./ /build
WORKDIR /build
# This is a super gross hack to get oasys to compile. Oasys only supports gcc<= 8, so we alter the relevant line(s) in the aclocal config
# to add gcc9. After we do that, we build dtnme
RUN sed -i 's/3.*|4.*|5.*|6.*|7.*|8|8.*)/3.*|4.*|5.*|6.*|7.*|8|8.*|9|9.*)/' oasys_source/aclocal/*.ac \ 
  && ./init_dtnme.sh 8 \
  && checkinstall -D --install=no --default --pkgname dtnme --pkgversion=0.0.1 --nodoc -y \
  && cp dtnme*.deb /dtnme.deb

FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin
RUN apt-get update && apt-get install -y tcl libssl-dev

COPY --from=build /*.deb /
ENV USER=root
RUN dpkg -i /*.deb \
    && rm /*.deb

#Install configuration and script
COPY launcher.sh /usr/bin/
RUN chmod 755 /usr/bin/launcher.sh
EXPOSE 4556
EXPOSE 5050/tcp
EXPOSE 5010/tcp

ENTRYPOINT ["/usr/bin/launcher.sh"]
