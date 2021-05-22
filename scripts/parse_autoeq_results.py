import json
import os
import re

# Run in AutoEQ results folder
root_dir = "."
out_file_name = "autoeq_profiles.json"

staging_dict = {}

def get_numbers(s):
    rr = re.findall("[-+]?[.]?[\d]+(?:,\d\d\d)*[\.]?\d*(?:[eE][-+]?\d+)?", s)
    return [float(d) for d in rr]

for dir_name, subdir_list, file_list in os.walk(root_dir):

    print(f"Found directory: {dir_name}")
    for file_name in file_list:

        if "ParametricEQ" in file_name:
            source = dir_name.split("/")[-2]
            name = file_name.split(" ParametricEQ")[0]
            path = os.path.join(dir_name, file_name)

            with open(path, "r") as f:
                content = f.readlines()

            preamp_gain = get_numbers(content[0])[0]
            filter_fc = []
            filter_db = []
            filter_q = []
            for line in content[1:]:
                filt_numbers = get_numbers(line)
                filter_fc.append(filt_numbers[1])
                filter_db.append(filt_numbers[2])
                filter_q.append(filt_numbers[3])
                        
            if source not in staging_dict.keys():
                print(f"Adding {source} group")
                staging_dict[source] = []
            
            staging_dict[source].append(
                {
                    "label": name,
                    "value": json.dumps(
                        {
                            "filter_fc": filter_fc,
                            "filter_db": filter_db,
                            "filter_q": filter_q
                        }
                    )
                }
            )

out_array = []

# print(staging_dict)

for k, v in staging_dict.items():
    group = {
        "label": k,
        "choices": v
    }
    out_array.append(group)


with open(out_file_name, "w") as f:
    json.dump(out_array, f)