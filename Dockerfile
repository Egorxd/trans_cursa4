FROM python:3.12-slim-bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends gcc make bison flex libc6-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /project
COPY . .
RUN make grammar && make
RUN python -m pip install --no-cache-dir -r requirements.txt
RUN python tests/run_tests.py --translator bin/translator --report /tmp/test-report.json

FROM python:3.12-slim-bookworm
RUN apt-get update && apt-get install -y --no-install-recommends libgomp1 && rm -rf /var/lib/apt/lists/*
COPY requirements.txt /tmp/requirements.txt
RUN python -m pip install --no-cache-dir -r /tmp/requirements.txt
COPY --from=build /project/bin/translator /usr/local/bin/translator
COPY --from=build /tmp/test-report.json /opt/translator/test-report.json
WORKDIR /work
ENTRYPOINT ["translator"]
CMD ["--help"]
