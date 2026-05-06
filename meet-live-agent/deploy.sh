#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Load environment variables from .env if it exists
if [ -f .env ]; then
  # Export variables ignoring comments and empty lines
  export $(grep -v '^#' .env | xargs)
fi

# Check required environment variables
if [ -z "$PROJECT_ID" ]; then
  echo "Error: PROJECT_ID is not set. Please set it in your environment or .env file."
  exit 1
fi

if [ -z "$GEMINI_API_KEY" ]; then
  echo "Error: GEMINI_API_KEY is not set. Please set it in your environment or .env file."
  exit 1
fi

if [ -z "$CLOUD_PROJECT_NUMBER" ]; then
  echo "Error: CLOUD_PROJECT_NUMBER is not set. Please set it in your environment or .env file."
  exit 1
fi

if [ -z "$CLIENT_ID" ]; then
  echo "Error: CLIENT_ID is not set. Please set it in your environment or .env file."
  exit 1
fi


# Set default region if not specified
REGION=${REGION:-us-west1}

echo "Deploying meet-live-agent to Cloud Run in project $PROJECT_ID and region $REGION..."

gcloud run deploy meet-live-agent \
  --source . \
  --region "$REGION" \
  --project "$PROJECT_ID" \
  --allow-unauthenticated \
  --min-instances 0 \
  --max-instances 3 \
  --memory 1Gi \
  --cpu 1000m \
  --set-env-vars "GEMINI_API_KEY=$GEMINI_API_KEY,CLOUD_PROJECT_NUMBER=$CLOUD_PROJECT_NUMBER,CLIENT_ID=$CLIENT_ID" \
  --clear-base-image \
  --port 3000
