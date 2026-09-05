PYTHON ?= $(if $(wildcard .venv/bin/python),.venv/bin/python,python3)

.PHONY: test schema
test:
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py' -v

schema:
	$(PYTHON) wiring_structure.py --output wiring.schema.json
