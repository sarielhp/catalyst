import sys
import math

def compute_path_length(file_path):
    with open(file_path, 'r') as file:
        lines = file.readlines()
    
    # Extract the number of points
    n = int(lines[2].strip())
    
    # Initialize lists to store x and y coordinates
    x_coords = []
    y_coords = []
    
    # Extract the coordinates from the file
    for i in range(3, 3 + n):
        parts = lines[i].strip().split()
        x_coords.append(float(parts[1]))
        y_coords.append(float(parts[2]))
    
    # Calculate the path length
    path_length = 0.0
    for i in range(1, n):
        dx = x_coords[i] - x_coords[i - 1]
        dy = y_coords[i] - y_coords[i - 1]
        path_length += math.sqrt(dx**2 + dy**2)
    
    # Print the path length
    print(f"# {path_length}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <path_to_file>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    
    compute_path_length(file_path)