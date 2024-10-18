def bin2string(path):
    array = open(path, 'rb').read()
    string = ''
    for i, byte in enumerate(array):
        string += f"{byte},"
        if not ((i+1) % 32):
            string += '\n    '
    string = string[:-1]
    return string


template = open('bin2array.c', 'r', encoding='utf8').read()

firmware = bin2string('build/llama.bin')
# bootloader = bin2string('build/bootloader/bootloader.bin')
# partition = bin2string('build/partition_table/partition-table.bin')

template = template.replace('#firmware#', firmware)
# template = template.replace('#bootloader#', bootloader)
# template = template.replace('#partition#', partition)

open('build/llama_binaries.c', 'w', encoding='utf8').write(template)
