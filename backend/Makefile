-include .env
export

DB_PORT ?= 15432
API_PORT ?= 8000
API_HOST ?= 127.0.0.1

COMPOSE_DEV = docker compose -f docker-compose.yml -f docker-compose.dev.yml

.PHONY: test up down logs dev dev-db dev-down

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
