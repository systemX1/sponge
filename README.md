## Deployment

```shell
# linux
git clone https://github.com/systemX1/sponge
mkdir build && cd build
cmake .. && make -j6 && make check_lab4               # test cases
./appstcp_benchmark                                   # benchmark
gprof ./apps/tcp_benchmark > tcp_benchmark_log.txt    # profiler

# or use docker
# in directory ~/Repo/others/CS-144/sponge
docker build -f DockerFile -t archlinux_cs144:latest .
docker run -it --privileged --cpus=6 archlinux_cs144:latest
```



