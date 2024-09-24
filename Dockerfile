FROM alpine:3.20

WORKDIR /app

RUN apk update && apk add bash nodejs npm mysql mysql-client gdb python3 py3-pip py3-tqdm py3-pefile;

EXPOSE 80
EXPOSE 3306

CMD ["sh", "-c", "while :; do sleep 1000; done"]