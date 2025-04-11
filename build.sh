#!/bin/bash                                                                                                            
                                                                                                                       
# Exit immediately if a command exits with a non-zero status.                                                          
set -e                                                                                                                 
                                                                                                                       
# Check if LLM_API_KEY environment variable is set                                                                     
if [ -z "$LLM_API_KEY" ]; then                                                                                         
  echo "Error: LLM_API_KEY environment variable is not set."                                                           
  echo "Please set it before running this script:"                                                                     
  echo "export LLM_API_KEY='your-api-key'"                                                                             
  exit 1                                                                                                               
fi                                                                                                                     
                                                                                                                       
# Define the build directory                                                                                           
BUILD_DIR="build"                                                                                                      
                                                                                                                       
# Create the build directory if it doesn't exist                                                                       
mkdir -p "$BUILD_DIR"                                                                                                  
                                                                                                                       
# Configure the project with CMake, passing the API key                                                                
echo "Configuring project..."                                                                                          
cmake -B "$BUILD_DIR" -DLLM_API_KEY="$LLM_API_KEY"                                                                     
                                                                                                                       
# Build the project                                                                                                    
echo "Building project..."                                                                                             
cmake --build "$BUILD_DIR"                                                                                             
                                                                                                                       
echo "Build complete. The executable is in the $BUILD_DIR directory." 