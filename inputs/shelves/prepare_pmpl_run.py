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

    content = content.replace("@@start@@", "0.5 -0.5 0.0 -0.5 0.0 0.0")
    content = content.replace("@@goal@@", "-0.06 -0.75 -0.3 -0.45 -1.0 0.0")
    # content = content.replace("@@goal@@", "0.55 -0.73 -0.25 -0.5 1.0 0.0")
    

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
