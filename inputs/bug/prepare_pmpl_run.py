import random
import os

def generate_random_float(start, end):
    return round(random.uniform(start, end), 2)

def generate_random_int(start, end):
    return random.randint(start, end)

def replace_placeholders(content):

    sseed = str(generate_random_int(0, 9999999))
    content = content.replace("@@seed@@", sseed)
    print( "## SEED ", sseed );
    
    start_ranges = [[-5.0, -5.0], [3.0, 3.0], [-25.0, -25.0], [0.0, 0.0], [0.0, 0.0], [0.0, 0.0]]
    goal_ranges = [[0.0, 0.0], [0.0, 0.0], [10.0, 10.0], [0.0, 0.0], [0.0, 0.0], [0.0, 0.0]]


    content = content.replace("@@start@@", generate_numbers_from_ranges(start_ranges))
    content = content.replace("@@goal@@", generate_numbers_from_ranges(goal_ranges))

    stick_bug_size = 2.0

    print( "## STICK BUG SIZE ", stick_bug_size )
    length_min = 0.0 * stick_bug_size
    length_max = 1.0 * stick_bug_size
    length_center = 0.5 * stick_bug_size

    scale = 0.7

    print( "## SCALE (coordinates) ", scale );
    content = content.replace("@@length_min@@", str(length_min))
    content = content.replace("@@length_max@@", str(length_max))
    content = content.replace("@@length_center@@", str(length_center))
    content = content.replace("@@scale@@", str(scale))


    return content

def generate_numbers_from_ranges(ranges):
    result = []
    for r in ranges:
        min_val, max_val = r
        if min_val <= max_val:
            number = generate_random_float(min_val, max_val)
            result.append(str(number))
        else:
            raise ValueError("Invalid range: min value is greater than max value")
    return ' '.join(result)
def main():

    with open("env/stick_bug_template.obj", "r") as template_file:
        template_content = template_file.read()
            
        content = replace_placeholders(template_content)
        filename = f"env/stick-bug.obj"

        with open(filename, "w") as new_file:
            new_file.write(content)


    with open("CatExperimentTemplate.xml", "r") as template_file:
        template_content = template_file.read()
            
        content = replace_placeholders(template_content)
        filename = f"CatExperiment.xml"

        with open(filename, "w") as new_file:
            new_file.write(content)

#        command = f"/home/sariel/prog/25/pmpl/build/ppl_mp -f {filename}"
#        print(command)
#        os.system(command)

if __name__ == "__main__":
    main()
