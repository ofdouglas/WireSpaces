PYTHON ?= $(if $(wildcard .venv/bin/python),.venv/bin/python,python3)
INPUT ?= examples/packaging_line.yaml
OUTPUT ?= build/topology.html

.PHONY: test schema
test:
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

schema:
	$(PYTHON) wiring_structure.py --output wiring.schema.json

.PHONY: viewer
viewer:
	mkdir -p "$(dir $(OUTPUT))"
	$(PYTHON) wiring_viewer.py "$(INPUT)" --output "$(OUTPUT)"
