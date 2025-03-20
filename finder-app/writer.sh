#!/bin/bash

# Check if both arguments are provided

if [$# -ne 2]; then
	echo "Error: Two arguements required: <writestr>"
	exit 1
fi

# Assign arguments to variables
writefile=$1
writestr=$2

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Write the string to the file, overwriting any existing content 
echo "$writestr" > "$writefile"

# Check if the file was successfully created
if [$? -ne 0]; then
	echo "Error: Could not create or write to the file $writefile"
	exit 1
fi

#Success message (optional)
echo "File created successfully with content: $writestr"

exit 0
