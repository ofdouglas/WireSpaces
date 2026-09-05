# Reference deployment configurations

amr.yaml and excavator.yaml are durable topology-only reference examples inspired
by the AMR and excavator designs. They use current HostId terminology,
groups and inferred trees; endpoint and authorization policies are not encoded.

branched.yaml selects an exact tree across a host branch and a shared-bus branch,
excluding a redundant physical Link and an extra bus listener. The compiled
network test generates every host from this file and exercises real runtime routing.

Application and hardware project deployments remain beside their projects; these
reference examples do not depend on the lifetime of the sketches directories.

packaging_line.yaml is a 40-host, four-cell line with 19 inferred Wires. It remains
within the current target limits and is exercised through generated C++ forwarding
classes and the real Router/Dispatcher in queued host-side network tests.

studies/multicore_gateway.yaml preserves the seven relationships from the
multicore sketch. Its topology resolves, but firmware generation intentionally
fails because Control requires seven local memberships. The read-only viewer
still shows the complete topology and the target diagnostic. This is a study,
not a production-ready deployment or an implemented observation/security policy.
See ../deployment_studies.md for the findings and assumptions.

From the repository root, after the setup in codegen/README.md:

    codegen/.venv/bin/python codegen/wiring_codegen.py codegen/examples/amr.yaml --local-host Motion --explain
    codegen/.venv/bin/python codegen/wiring_codegen.py codegen/examples/excavator.yaml --local-host Vcu --explain
    codegen/.venv/bin/python codegen/wiring_codegen.py codegen/examples/branched.yaml --local-host Gateway --explain
    make -C codegen viewer INPUT=examples/packaging_line.yaml
