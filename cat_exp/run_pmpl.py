import random
import os

def generate_random_float(start, end):
    return round(random.uniform(start, end), 2)

def generate_random_int(start, end):
    return random.randint(start, end)

def replace_placeholders(content):
    content = content.replace("@@seed@@", str(generate_random_int(0, 9999999)))

    return content

def main():
    with open("CatExperimentTemplate.xml", "r") as template_file:
        template_content = template_file.read()
            

        content = replace_placeholders(template_content)
        filename = f"CatExperiment.xml"

        with open(filename, "w") as new_file:
            new_file.write(content)

        command = f"/home/sariel/prog/25/pmpl/build/ppl_mp -f {filename}"
        print(command)
        os.system(command)

if __name__ == "__main__":
    main()
