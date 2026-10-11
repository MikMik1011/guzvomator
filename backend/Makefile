-include .env
export

DB_PORT ?= 15432
API_PORT ?= 8000
API_HOST ?= 127.0.0.1

COMPOSE_DEV = docker compose -f docker-compose.yml -f docker-compose.dev.yml

.PHONY: test up down logs dev dev-db dev-down azure-package azure-deploy

test:
	uv run pytest -q -p no:cacheprovider

up:
	docker compose up --build -d

down:
	docker compose down

logs:
	docker compose logs -f api

dev-db:
	@test -n "$(POSTGRES_PASSWORD)" -a -n "$(API_KEY)" || \
		(echo "copy .env.example to .env and set both values" && exit 1)
	$(COMPOSE_DEV) up -d --wait db

dev: dev-db
	DATABASE_URL=postgresql://guzvo:$(POSTGRES_PASSWORD)@localhost:$(DB_PORT)/guzvomator \
		uv run uvicorn app.api:app --reload --host $(API_HOST) --port $(API_PORT)

dev-down:
	$(COMPOSE_DEV) down

AZURE_GROUP ?= guzvomator-rg

# Azure Functions wants a requirements.txt next to function_app.py; it is built
# from the lock file, so it never drifts from the Docker image.
requirements.txt: pyproject.toml uv.lock
	uv export --no-dev --group azure --no-emit-project --no-hashes -o $@

azure-package: requirements.txt
	rm -f azure-package.zip
	zip -qr azure-package.zip function_app.py host.json requirements.txt app \
		-x "app/__pycache__/*" "app/mqtt_ingest.py"

# Python apps on Flex Consumption are zipped and built remotely, so the
# dependencies match the Linux host
azure-deploy: azure-package
	@test -n "$(AZURE_APP)" || (echo "set AZURE_APP to the function app name" && exit 1)
	az functionapp deployment source config-zip --resource-group $(AZURE_GROUP) \
		--name $(AZURE_APP) --src azure-package.zip --build-remote true
