import random
import os

def generate_random_float(start, end):
    return round(random.uniform(start, end), 2)

def generate_random_int(start, end):
    return random.randint(start, end)

def replace_placeholders(content):

    sseed = str(generate_random_int(0, 9999999))
    content = content.replace("@@seed@@", sseed)
    print( "## SEED ", sseed )

    start_ranges = [[-33.0, 33.0], [30.0, 33.0], [-1.0, 1.0]]
    goal_ranges = [[-33.0, 33.0], [-33.0, -30.0], [-1.0, 1.0]]

    content = content.replace("@@start@@", generate_numbers_from_ranges(start_ranges))
    content = content.replace("@@goal@@", generate_numbers_from_ranges(goal_ranges))

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
