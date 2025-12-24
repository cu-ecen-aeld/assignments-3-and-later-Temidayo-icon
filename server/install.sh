#!/bin/bash

# Install script for aesdsocket

echo "Installing aesdsocket..."

# Compile the program
make clean
make

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Copy binary to /usr/bin
echo "Installing binary to /usr/bin..."
sudo cp aesdsocket /usr/bin/

# Copy startup script
echo "Installing startup script..."
sudo cp aesdsocket-start-stop /etc/init.d/
sudo chmod +x /etc/init.d/aesdsocket-start-stop

# Create symbolic links for runlevels
echo "Setting up runlevel links..."
if command -v update-rc.d >/dev/null 2>&1; then
    sudo update-rc.d aesdsocket-start-stop defaults
elif command -v chkconfig >/dev/null 2>&1; then
    sudo chkconfig --add aesdsocket-start-stop
else
    echo "Warning: Could not set up automatic startup"
fi

echo "Installation complete!"
echo ""
echo "Usage:"
echo "  sudo service aesdsocket-start-stop start"
echo "  sudo service aesdsocket-start-stop stop"
echo "  sudo service aesdsocket-start-stop restart"
echo "  sudo service aesdsocket-start-stop status"
