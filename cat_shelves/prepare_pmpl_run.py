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
    
    # cfgs = [
    # "-0.08 -0.75 -0.2 1.0 -0.5 0.0",
    # "0.08 -0.75 -0.2 1.0 -0.5 0.0",
    # "-0.42 -0.75 -0.2 1.0 -0.5 0.0",
    # "-0.57 -0.75 -0.2 1.0 -0.5 0.0",
    # "0.42 -0.75 -0.2 1.0 -0.5 0.0",
    # "0.57 -0.75 -0.2 1.0 -0.5 0.0",
    # "-0.05 -0.85 -0.4 -0.3 0.0 0.0",
    # "-0.55 -0.85 -0.4 -0.3 0.0 0.0",
    # "0.45 -0.85 -0.4 -0.3 0.0 0.0",
    # "0.08 -0.85 -0.4 -0.3 1.0 0.0",
    # "-0.42 -0.85 -0.4 -0.3 1.0 0.0",
    # "0.58 -0.85 -0.4 -0.3 1.0 0.0"
    # ]

    cfgs = [
    "-0.08 -0.75 -0.2 1.0 -0.5 0.0",
    "0.08 -0.75 -0.2 1.0 -0.5 0.0"
    "-0.42 -0.75 -0.2 1.0 -0.5 0.0",
    "-0.58 -0.75 -0.2 1.0 -0.5 0.0",
    "0.42 -0.75 -0.2 1.0 -0.5 0.0",
    "0.58 -0.75 -0.2 1.0 -0.5 0.0",
    # "-0.08 -0.85 -0.36 -0.3 0.5 0.0",
    # "-0.58 -0.85 -0.36 -0.3 0.5 0.0",
    # "0.425 -0.85 -0.36 -0.3 0.5 0.0",
    "0.08 -0.85 -0.4 -0.3 1.0 0.0",
    "-0.42 -0.85 -0.4 -0.3 1.0 0.0",
    "0.58 -0.85 -0.4 -0.3 1.0 0.0"
    ]

    start, goal = random.sample(cfgs, 2)


    content = content.replace("@@start@@", start)
    content = content.replace("@@goal@@", goal)

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
