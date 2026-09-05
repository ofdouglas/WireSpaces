# Sketch deployment configurations

amr.yaml and excavator.yaml are the topology-only configurations for the adjacent
07_amr.md and 08_excavator.md sketches. They use current HostId terminology,
groups and inferred trees; endpoint and authorization policies are not encoded.

From the repository root, after the setup in codegen/README.md:

    codegen/.venv/bin/python codegen/wiring_codegen.py sketches_b/topologies/amr.yaml --local-host Motion --explain
    codegen/.venv/bin/python codegen/wiring_codegen.py sketches_b/topologies/excavator.yaml --local-host Vcu --explain
