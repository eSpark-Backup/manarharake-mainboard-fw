#!/bin/bash

# Define paths for the first file
source_file1="../peripheral_uart_sample/build_1/zephyr/zephyr.hex"
destination_directory1="./NRF"

# Define paths for the second file
source_file2="..\sam4s-USART\sam4s-USART\Debug\sam4s-USART.hex"
destination_directory2="./SAM"

# Check if first source file exists
if [ ! -f "$source_file1" ]; then
    echo "Error: Source file '$source_file1' does not exist."
    read -p "Press Enter to exit..."
    exit 1
fi

# Check if first destination directory exists
if [ ! -d "$destination_directory1" ]; then
    echo "Error: Destination directory '$destination_directory1' does not exist."
    read -p "Press Enter to exit..."
    exit 1
fi

# Check if second source file exists
if [ ! -f "$source_file2" ]; then
    echo "Error: Source file '$source_file2' does not exist."
    read -p "Press Enter to exit..."
    exit 1
fi

# Check if second destination directory exists
if [ ! -d "$destination_directory2" ]; then
    echo "Error: Destination directory '$destination_directory2' does not exist."
    read -p "Press Enter to exit..."
    exit 1
fi

# Copying the first file to the first destination directory
cp "$source_file1" "$destination_directory1"

# Check if first file copy was successful
if [ $? -eq 0 ]; then
    echo "File '$source_file1' copied to '$destination_directory1' successfully."
else
    echo "Error: Failed to copy '$source_file1' to '$destination_directory1'."
fi

# Copying the second file to the second destination directory
cp "$source_file2" "$destination_directory2"

# Check if second file copy was successful
if [ $? -eq 0 ]; then
    echo "File '$source_file2' copied to '$destination_directory2' successfully."
else
    echo "Error: Failed to copy '$source_file2' to '$destination_directory2'."
fi

read -p "Press Enter to exit..."
