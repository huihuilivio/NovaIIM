#!/bin/bash

# Deployment script for NovaIIM project (Linux)
# This script deploys the built application

DEPLOY_PATH=${1:-"./deploy"}
CONFIG=${2:-"Release"}

echo "Deploying NovaIIM project to $DEPLOY_PATH..."

# Check if build exists
if [ ! -d "../build" ]; then
    echo "Build directory not found. Please build first."
    exit 1
fi

# Create deploy directory
mkdir -p "$DEPLOY_PATH"

# Copy binaries
if [ -d "../build/output/bin" ]; then
    cp -r ../build/output/bin/* "$DEPLOY_PATH/"
fi

# Copy libraries
if [ -d "../build/output/lib" ]; then
    cp -r ../build/output/lib/* "$DEPLOY_PATH/"
fi

# Copy config files
if [ -d "configs" ]; then
    cp -r configs/* "$DEPLOY_PATH/"
fi

# Copy documentation
if [ -f "README.md" ]; then
    cp README.md "$DEPLOY_PATH/"
fi

echo "Deployment completed successfully to $DEPLOY_PATH"

# Create start script
cat > "$DEPLOY_PATH/start.sh" << 'EOF'
#!/bin/bash
# Start script for NovaIIM
echo "Starting NovaIIM server..."
# Add your server startup command here
# Example: ./server --config config.yaml
EOF

chmod +x "$DEPLOY_PATH/start.sh"

echo "Created start.sh in deploy directory."