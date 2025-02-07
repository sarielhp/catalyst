import sys

def multiply_numbers_by_factor(line, factor):
    # Split the line into parts
    parts = line.split()
    # Multiply all numerical parts by the factor
    modified_parts = [parts[0]] + [str(float(part) * factor) for part in parts[1:]]
    # Join the modified parts back into a string
    return ' '.join(modified_parts) + '\n'

def process_file(input_file_path):
    # Determine the output file name
    output_file_name = "small_" + input_file_path.split("/")[-1]

    # Open input and output files
    with open(input_file_path, 'r') as input_file, open(output_file_name, 'w') as output_file:
        # Process each line
        for line in input_file:
            if line.startswith("v") or line.startswith("vt"):
                # Multiply numbers by 0.01 for lines starting with 'v' or 'vt'
                modified_line = multiply_numbers_by_factor(line, 0.01)
                output_file.write(modified_line)
            else:
                # For other lines, simply copy them
                output_file.write(line)

if __name__ == "__main__":
    # Check if the correct number of arguments is provided
    if len(sys.argv) != 2:
        print("Usage: python script.py <input_file_path>")
        sys.exit(1)

    # Get the input file path from command-line arguments
    input_file_path = sys.argv[1]

    # Process the file
    process_file(input_file_path)

    print("File processing complete.")
