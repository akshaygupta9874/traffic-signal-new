# The MASM traffic program (traffic.exe) running in headless DOSBox, with COM1 exposed as TCP port 5000.
FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends dosbox \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY platform/dos/traffic.exe ./dos/traffic.exe
COPY dosbox-traffic.conf ./dosbox-traffic.conf
# No screen in a container: SDL's "dummy" drivers. (The DOS screen is not visible; the apps are the UI.)
ENV SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    HOME=/tmp
EXPOSE 5000
CMD ["dosbox", "-conf", "/app/dosbox-traffic.conf", "-c", "mount c /app/dos", "-c", "c:", "-c", "traffic.exe"]
