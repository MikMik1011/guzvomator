#!/usr/bin/env bash
# Creates what the backend needs on Azure: a PostgreSQL server and a Function
# App on the Flex Consumption plan, then points the app at the database.
# Run `az login` first; run it from backend/. Safe to run again: whatever
# already exists is skipped.
#
#   AZURE_SUFFIX=ab12 POSTGRES_PASSWORD=... API_KEY=... azure/provision.sh
#   make azure-deploy AZURE_APP=guzvomator-ab12
#
# `az group delete --name guzvomator-rg` removes everything again.
set -euo pipefail

: "${AZURE_SUFFIX:?a short unique suffix, lowercase letters and digits (up to 14)}"
: "${POSTGRES_PASSWORD:?a password of letters and digits only, it goes into a URL}"
: "${API_KEY:?the key the node sends in the X-API-Key header}"

# Azure for Students only allows italynorth, polandcentral, belgiumcentral,
# denmarkeast and norwayeast, and Flex Consumption has to be offered there too
LOCATION="${AZURE_LOCATION:-polandcentral}"
GROUP=guzvomator-rg
SERVER="guzvomator-db-${AZURE_SUFFIX}"
STORAGE="guzvomator${AZURE_SUFFIX}"
APP="guzvomator-${AZURE_SUFFIX}"
DATABASE=guzvomator
DB_USER=guzvo

step() { printf '\n[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }

step "1/7 Registering resource providers (a minute or two)"
# (Microsoft.App is only for virtual network integration, which is not used,
# and its registration can take very long)
for provider in Microsoft.Web Microsoft.Storage Microsoft.DBforPostgreSQL \
  Microsoft.Insights Microsoft.OperationalInsights; do
  echo "  $provider"
  az provider register --namespace "$provider" --wait
done

step "2/7 Resource group $GROUP in $LOCATION"
az group create --name "$GROUP" --location "$LOCATION" --output none

step "3/7 PostgreSQL server $SERVER (5 to 10 minutes the first time, only a spinner shows)"
if az postgres flexible-server show --resource-group "$GROUP" --name "$SERVER" \
  --output none 2>/dev/null; then
  echo "  already exists"
else
  az postgres flexible-server create \
    --resource-group "$GROUP" --name "$SERVER" --location "$LOCATION" \
    --admin-user "$DB_USER" --admin-password "$POSTGRES_PASSWORD" \
    --tier Burstable --sku-name Standard_B1ms --storage-size 32 --version 16 \
    --public-access 0.0.0.0 --yes --output none
fi

step "4/7 Database $DATABASE"
if az postgres flexible-server db show --resource-group "$GROUP" \
  --server-name "$SERVER" --name "$DATABASE" --output none 2>/dev/null; then
  echo "  already exists"
else
  az postgres flexible-server db create \
    --resource-group "$GROUP" --server-name "$SERVER" --name "$DATABASE" --output none
fi

step "5/7 Storage account $STORAGE"
az storage account create \
  --name "$STORAGE" --location "$LOCATION" --resource-group "$GROUP" \
  --sku Standard_LRS --allow-blob-public-access false --output none

step "6/7 Function app $APP (a few minutes the first time)"
if az functionapp show --resource-group "$GROUP" --name "$APP" --output none 2>/dev/null; then
  echo "  already exists"
else
  # 512 MB instances are the cheapest; each request is billed for at least one
  # second, so the memory size sets how fast the free grant runs out
  az functionapp create \
    --resource-group "$GROUP" --name "$APP" --storage-account "$STORAGE" \
    --flexconsumption-location "$LOCATION" \
    --runtime python --runtime-version 3.12 --instance-memory 512 --output none
fi

# One instance is plenty for a node posting every few seconds, and it keeps the
# database connections few (the pool holds up to 4 per instance). Python apps
# default to ONE request at a time per instance, and with a single instance the
# front end then makes requests wait about a minute, so allow a few at once.
az functionapp scale config set \
  --resource-group "$GROUP" --name "$APP" --maximum-instance-count 1 \
  --trigger-type http --trigger-settings perInstanceConcurrency=4 --output none

step "7/7 Pointing the app at the database"
DATABASE_URL="postgresql://${DB_USER}:${POSTGRES_PASSWORD}@${SERVER}.postgres.database.azure.com:5432/${DATABASE}?sslmode=require"
az functionapp config appsettings set \
  --resource-group "$GROUP" --name "$APP" \
  --settings "DATABASE_URL=${DATABASE_URL}" "API_KEY=${API_KEY}" \
  MPLCONFIGDIR=/tmp/matplotlib --output none

step "Done"
echo "Deploy the code with: make azure-deploy AZURE_APP=${APP}"
echo "The node's endpoint_url will be: https://$(az functionapp show --resource-group "$GROUP" --name "$APP" --query properties.defaultHostName --output tsv)/api/v1/readings"
