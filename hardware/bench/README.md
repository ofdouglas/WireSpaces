# Bench topology

topology.yaml describes the planned lab-bench PCB's physical Links and test Wires.
It belongs here with the hardware project, rather than in the code generator.
The deliberate LongBenchPath selects only its named interface attachments.

From the repository root:

    codegen/.venv/bin/python codegen/wiring_codegen.py hardware/bench/topology.yaml --local-host Gateway --explain

See ../../codegen/README.md for dependency setup and schema documentation.
These are static local-origin projections; ingress forwarding and hardware
multi-hop verification are not implemented by this configuration.
