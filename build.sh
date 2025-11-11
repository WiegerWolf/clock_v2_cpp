#!/bin/bash                                                                                                            
                                                                                                                       
# Exit immediately if a command exits with a non-zero status.                                                          
set -e                                                                                                                 
                                                                                                                       
# Check if a .env file exists and source it                                                                            
if [ -f .env ]; then                                                                                                   
  echo "Sourcing environment variables from .env file..."                                                              
  set -a # Automatically export all variables                                                                          
  source .env                                                                                                          
  set +a # Stop automatically exporting variables                                                                      
fi                                                                                                                     
                                                                                                                       
# Check if CEREBRAS_API_KEY environment variable is set (either from .env or externally)
if [ -z "$CEREBRAS_API_KEY" ]; then
  echo "Error: CEREBRAS_API_KEY environment variable is not set."
  echo "Please set it either in a .env file or export it before running this script:"
  echo "Example .env file:"
  echo "CEREBRAS_API_KEY='your-cerebras-key'"
  echo "Or run: export CEREBRAS_API_KEY='your-cerebras-key'"
  exit 1
fi
                                                                                                                       
# Define the build directory                                                                                            
BUILD_DIR="build"                                                                                                        
                                                                                                                       
# Create the build directory if it doesn't exist                                                                       
mkdir -p "$BUILD_DIR"                                                                                                  
                                                                                                                       
# Configure the project with CMake, passing the API key                                                                
echo "Configuring project..."                                                                                          
cmake -B "$BUILD_DIR" -DCEREBRAS_API_KEY_DEFINE="$CEREBRAS_API_KEY"                                                                                                                       
# Build the project                                                                                                    
echo "Building project..."                                                                                              
cmake --build "$BUILD_DIR"                                                                                             
                                                                                                                       
echo "Build complete. The executable is in the $BUILD_DIR directory." 