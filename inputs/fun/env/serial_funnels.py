import sys

def generate_environment(num_walls):
    environment_text = "Boundary Box [-35:35 ; -35:35 ; -35:35]\n\nMultibodies\n"
    num_objects = 0
    object_descriptions = []
    block_counter = 1

    for wall in range(num_walls):
        num_funnels = int(input(f"Enter the number of funnels in wall {wall + 1}: "))
        wall_y_coordinate = (wall - (num_walls - 1) / 2) * 7
        funnel_x_coordinates = [round((-35 + 70 * (i + 1) / (num_funnels + 1)),2) for i in range(num_funnels)]

        for funnel_x in funnel_x_coordinates:
            left_funnel_x = funnel_x - 2
            right_funnel_x = funnel_x + 2
            object_descriptions.append(f"Passive\nleft_funnel.obj {left_funnel_x} {wall_y_coordinate + 0.833} 0 0 0 0\n\n")
            object_descriptions.append(f"Passive\nright_funnel.obj {right_funnel_x} {wall_y_coordinate + 0.833} 0 0 0 0\n\n")
            num_objects += 2

        
        block_positions = [round(((-35.0 + funnel_x_coordinates[0]-3.5)/2.0),2)] + [round(((funnel_x_coordinates[i] + 3.5 + funnel_x_coordinates[i+1] - 3.5)/2.0),2) for i in range(0,len(funnel_x_coordinates)-1)] + [round(((35.0 + funnel_x_coordinates[-1]+3.5)/2.0),2)]
        block_lengths = [round(((funnel_x_coordinates[0]-3.5) + 35.0),2)] + [round(((funnel_x_coordinates[i+1] - 3.5) - (funnel_x_coordinates[i] + 3.5)),2) for i in range(0,len(funnel_x_coordinates)-1)] + [round((35.0 - (funnel_x_coordinates[-1]+3.5)),2)]
        print(block_positions)
        print(block_lengths)
        
        for i in range(len(block_positions)):
            block_length = block_lengths[i]
            object_descriptions.append(f"Passive\nblock{block_counter}.g {block_positions[i]} {wall_y_coordinate} 0 0 0 0\n\n")
            create_block_file(block_counter, block_length)
            block_counter += 1
            num_objects += 1

    environment_text += f"{num_objects}\n\n"
    # environment_text += f"{num_objects+1}\n\nActive\n1\nrobot.g Planar Rotational\nConnections\n0\n\n"
    environment_text += "".join(object_descriptions)

    with open("funnels.env", "w") as f:
        f.write(environment_text)

def create_block_file(block_number, block_length):
    block_content = f"""       1      8     12     36
      1     12
{block_length / 2} 0.5 0.5
{block_length / 2} 0.5 -0.5
{block_length / 2} -0.5 0.5
{block_length / 2} -0.5 -0.5
-{block_length / 2} 0.5 0.5
-{block_length / 2} 0.5 -0.5
-{block_length / 2} -0.5 0.5
-{block_length / 2} -0.5 -0.5
1 5 -7
1 7 -3
1 2 -6
1 6 -5
1 3 -4
1 4 -2
5 6 -8
5 8 -7
2 4 -8
2 8 -6
3 7 -8
3 8 -4"""
    
    with open(f"block{block_number}.g", "w") as f:
        f.write(block_content)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python serial_funnels.py <num_walls>")
        sys.exit(1)

    num_walls = int(sys.argv[1])
    generate_environment(num_walls)