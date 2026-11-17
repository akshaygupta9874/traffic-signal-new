# Relay + the two web apps (Node). Talks to the MASM program over TCP (DOSBox COM1).
FROM node:20-alpine
WORKDIR /app
COPY relay/package.json relay/package-lock.json ./relay/
RUN cd relay && npm ci --omit=dev
COPY relay/server.js ./relay/server.js
COPY apps ./apps
ENV PORT=8080 \
    DOS_ADDR=dos:5000 \
    CONTROL_PIN=1234 \
    APPS_DIR=/app/apps
EXPOSE 8080
USER node
CMD ["node", "relay/server.js"]
