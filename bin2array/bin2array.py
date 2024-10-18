def bin2string(path):
    array = open(path, 'rb').read()
    string = ''
    for i, byte in enumerate(array):
        string += f"{byte},"
        if not ((i+1) % 32):
            string += '\n    '
    string = string[:-1]
    return string


template = open('bin2array.template', 'r', encoding='utf8').read()

firmware = bin2string('../build/llama.bin')
# bootloader = bin2string('../build/bootloader/bootloader.bin')
# partition = bin2string('../build/partition_table/partition-table.bin')

bin_file = template
bin_file = template.replace('#firmware#', firmware)
# bin_file = template.replace('#bootloader#', bootloader)
# bin_file = template.replace('#partition#', partition)

empty_file = template
empty_file = template.replace('#firmware#', '')
# empty_file = template.replace('#bootloader#', '')
# empty_file = template.replace('#partition#', '')

open('../build/llama.bin.c', 'w', encoding='utf8').write(bin_file)
open('../build/llama_empty.bin.c', 'w', encoding='utf8').write(empty_file)
