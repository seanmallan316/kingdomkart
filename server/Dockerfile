# The Kingdom Kart online server, as a tiny Linux container (built by Fly.io from fly.toml in the project's top folder).
FROM alpine:3.20 AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY src/net/proto.h src/net/proto.h
COPY server/kkserver.c server/kkserver.c
RUN gcc -std=gnu11 -O2 -Wall -static -Isrc server/kkserver.c -o /kkserver

FROM alpine:3.20
COPY --from=build /kkserver /kkserver
# Fly.io delivers UDP only to programs listening on the address "fly-global-services"
ENV KK_BIND=fly-global-services KK_PORT=7777
CMD ["/kkserver"]
