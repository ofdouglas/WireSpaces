# Reference deployment configurations

amr.yaml and excavator.yaml are durable topology-only reference examples inspired
by the AMR and excavator designs. They use current HostId terminology,
groups and inferred trees; endpoint and authorization policies are not encoded.

branched.yaml selects an exact tree across a host branch and a shared-bus branch,
excluding a redundant physical Link and an extra bus listener. The compiled
network test generates every host from this file and exercises real runtime routing.

Application and hardware project deployments remain beside their projects; these
reference examples do not depend on the lifetime of the sketches directories.

From the repository root, after the setup in codegen/README.md:

    codegen/.venv/bin/python codegen/wiring_codegen.py codegen/examples/amr.yaml --local-host Motion --explain
    codegen/.venv/bin/python codegen/wiring_codegen.py codegen/examples/excavator.yaml --local-host Vcu --explain
    codegen/.venv/bin/python codegen/wiring_codegen.py codegen/examples/branched.yaml --local-host Gateway --explain
