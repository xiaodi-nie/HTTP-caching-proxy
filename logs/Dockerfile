FROM ubuntu:18.04
RUN mkdir /var/log/erss
RUN apt-get update && apt-get -y install g++ make
ADD . /var/log/erss
WORKDIR /var/log/erss
RUN make clean
RUN make
CMD ["./proxy"]
